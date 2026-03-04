/*-
 * Copyright (c) 2026 Davide Beatrici
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_endian.h"
#include "archive_entry.h"
#include "archive_entry_locale.h"
#include "archive_private.h"
#include "archive_read_private.h"

#define MAX_NFILES		100000
#define STR_SIZE_LIMIT	(1024 * 1024)	/* 1 MiB */

#define LEAD_MAGIC		"\xed\xab\xee\xdb"
#define HEADER_MAGIC	"\x8e\xad\xe8\x01\x00\x00\x00\x00"

#define CPIO_END_MARK		"TRAILER!!!"
#define CPIO_END_MARK_SIZE	10

#define CPIO_HEADER_STR_SIZE		16
#define CPIO_HEADER_SVR_SIZE		110
#define CPIO_HEADER_SVR_PATH_OFF	94

#define CPIO_MAGIC_SIZE			6
#define CPIO_MAGIC_STR			"07070X"
#define CPIO_MAGIC_SVR4_CRC		"070702"
#define CPIO_MAGIC_SVR4_NOCRC	"070701"

#define TAG_FILESIZES		1028
#define TAG_FILEMODES		1030
#define TAG_FILERDEVS		1033
#define TAG_FILEMTIMES		1034
#define TAG_FILEUSERNAMES	1039
#define TAG_FILEGROUPNAMES	1040
#define TAG_FILEDEVICES		1095
#define TAG_FILEINODES		1096

#define TAG_DIRINDEXES	1116
#define TAG_BASENAMES	1117
#define TAG_DIRNAMES	1118

#define TAG_PAYLOADCOMPRESSOR	1125

#define TAG_LONGFILESIZES	5008

enum rpm_cpio_format {
	CPIO_UNKNOWN,
	CPIO_STR,
	CPIO_SVR4_CRC,
	CPIO_SVR4_NOCRC
};

struct rpm_file_info {
	char		*pathname;
	char		*uname;
	char		*gname;
	uint64_t	size;
	uint16_t	mode;
	int32_t		dev;
	int16_t		rdev;
	uint32_t	mtime;
	uint32_t	ino;
};

struct rpm_inode_info {
	uint32_t				n_files;
	struct rpm_file_info	**files;
	uint32_t				n_processed;
};

struct rpm_inode_temp_entry {
	uint64_t ino;
	struct rpm_file_info **files;
	uint32_t n_files;
	size_t capacity;
	struct rpm_inode_temp_entry *next;
};

struct rpm_lead {
    unsigned char	magic[4];
    unsigned char	major, minor;
    short			type;
    short			archnum;
    char			name[66];
    short			osnum;
    short			signature_type;
    char			reserved[16];
};

struct rpm_header {
	unsigned char	magic[8];
	uint32_t		n_entries;
	uint32_t		data_size;
};

struct rpm_entry {
	uint32_t	tag;
	uint32_t	type;
	int32_t		offset;
	uint32_t	count;
};

struct rpm {
	enum {
		ST_LEAD,
		ST_HEADER,
		ST_PADDING,
		ST_ARCHIVE
	}	state;
	uint8_t	first_header;

	char	*compressor;

	uint32_t				n_files;
	uint32_t				n_inodes;
	struct rpm_file_info	*files;
	struct rpm_inode_info	*inodes;

	struct inode_hash_entry {
		uint64_t ino;
		struct rpm_inode_info *info;
		struct inode_hash_entry *next;
	}	**inode_hash;

	size_t	inode_hash_size;

	ssize_t	 entry_bytes_remaining;
	ssize_t	 entry_bytes_unconsumed;
	ssize_t	 entry_offset;
	ssize_t	 entry_padding;
};

static int archive_read_format_rpm_bid(struct archive_read *, int);
static int archive_read_format_rpm_read_header(struct archive_read *,
	struct archive_entry *);
static int archive_read_format_rpm_read_data(struct archive_read *,
	const void **, size_t *, int64_t *);
static int archive_read_format_rpm_read_data_skip(struct archive_read *);
static int archive_read_format_rpm_cleanup(struct archive_read *);

static int rpm_add_entry(struct archive_read *, struct archive_entry *);
static int	rpm_parse_main_header(struct archive_read *,
	const struct rpm_header *header);

static struct rpm_inode_info *rpm_get_inode(struct archive_read *, uint32_t);
static enum rpm_cpio_format rpm_get_cpio_format(struct archive_read *);
static uint8_t rpm_is_eof(struct archive_read *);

static uint16_t	rpm_be16_at(const void *buf_start,
	const void *buf_end, const size_t off);
static uint32_t	rpm_be32_at(const void *buf_start,
	const void *buf_end, const size_t off);
static uint64_t	rpm_be64_at(const void *buf_start,
	const void *buf_end, const size_t off);

static inline size_t	rpm_limit_bytes(size_t, size_t);
static char	*rpm_strndup(struct archive_read *, const char *, size_t);
static char	*rpm_strread(struct archive_read *, size_t);
static void	rpm_strcat(struct archive_string *, const void *, const void *);
static const char	*rpm_strlist_at(const char *, const void *,
	uint64_t, uint64_t);

static void	rpm_free_inode_temp_hash(struct rpm_inode_temp_entry **, size_t);

int
archive_read_support_format_rpm(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct rpm *rpm;
	int r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_format_rpm");

	rpm = calloc(1, sizeof(*rpm));
	if (!rpm) {
		archive_set_error(&a->archive,
			ENOMEM,
			"Can't allocate rpm data");
		return ARCHIVE_FATAL;
	}

	r = __archive_read_register_format(a,
	    rpm,
	    "rpm",
	    archive_read_format_rpm_bid,
	    NULL,
	    archive_read_format_rpm_read_header,
	    archive_read_format_rpm_read_data,
	    archive_read_format_rpm_read_data_skip,
	    NULL,
	    archive_read_format_rpm_cleanup,
	    NULL,
	    NULL);

	if (r != ARCHIVE_OK)
		free(rpm);

	return ARCHIVE_OK;
}

static int
archive_read_format_rpm_bid(struct archive_read *a, int best_bid)
{
	const unsigned char *p;

	(void)best_bid;

	if ((p = __archive_read_ahead(a, sizeof(LEAD_MAGIC) - 1, NULL)) == NULL)
		return -1;

	if (memcmp(p, LEAD_MAGIC, sizeof(LEAD_MAGIC) - 1) == 0)
		return 48;
	else
		return ARCHIVE_WARN;
}

static int
archive_read_format_rpm_read_header(struct archive_read *a,
    struct archive_entry *entry)
{
	struct rpm *rpm = a->format->data;
	const unsigned char *p;
	int r;

	a->archive.archive_format = ARCHIVE_FORMAT_RPM;

	for (;;) {
		switch (rpm->state) {
		case ST_LEAD: {
			const struct rpm_lead *lead = __archive_read_ahead(a, sizeof(*lead), NULL);
			if (lead == NULL) {
				archive_set_error(&a->archive,
					ARCHIVE_ERRNO_FILE_FORMAT,
					"Truncated lead");
				return ARCHIVE_FATAL;
			}

			if (memcmp(lead->magic, LEAD_MAGIC, sizeof(lead->magic)) != 0) {
				archive_set_error(&a->archive,
					ARCHIVE_ERRNO_FILE_FORMAT,
					"Unrecognized lead");
				return ARCHIVE_FATAL;
			}

			__archive_read_consume(a, sizeof(*lead));

			rpm->state = ST_HEADER;
			rpm->first_header = 1;
			break;
		}
		case ST_HEADER: {
			struct rpm_header header;

			p = __archive_read_ahead(a, sizeof(header), NULL);
			if (p == NULL) {
				archive_set_error(&a->archive,
					ARCHIVE_ERRNO_FILE_FORMAT,
					"Truncated header");
				return ARCHIVE_FATAL;
			}

			memcpy(&header, p, sizeof(header));

			if (memcmp(header.magic, HEADER_MAGIC, sizeof(header.magic)) != 0) {
				archive_set_error(&a->archive,
					ARCHIVE_ERRNO_FILE_FORMAT,
					"Unrecognized header");
				return ARCHIVE_FATAL;
			}

			header.n_entries = archive_be32dec(&header.n_entries);
			header.data_size = archive_be32dec(&header.data_size);

			__archive_read_consume(a, sizeof(header));

			r = rpm_parse_main_header(a, &header);
			if (r != ARCHIVE_OK)
				return r;

			rpm->state = ST_PADDING;
			break;
		}
		case ST_PADDING: {
			for (;;) {
				p = __archive_read_ahead(a, 1, NULL);
				if (p == NULL) {
					archive_set_error(&a->archive,
						ARCHIVE_ERRNO_FILE_FORMAT,
						"Truncated padding");
					return ARCHIVE_FATAL;
				}

				if (*p != 0)
					break;

				__archive_read_consume(a, 1);
			}

			if (rpm->first_header) {
				rpm->first_header = 0;
				rpm->state = ST_HEADER;
			} else {
				enum rpm_cpio_format cpio_format;

				if (!rpm->compressor) {
					/* No compression. */
				} else if (strcmp(rpm->compressor, "zstd") == 0) {
					r = archive_read_append_filter(&a->archive, ARCHIVE_FILTER_ZSTD);
				} else if (strcmp(rpm->compressor, "xz") == 0) {
					archive_read_append_filter(&a->archive, ARCHIVE_FILTER_XZ);
				} else if (strcmp(rpm->compressor, "gzip") == 0) {
					archive_read_append_filter(&a->archive, ARCHIVE_FILTER_GZIP);
				} else if (strcmp(rpm->compressor, "bzip2") == 0) {
					archive_read_append_filter(&a->archive, ARCHIVE_FILTER_BZIP2);
				} else if (strcmp(rpm->compressor, "lzma") == 0) {
					archive_read_append_filter(&a->archive, ARCHIVE_FILTER_LZMA);
				} else {
					archive_set_error(&a->archive,
						ARCHIVE_ERRNO_FILE_FORMAT,
						"Unrecognized compressor: %s", rpm->compressor);
					return ARCHIVE_FATAL;
				}

				if (r != ARCHIVE_OK) {
					archive_set_error(&a->archive, r, "Cannot append %s filter",
						rpm->compressor);
					return r;
				}

				cpio_format = rpm_get_cpio_format(a);
				switch (cpio_format) {
				case CPIO_STR:
					a->archive.archive_format_name = "RPM (stripped CPIO)";
					break;
				case CPIO_SVR4_CRC:
					a->archive.archive_format_name = "RPM (SVR4 cpio with CRC)";
					break;
				case CPIO_SVR4_NOCRC:
					a->archive.archive_format_name = "RPM (SVR4 cpio with no CRC)";
					break;
				case CPIO_UNKNOWN:
				default:
					return ARCHIVE_FATAL;
				}

				rpm->state = ST_ARCHIVE;
			}
			break;
		}
		case ST_ARCHIVE:
			return rpm_add_entry(a, entry);
		}
	}
}

static int
archive_read_format_rpm_read_data(struct archive_read *a,
    const void **buff, size_t *size, int64_t *offset)
{
	struct rpm *rpm = a->format->data;
	ssize_t bytes_read;

	if (rpm->entry_bytes_unconsumed > 0) {
		__archive_read_consume(a, rpm->entry_bytes_unconsumed);
		rpm->entry_bytes_unconsumed = 0;
	}

	if (rpm->entry_bytes_remaining > 0) {
		*buff = __archive_read_ahead(a, 1, &bytes_read);
		if (bytes_read <= 0)
			return ARCHIVE_FATAL;

		if (bytes_read > rpm->entry_bytes_remaining)
			bytes_read = (ssize_t)rpm->entry_bytes_remaining;

		*size = bytes_read;
		rpm->entry_bytes_unconsumed = bytes_read;
		*offset = rpm->entry_offset;
		rpm->entry_offset += bytes_read;
		rpm->entry_bytes_remaining -= bytes_read;

		return ARCHIVE_OK;
	} else {
		if (rpm->entry_padding !=
			__archive_read_consume(a, rpm->entry_padding)) {
			return ARCHIVE_FATAL;
		}

		rpm->entry_padding = 0;
		*buff = NULL;
		*size = 0;
		*offset = rpm->entry_offset;

		return ARCHIVE_EOF;
	}
}

static int
archive_read_format_rpm_read_data_skip(struct archive_read *a)
{
	struct rpm *rpm = a->format->data;
	const ssize_t to_skip = rpm->entry_bytes_remaining + rpm->entry_padding
		+ rpm->entry_bytes_unconsumed;

	if (to_skip != __archive_read_consume(a, to_skip))
		return ARCHIVE_FATAL;

	rpm->entry_bytes_remaining = 0;
	rpm->entry_padding = 0;
	rpm->entry_bytes_unconsumed = 0;

	return ARCHIVE_OK;
}

static int
archive_read_format_rpm_cleanup(struct archive_read *a)
{
	struct rpm *rpm = a->format->data;
	size_t i;

	free(rpm->compressor);

	if (rpm->inode_hash) {
		for (i = 0; i < rpm->inode_hash_size; ++i) {
			free(rpm->inode_hash[i]);
		}

		free(rpm->inode_hash);
	}

	if (rpm->inodes != NULL) {
		for (i = 0; i < rpm->n_inodes; i++) {
			free(rpm->inodes[i].files);
		}

		free(rpm->inodes);
	}

	if (rpm->files != NULL) {
		for (i = 0; i < rpm->n_files; i++) {
			free(rpm->files[i].pathname);
			free(rpm->files[i].uname);
			free(rpm->files[i].gname);
		}

		free(rpm->files);
	}

	free(rpm);

	a->format->data = NULL;

	return ARCHIVE_OK;
}

static int
rpm_add_entry(struct archive_read *a, struct archive_entry *entry)
{
	struct rpm *rpm = a->format->data;
	struct rpm_file_info *file = NULL;
	struct rpm_inode_info *inode;
	uint64_t idx_ino;
	const void *p;

	const enum rpm_cpio_format cpio_format = rpm_get_cpio_format(a);

	p = __archive_read_ahead(a, CPIO_HEADER_STR_SIZE, NULL);
	if (!p) {
		archive_set_error(&a->archive,
			ARCHIVE_ERRNO_FILE_FORMAT,
			"Premature EOF");
		return ARCHIVE_FATAL;
	}

	char hex_str[9] = {0};
	memcpy(hex_str, p + CPIO_MAGIC_SIZE, sizeof(idx_ino));
	idx_ino = strtoull(hex_str, NULL, 16);

	switch (cpio_format) {
	case CPIO_STR:
		__archive_read_consume(a, CPIO_HEADER_STR_SIZE);

		if (rpm_is_eof(a))
			return ARCHIVE_EOF;

		if (idx_ino >= rpm->n_files) {
			archive_set_error(&a->archive,
				ARCHIVE_ERRNO_FILE_FORMAT,
				"File index %" PRIu64 " out of range (max %u)", idx_ino, rpm->n_files - 1);
			return ARCHIVE_FATAL;
		}

		file = &rpm->files[idx_ino];
		inode = rpm_get_inode(a, file->ino);
		if (!inode)
			return ARCHIVE_FATAL;

		break;
	case CPIO_SVR4_CRC:
	case CPIO_SVR4_NOCRC: {
		size_t i;
		uint64_t size;
		char *path;

		p = __archive_read_ahead(a, CPIO_HEADER_SVR_SIZE, NULL);
		if (!p) {
			archive_set_error(&a->archive,
				ARCHIVE_ERRNO_FILE_FORMAT,
				"Truncated SVR4 header");
			return ARCHIVE_FATAL;
		}

		memcpy(hex_str, p + CPIO_HEADER_SVR_PATH_OFF, sizeof(size));

		__archive_read_consume(a, CPIO_HEADER_SVR_SIZE);

		if (rpm_is_eof(a))
			return ARCHIVE_EOF;

		inode = rpm_get_inode(a, idx_ino);
		if (!inode)
			return ARCHIVE_FATAL;

		size = strtoull(hex_str, NULL, 16);

		path = rpm_strread(a, size);
		if (!path)
			return ARCHIVE_FATAL;

		for (i = 0; i < inode->n_files; ++i) {
			if (strcmp(inode->files[i]->pathname, path) == 0) {
				file = inode->files[i];
				break;
			}
		}

		free(path);

		if (!file) {
			archive_set_error(&a->archive,
				ARCHIVE_ERRNO_FILE_FORMAT,
				"Path not found");
			return ARCHIVE_FATAL;
		}

		/* Pad name to 2 more than a multiple of 4. */
		size += (2 - size) & 3;
		__archive_read_consume(a, size);

		break;
	}
	default:
		return ARCHIVE_FATAL;
	}

	archive_entry_set_pathname_utf8(entry, file->pathname);
	archive_entry_set_uname_utf8(entry, file->uname);
	archive_entry_set_gname_utf8(entry, file->gname);
	archive_entry_set_dev(entry, file->dev);
	archive_entry_set_ino(entry, file->ino);
	archive_entry_set_mode(entry, file->mode);
	archive_entry_set_rdev(entry, file->rdev);
	archive_entry_set_mtime(entry, file->mtime, 0);
	archive_entry_set_nlink(entry, inode->n_files);

	/* Hardlink: only last entry carries payload */
	if (++inode->n_processed == inode->n_files)
		rpm->entry_bytes_remaining = file->size;
	else
		rpm->entry_bytes_remaining = 0;

	/* Pad file contents to a multiple of 4. */
	rpm->entry_padding = 3 & -rpm->entry_bytes_remaining;
	rpm->entry_offset = 0;
	rpm->entry_bytes_unconsumed = 0;

	if (S_ISLNK(file->mode)) {
		char *target = rpm_strread(a, file->size);
		if (!target)
			return ARCHIVE_FATAL;

		__archive_read_consume(a, rpm->entry_bytes_remaining);
		rpm->entry_bytes_remaining = 0;

		archive_entry_set_symlink_utf8(entry, target);

		free(target);
	}

	archive_entry_set_size(entry, rpm->entry_bytes_remaining);

	return ARCHIVE_OK;
}

static int
rpm_parse_main_header(struct archive_read *a, const struct rpm_header *header)
{
	struct rpm *rpm = a->format->data;

	struct rpm_header_parse {
		uint32_t	 n_files;

		const char	 *basenames;
		uint32_t	 n_basenames;

		const char	 *dirnames;
		uint32_t	 n_dirnames;

		const char	 *usernames;
		uint32_t	 n_usernames;

		const char	 *groupnames;
		uint32_t	 n_groupnames;

		union {
			const int64_t	*filesizes64;
			const int32_t	*filesizes32;
		};
		uint8_t	is_filesizes64;

		const int32_t	*dirindexes;
		const int16_t	*filemodes;
		const int32_t	*filedevices;
		const int16_t	*filerdevs;
		const int32_t	*filemtimes;
		const int32_t	*fileinodes;
	} hp;

	const struct rpm_entry *entries;
	struct rpm_inode_temp_entry **temp_hash;
	size_t hlen, temp_hash_size = 1;
	uint64_t i, ino = 0;

	memset(&hp, 0, sizeof(hp));

	hlen = sizeof(struct rpm_entry)
		 * (size_t)header->n_entries
		 + (size_t)header->data_size;

	entries = __archive_read_ahead(a, hlen, NULL);
	if (entries == NULL)
		return ARCHIVE_EOF;

	if (rpm->first_header) {
		__archive_read_consume(a, hlen);
		return ARCHIVE_OK;
	}

	for (i = 0; i < header->n_entries; i++) {
		uint32_t tag, cnt;
		int32_t off;
		const void *p;

		tag = archive_be32dec(&entries[i].tag);
		off = archive_be32dec(&entries[i].offset);
		cnt = archive_be32dec(&entries[i].count);

		if (off < 0 || (uint32_t)off >= header->data_size)
			continue;

		p = (const uint8_t *)&entries[header->n_entries] + off;

		switch (tag) {
		case TAG_PAYLOADCOMPRESSOR:
			rpm->compressor = rpm_strndup(a, p ? p : "", 0);
			break;
		case TAG_BASENAMES:
			hp.basenames = p;
			hp.n_basenames = cnt;
			break;
		case TAG_DIRNAMES:
			hp.dirnames = p;
			hp.n_dirnames = cnt;
			break;
		case TAG_FILEUSERNAMES:
			hp.usernames = p;
			hp.n_usernames = cnt;
			break;
		case TAG_FILEGROUPNAMES:
			hp.groupnames = p;
			hp.n_groupnames = cnt;
			break;
		case TAG_LONGFILESIZES:
			hp.filesizes64 = p;
			hp.n_files = cnt;
			hp.is_filesizes64 = 1;
			break;
		case TAG_FILESIZES:
			/* This tag should never appear when Longfilesizes is present,
			 * but checking doesn't hurt. */
			if (!hp.is_filesizes64) {
				hp.filesizes32 = p;
				hp.n_files = cnt;
			}
			break;
		case TAG_DIRINDEXES:
			hp.dirindexes = p;
			break;
		case TAG_FILEINODES:
			hp.fileinodes = p;
			break;
		case TAG_FILEMODES:
			hp.filemodes = p;
			break;
		case TAG_FILEDEVICES:
			hp.filedevices = p;
			break;
		case TAG_FILERDEVS:
			hp.filerdevs = p;
			break;
		case TAG_FILEMTIMES:
			hp.filemtimes = p;
			break;
		}
	}

	if (hp.n_files >= MAX_NFILES) {
		archive_set_error(&a->archive,
			ARCHIVE_ERRNO_FILE_FORMAT,
			"n_files out of range");
		return ARCHIVE_FATAL;
	}

	rpm->files = calloc(hp.n_files, sizeof(*rpm->files));
	if (rpm->files == NULL) {
		archive_set_error(&a->archive,
			ENOMEM,
			"Can't allocate files data");
		return ARCHIVE_FATAL;
	}

	rpm->n_files = hp.n_files;

	while (temp_hash_size < hp.n_files * 2)
		temp_hash_size <<= 1;

	temp_hash = calloc(temp_hash_size, sizeof(*temp_hash));
	if (!temp_hash) {
		archive_set_error(&a->archive,
			ENOMEM,
			"Can't allocate temp hash");
		return ARCHIVE_FATAL;
	}

	for (i = 0; i < hp.n_files; i++) {
		const void *hbuf_end = (const uint8_t *)entries + hlen;
		struct rpm_file_info *file = &rpm->files[i];
		struct rpm_inode_temp_entry *group;
		struct archive_string as;
		const char *dname, *bname, *uname, *gname;
		size_t bucket;

		if (hp.dirindexes != NULL) {
			const uint32_t diri = rpm_be32_at(hp.dirindexes, hbuf_end, i);

			if (diri >= hp.n_dirnames) {
				rpm_free_inode_temp_hash(temp_hash, temp_hash_size);
				archive_set_error(&a->archive,
					ARCHIVE_ERRNO_FILE_FORMAT,
					"dirindex out of range");
				return ARCHIVE_FATAL;
			}

			dname = rpm_strlist_at(hp.dirnames, hbuf_end, diri, hp.n_dirnames);
		} else
			dname = NULL;

		bname = rpm_strlist_at(hp.basenames, hbuf_end, i, hp.n_basenames);
		uname = rpm_strlist_at(hp.usernames, hbuf_end, i, hp.n_usernames);
		gname = rpm_strlist_at(hp.groupnames, hbuf_end, i, hp.n_groupnames);

		archive_string_init(&as);
		archive_strappend_char(&as, '.');
		rpm_strcat(&as, dname, hbuf_end);
		rpm_strcat(&as, bname, hbuf_end);
		file->pathname = strdup(as.s);
		archive_string_free(&as);

		file->uname = rpm_strndup(a, uname ? uname : "", 0);
		file->gname = rpm_strndup(a, gname ? gname : "", 0);

		if (hp.is_filesizes64)
			file->size = rpm_be64_at(hp.filesizes64, hbuf_end, i);
		else
			file->size = rpm_be32_at(hp.filesizes32, hbuf_end, i);
		file->mode = rpm_be16_at(hp.filemodes, hbuf_end, i);
		file->dev = rpm_be32_at(hp.filedevices, hbuf_end, i);
		file->rdev = rpm_be16_at(hp.filerdevs, hbuf_end, i);
		file->mtime = rpm_be32_at(hp.filemtimes, hbuf_end, i);
		file->ino = rpm_be32_at(hp.fileinodes, hbuf_end, i);

		bucket = file->ino & (temp_hash_size - 1);
		group = temp_hash[bucket];

		while (group) {
			if (group->ino == file->ino)
				break;

			group = group->next;
		}

		if (!group) {
			group = calloc(1, sizeof(*group));
			if (!group) {
				rpm_free_inode_temp_hash(temp_hash, temp_hash_size);
				archive_set_error(&a->archive,
					ENOMEM,
					"Can't allocate inode group");
				return ARCHIVE_FATAL;
			}

			group->ino = file->ino;
			group->files = NULL;
			group->n_files = 0;
			group->capacity = 0;

			group->next = temp_hash[bucket];
			temp_hash[bucket] = group;
		}

		if (group->n_files >= group->capacity) {
			group->capacity = group->capacity ? group->capacity * 2 : 8;
			void *prev_ptr = group->files;

			group->files = realloc(group->files, group->capacity * sizeof(*group->files));
			if (!group->files) {
				free(prev_ptr);
				rpm_free_inode_temp_hash(temp_hash, temp_hash_size);
				archive_set_error(&a->archive,
					ENOMEM,
					"Can't grow inode file list");
				return ARCHIVE_FATAL;
			}
		}

		group->files[group->n_files++] = file;
	}

	rpm->n_inodes = 0;
	for (i = 0; i < temp_hash_size; i++) {
		for (struct rpm_inode_temp_entry *e = temp_hash[i]; e; e = e->next)
			rpm->n_inodes++;
	}

	rpm->inodes = calloc(rpm->n_inodes, sizeof(*rpm->inodes));
	if (!rpm->inodes) {
		rpm_free_inode_temp_hash(temp_hash, temp_hash_size);
		archive_set_error(&a->archive,
			ENOMEM,
			"Can't allocate inodes array");
		return ARCHIVE_FATAL;
	}

	for (i = 0; i < temp_hash_size; i++) {
		struct rpm_inode_temp_entry *e = temp_hash[i];
		while (e) {
			struct rpm_inode_info *info = &rpm->inodes[ino++];
			info->n_files = e->n_files;
			info->files = e->files;
			info->n_processed = 0;

			struct rpm_inode_temp_entry *next = e->next;
			free(e);
			e = next;
		}
	}

	free(temp_hash);

	rpm->inode_hash_size = 1;
	while (rpm->inode_hash_size < rpm->n_inodes * 2)
		rpm->inode_hash_size <<= 1;

	rpm->inode_hash = calloc(rpm->inode_hash_size, sizeof(*rpm->inode_hash));
	if (!rpm->inode_hash) {
		archive_set_error(&a->archive,
			ENOMEM,
			"Can't allocate inode hash");
		return ARCHIVE_FATAL;
	}

	for (i = 0; i < rpm->n_inodes; i++) {
		uint64_t raw = rpm->inodes[i].files[0]->ino;
		size_t bucket = raw & (rpm->inode_hash_size - 1);

		struct inode_hash_entry *e = malloc(sizeof(*e));
		if (!e) {
			archive_set_error(&a->archive,
				ENOMEM,
				"Can't allocate inode hash entry");
			return ARCHIVE_FATAL;
		}

		e->ino = raw;
		e->info = &rpm->inodes[i];
		e->next = rpm->inode_hash[bucket];
		rpm->inode_hash[bucket] = e;
	}

	__archive_read_consume(a, hlen);

	return ARCHIVE_OK;
}

static struct rpm_inode_info *
rpm_get_inode(struct archive_read *a, const uint32_t ino)
{
	const struct rpm *rpm = a->format->data;
	struct inode_hash_entry *e;
	size_t bucket;

	if (!rpm->inode_hash)
		return NULL;

	bucket = ino & (rpm->inode_hash_size - 1);

	for (e = rpm->inode_hash[bucket]; e; e = e->next)
		if (e->ino == ino)
			return e->info;

	return NULL;
}

static enum rpm_cpio_format
rpm_get_cpio_format(struct archive_read *a)
{
	const char *magic = __archive_read_ahead(a, CPIO_MAGIC_SIZE, NULL);
	if (!magic) {
		archive_set_error(&a->archive,
			ARCHIVE_ERRNO_FILE_FORMAT,
			"Premature EOF");
		return ARCHIVE_FATAL;
	}

	if (memcmp(magic, CPIO_MAGIC_STR, CPIO_MAGIC_SIZE) == 0)
		return CPIO_STR;
	else if (memcmp(magic, CPIO_MAGIC_SVR4_CRC, CPIO_MAGIC_SIZE) == 0)
		return CPIO_SVR4_CRC;
	else if (memcmp(magic, CPIO_MAGIC_SVR4_NOCRC, CPIO_MAGIC_SIZE) == 0)
		return CPIO_SVR4_NOCRC;
	else {
		archive_set_error(&a->archive,
			ARCHIVE_ERRNO_FILE_FORMAT,
			"Unrecognized magic");
		return CPIO_UNKNOWN;
	}
}

static uint8_t
rpm_is_eof(struct archive_read *a)
{
	const void *p = __archive_read_ahead(a, CPIO_END_MARK_SIZE, NULL);
	if (!p)
		return 1;

	return memcmp(p, CPIO_END_MARK, CPIO_END_MARK_SIZE) == 0;
}

static uint16_t
rpm_be16_at(const void *buf_start, const void *buf_end, size_t off)
{
	off *= sizeof(uint16_t);

	if (!buf_start || (buf_end <= buf_start) ||
		(size_t)(buf_end - buf_start) < (off + sizeof(uint16_t)))
		return 0;

	return archive_be16dec(buf_start + off);
}

static uint32_t
rpm_be32_at(const void *buf_start, const void *buf_end, size_t off)
{
	off *= sizeof(uint32_t);

	if (!buf_start || (buf_end <= buf_start) ||
		(size_t)(buf_end - buf_start) < (off + sizeof(uint32_t)))
		return 0;

	return archive_be32dec(buf_start + off);
}

static uint64_t
rpm_be64_at(const void *buf_start, const void *buf_end, size_t off)
{
	off *= sizeof(uint64_t);

	if (!buf_start || (buf_end <= buf_start) ||
		(size_t)(buf_end - buf_start) < (off + sizeof(uint64_t)))
		return 0;

	return archive_be64dec(buf_start + off);
}

static inline size_t
rpm_limit_bytes(const size_t bytes, const size_t max)
{
	return (bytes > max ? max : bytes);
}

static char *
rpm_strndup(struct archive_read *a, const char *s, size_t len)
{
	if (s == NULL)
		return NULL;

	if (len == 0)
		len = strnlen(s, STR_SIZE_LIMIT);

	if (len > STR_SIZE_LIMIT) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
			"String too long (malformed?)");
		return NULL;
	}

	return strndup(s, len);
}

static char *
rpm_strread(struct archive_read *a, size_t len) {
	const void *p;

	if (len > STR_SIZE_LIMIT) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
			"String too long");
		return NULL;
	}

	p = __archive_read_ahead(a, len, NULL);
	if (!p) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
			"Truncated string");
		return NULL;
	}

	return rpm_strndup(a, p, len);
}

static void
rpm_strcat(struct archive_string *a, const void *s, const void *buf_end)
{
	if (!s || s >= buf_end)
		return;

	archive_strncat(a, s, rpm_limit_bytes(buf_end - s, STR_SIZE_LIMIT));
}

static const char *
rpm_strlist_at(const char *p, const void *end, uint64_t i, uint64_t n)
{
	uint64_t k;

	if (p == NULL || i >= n || (const char *)end < p)
		return NULL;

	for (k = 0; k < n; k++) {
		const uint64_t max_len = rpm_limit_bytes((const char *)end - p,
			STR_SIZE_LIMIT);
		const uint64_t len = strnlen(p, max_len);
		if (len == max_len)
			/* Unterminated or absurdly long string. */
			return NULL;

		if (k == i)
			return p;

		p += len + 1;

		if (p >= (const char *)end)
			return NULL;
	}

	return NULL;
}

static void
rpm_free_inode_temp_hash(struct rpm_inode_temp_entry **hash, const size_t hash_size)
{
	for (size_t i = 0; i < hash_size; i++) {
		struct rpm_inode_temp_entry *e = hash[i];
		while (e) {
			struct rpm_inode_temp_entry *next = e->next;
			free(e->files);
			free(e);
			e = next;
		}
	}

	free(hash);
}
