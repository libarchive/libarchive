/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
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

#include "archive.h"
#include "archive_endian.h"
#include "archive_private.h"
#include "archive_read_private.h"

#define MAX_NFILES	100000
#define STR_SIZE_LIMIT	(1024 * 1024) /* 1 MiB */

#define RPM_LEAD_MAGIC		"\xed\xab\xee\xdb"
#define RPM_HEADER_MAGIC	"\x8e\xad\xe8\x01\x00\x00\x00\x00"

#define RPM_LEAD_SIZE		96
#define RPM_MIN_HEAD_SIZE	16

#define RPMTAG_FILESIZES		1028
#define RPMTAG_FILEMODES		1030
#define RPMTAG_FILERDEVS		1033
#define RPMTAG_FILEMTIMES		1034
#define RPMTAG_FILEUSERNAMES	1039
#define RPMTAG_FILEGROUPNAMES	1040
#define RPMTAG_FILEDEVICES      1095
#define RPMTAG_FILEINODES		1096

#define RPMTAG_DIRINDEXES	1116
#define RPMTAG_BASENAMES	1117
#define RPMTAG_DIRNAMES		1118

#define RPMTAG_LONGFILESIZES	5008

struct rpm_header_parse {
	uint64_t	 n_files;

	const char	 *basenames;
	uint64_t	 n_basenames;

	const char	 *dirnames;
	uint64_t	 n_dirnames;

	const char	 *usernames;
	uint64_t	 n_usernames;

	const char	 *groupnames;
	uint64_t	 n_groupnames;

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
};

struct rpm_inode_map {
	uint32_t	raw;
	uint32_t	compact;
};

struct rpm {
	int64_t		 total_in;
	uint64_t	 hpos;
	uint64_t	 hlen;
	unsigned char	 header[16];
	enum {
		ST_LEAD,	/* Skipping 'Lead' section. */
		ST_HEADER,	/* Reading 'Header' section;
				 * first 16 bytes. */
		ST_HEADER_DATA,	/* Skipping 'Header' section. */
		ST_PADDING,	/* Skipping padding data after the
				 * 'Header' section. */
		ST_ARCHIVE	/* Reading 'Archive' section. */
	}		 state;
	int		 first_header;

    unsigned char	*hbuf;
    uint64_t		hbuf_size;
    uint8_t			have_main_header;
};

static int	rpm_bidder_bid(struct archive_read_filter_bidder *,
		    struct archive_read_filter *);
static int	rpm_bidder_init(struct archive_read_filter *);

static int64_t	rpm_filter_read(struct archive_read_filter *,
		    const void **);
static int	rpm_filter_close(struct archive_read_filter *);

static void	rpm_context_free(void *);

static int	rpm_parse_main_header(struct archive_read *,
			const unsigned char *, uint64_t);

static uint16_t	rpm_be16_at(const void *buf_start,
	const void *buf_end, const uint64_t off);
static uint32_t	rpm_be32_at(const void *buf_start,
	const void *buf_end, const uint64_t off);
static uint64_t	rpm_be64_at(const void *buf_start,
	const void *buf_end, const uint64_t off);

static inline uint64_t	rpm_limit_bytes(uint64_t, uint64_t);
static char	*rpm_safe_strndup(struct archive_read *, const char *);
static void	rpm_strcat(struct archive_string *, const void *, const void *);
static const char	*rpm_strlist_at(const char *, const void *,
	uint64_t, uint64_t);

#if ARCHIVE_VERSION_NUMBER < 4000000
/* Deprecated; remove in libarchive 4.0 */
int
archive_read_support_compression_rpm(struct archive *a)
{
	return archive_read_support_filter_rpm(a);
}
#endif

static const struct archive_read_filter_bidder_vtable
rpm_bidder_vtable = {
	.bid = rpm_bidder_bid,
	.init = rpm_bidder_init,
};

int
archive_read_support_filter_rpm(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;

	return __archive_read_register_bidder(a, NULL, "rpm",
			&rpm_bidder_vtable);
}

static int
rpm_bidder_bid(struct archive_read_filter_bidder *self,
    struct archive_read_filter *filter)
{
	const unsigned char *b;
	int64_t avail;
	int bits_checked;

	(void)self; /* UNUSED */

	b = __archive_read_filter_ahead(filter, 8, &avail);
	if (b == NULL)
		return (0);

	bits_checked = 0;
	/*
	 * Verify Header Magic Bytes
	 */
	if (memcmp(b, RPM_LEAD_MAGIC, sizeof(RPM_LEAD_MAGIC) - 1) != 0)
		return (0);
	bits_checked += 32;
	/*
	 * Check major version.
	 */
	if (b[4] != 3 && b[4] != 4)
		return (0);
	bits_checked += 8;
	/*
	 * Check package type; binary or source.
	 */
	if (b[6] != 0)
		return (0);
	bits_checked += 8;
	if (b[7] != 0 && b[7] != 1)
		return (0);
	bits_checked += 8;

	return (bits_checked);
}

static const struct archive_read_filter_vtable
rpm_reader_vtable = {
	.read = rpm_filter_read,
	.close = rpm_filter_close,
};

static int
rpm_bidder_init(struct archive_read_filter *self)
{
	struct rpm   *rpm;

	self->code = ARCHIVE_FILTER_RPM;
	self->name = "rpm";

	rpm = calloc(1, sizeof(*rpm));
	if (rpm == NULL) {
		archive_set_error(&self->archive->archive, ENOMEM,
		    "can't allocate data for rpm");
		return (ARCHIVE_FATAL);
	}

	self->data = rpm;
	rpm->state = ST_LEAD;
	self->vtable = &rpm_reader_vtable;

	return (ARCHIVE_OK);
}

static int64_t
rpm_filter_read(struct archive_read_filter *self, const void **buff)
{
	struct rpm *rpm;
	const unsigned char *b;
	int64_t avail_in, total, used;
	uint64_t n;
	uint64_t section;
	uint64_t bytes;

	rpm = (struct rpm *)self->data;
	*buff = NULL;
	total = avail_in = 0;
	b = NULL;
	used = 0;
	do {
		if (b == NULL) {
			b = __archive_read_filter_ahead(self->upstream, 1,
			    &avail_in);
			if (b == NULL) {
				if (avail_in < 0)
					return (ARCHIVE_FATAL);
				else
					break;
			}
		}

		switch (rpm->state) {
		case ST_LEAD:
			if (rpm->total_in + avail_in < RPM_LEAD_SIZE)
				used += avail_in;
			else {
				n = (uint64_t)(RPM_LEAD_SIZE - rpm->total_in);
				used += n;
				b += n;
				rpm->state = ST_HEADER;
				rpm->hpos = 0;
				rpm->hlen = 0;
				rpm->first_header = 1;
			}
			break;
		case ST_HEADER:
			n = rpm_limit_bytes(RPM_MIN_HEAD_SIZE - rpm->hpos,
			    avail_in - used);
			memcpy(rpm->header + rpm->hpos, b, n);
			b += n;
			used += n;
			rpm->hpos += n;

			if (rpm->hpos == RPM_MIN_HEAD_SIZE) {
				if (memcmp(rpm->header, RPM_HEADER_MAGIC, sizeof(RPM_HEADER_MAGIC) - 1) != 0) {
					if (rpm->first_header) {
						archive_set_error(
						    &self->archive->archive,
						    ARCHIVE_ERRNO_FILE_FORMAT,
						    "Unrecognized rpm header");
						return (ARCHIVE_FATAL);
					}
					rpm->state = ST_ARCHIVE;
					*buff = rpm->header;
					total = RPM_MIN_HEAD_SIZE;
					break;
				}

				/* Calculate full header length. */
				section = archive_be32dec(rpm->header + 8);
				bytes = archive_be32dec(rpm->header + 12);
				rpm->hlen = rpm->hpos + section * RPM_MIN_HEAD_SIZE + bytes;
				rpm->state = ST_HEADER_DATA;
			}
			break;
		case ST_HEADER_DATA: {
			uint64_t need = rpm->hlen - rpm->hpos;
			n = rpm_limit_bytes(need, avail_in - used);

			rpm->hbuf_size = rpm->hlen;
			rpm->hbuf = realloc(rpm->hbuf, rpm->hbuf_size);
			if (rpm->hbuf == NULL)
				return ARCHIVE_FATAL;

			/* Copy already-read 16-byte header record. */
			memcpy(rpm->hbuf, rpm->header, RPM_MIN_HEAD_SIZE);

			/* Append remaining header bytes at current hpos. */
			memcpy(rpm->hbuf + rpm->hpos, b, n);

			b += n;
			used += n;
			rpm->hpos += n;

			if (rpm->hpos == rpm->hlen) {
				rpm->state = ST_PADDING;

				/* Only parse the MAIN header (not the signature). */
				if (!rpm->first_header && !rpm->have_main_header) {
					const int r = rpm_parse_main_header(self->archive, rpm->hbuf, rpm->hbuf_size);
					if (r != ARCHIVE_OK) {
						total = r;
						goto cleanup;
					}

					rpm->have_main_header = 1;
				}
			}

			break;
		}
		case ST_PADDING:
			while (used < avail_in) {
				if (*b != 0) {
					/* Finished one header; if that was the signature,
					 * the next header is the main header. */
					if (rpm->first_header && !rpm->have_main_header)
						rpm->first_header = 0;

					/* Read next header. */
					rpm->state = ST_HEADER;
					rpm->hpos = 0;
					rpm->hlen = 0;
					break;
				}
				b++;
				used++;
			}
			break;
		case ST_ARCHIVE:
			*buff = b;
			total = avail_in;
			used = avail_in;
			break;
		}
		if (used == avail_in) {
			rpm->total_in += used;
			__archive_read_filter_consume(self->upstream, used);
			b = NULL;
			used = 0;
		}
	} while (total == 0 && avail_in > 0);

	if (used > 0 && b != NULL) {
		rpm->total_in += used;
		__archive_read_filter_consume(self->upstream, used);
	}

cleanup:
	free(rpm->hbuf);
	rpm->hbuf = NULL;
	rpm->hbuf_size = 0;

	return (total);
}

static int
rpm_filter_close(struct archive_read_filter *self)
{
	struct rpm *rpm;

	rpm = (struct rpm *)self->data;
	free(rpm);

	return (ARCHIVE_OK);
}

static void
rpm_context_free(void *p)
{
	struct rpm_context *ctx = p;
	uint64_t i;

	if (ctx == NULL)
		return;

	if (ctx->inodes != NULL) {
		for (i = 0; i < ctx->n_inodes; i++) {
			free(ctx->inodes[i].files);
		}

		free(ctx->inodes);
	}

	if (ctx->files != NULL) {
		for (i = 0; i < ctx->n_files; i++) {
			free(ctx->files[i].pathname);
			free(ctx->files[i].uname);
			free(ctx->files[i].gname);
		}

		free(ctx->files);
	}

	free(ctx);
}

static int
rpm_parse_main_header(struct archive_read *a,
    const unsigned char *hbuf, uint64_t hlen)
{
	struct rpm_context *ctx;
	struct rpm_header_parse hp;
	struct rpm_inode_map *im;
	const unsigned char *hbuf_end = hbuf + hlen;
	uint64_t i, n_index, count;

	memset(&hp, 0, sizeof(hp));

	if (hlen < RPM_MIN_HEAD_SIZE)
		return ARCHIVE_FATAL;

	n_index = archive_be32dec(hbuf + 8);
	count = archive_be32dec(hbuf + 12);

	hbuf += RPM_MIN_HEAD_SIZE;
	hlen -= RPM_MIN_HEAD_SIZE;

	if ((uint64_t)n_index * RPM_MIN_HEAD_SIZE + count > hlen)
		return ARCHIVE_FATAL;

	for (i = 0; i < n_index; i++) {
		uint32_t tag, /*type,*/ cnt;
		int32_t off;
		const unsigned char *p;
		const unsigned char *ip = hbuf + i * RPM_MIN_HEAD_SIZE;

		tag = rpm_be32_at(ip, hbuf_end, 0);
		/*type = rpm_be32_at(ip, hbuf_end, 1);*/
		off = rpm_be32_at(ip, hbuf_end, 2);
		cnt = rpm_be32_at(ip, hbuf_end, 3);

		if (off < 0 || (uint32_t)off >= count)
			continue;

		p = hbuf + (uint64_t)n_index * RPM_MIN_HEAD_SIZE + off;

		switch (tag) {
		case RPMTAG_BASENAMES:
			hp.basenames = (const char *)p;
			hp.n_basenames = cnt;
			break;
		case RPMTAG_DIRNAMES:
			hp.dirnames = (const char *)p;
			hp.n_dirnames = cnt;
			break;
		case RPMTAG_FILEUSERNAMES:
			hp.usernames = (const char *)p;
			hp.n_usernames = cnt;
			break;
		case RPMTAG_FILEGROUPNAMES:
			hp.groupnames = (const char *)p;
			hp.n_groupnames = cnt;
			break;
		case RPMTAG_LONGFILESIZES:
			hp.filesizes64 = (const int64_t *)p;;
			hp.n_files = cnt;
			hp.is_filesizes64 = 1;
			break;
		case RPMTAG_FILESIZES:
			/* This tag should never appear when Longfilesizes is present,
			 * but checking doesn't hurt. */
			if (!hp.is_filesizes64) {
				hp.filesizes32 = (const int32_t *)p;
				hp.n_files = cnt;
			}
			break;
		case RPMTAG_DIRINDEXES:
			hp.dirindexes = (const int32_t *)p;
			break;
		case RPMTAG_FILEINODES:
			hp.fileinodes = (const int32_t *)p;
			break;
		case RPMTAG_FILEMODES:
			hp.filemodes = (const int16_t *)p;
			break;
		case RPMTAG_FILEDEVICES:
			hp.filedevices = (const int32_t *)p;
			break;
		case RPMTAG_FILERDEVS:
			hp.filerdevs = (const int16_t *)p;
			break;
		case RPMTAG_FILEMTIMES:
			hp.filemtimes = (const int32_t *)p;
			break;
		}
	}

	if (hp.n_files >= MAX_NFILES) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
				"n_files out of range");
		return (ARCHIVE_FATAL);
	}

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		return (ARCHIVE_FATAL);

	ctx->files = calloc(hp.n_files, sizeof(*ctx->files));
	if (ctx->files == NULL)
		goto fail;

	ctx->n_files = hp.n_files;

	for (i = 0; i < hp.n_files; i++) {
		struct rpm_file_info *file = &ctx->files[i];
		struct archive_string as;
		const char *dname, *bname, *uname, *gname;

		if (hp.dirindexes != NULL) {
			const uint32_t diri = rpm_be32_at(hp.dirindexes, hbuf_end, i);

			if (diri >= hp.n_dirnames) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
					"dirindex out of range");
				goto fail;
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

		file->uname = rpm_safe_strndup(a, uname ? uname : "");
		file->gname = rpm_safe_strndup(a, gname ? gname : "");

		if (hp.is_filesizes64)
			file->size = rpm_be64_at(hp.filesizes64, hbuf_end, i);
		else
			file->size = rpm_be32_at(hp.filesizes32, hbuf_end, i);
		file->mode = rpm_be16_at(hp.filemodes, hbuf_end, i);
		file->dev = rpm_be32_at(hp.filedevices, hbuf_end, i);
		file->rdev = rpm_be16_at(hp.filerdevs, hbuf_end, i);
		file->mtime = rpm_be32_at(hp.filemtimes, hbuf_end, i);
		file->ino = rpm_be32_at(hp.fileinodes, hbuf_end, i);
	}

	ctx->inodes = calloc(hp.n_files, sizeof(*ctx->inodes));
	if (ctx->inodes == NULL)
		goto fail;

	im = calloc(hp.n_files, sizeof(*im));
	if (im == NULL)
		goto fail;

	count = 0;

	/* Normalize inode numbers for indexing. */
	for (i = 0; i < hp.n_files; i++) {
		struct rpm_file_info *file = &ctx->files[i];
		struct rpm_inode_info *inode;
		const uint32_t raw = file->ino;
		uint32_t j;

		for (j = 0; j < count; j++) {
			if (im[j].raw == raw)
				break;
		}

		if (j == count) {
			im[j].raw = raw;
			im[j].compact = count;
			count++;
		}

		inode = &ctx->inodes[im[j].compact];

		inode->files = realloc(inode->files,
			++inode->n_files * sizeof(struct rpm_file_info *));
		inode->files[inode->n_files - 1] = file;

		file->ino = im[j].compact;
	}

	free(im);

	__archive_read_set_private(a, "rpm", ctx, rpm_context_free);

	return (ARCHIVE_OK);
fail:
	rpm_context_free(ctx);
	return (ARCHIVE_FATAL);
}

static uint16_t
rpm_be16_at(const void *buf_start, const void *buf_end,
	uint64_t off)
{
	off *= sizeof(uint16_t);

	if (!buf_start || (buf_end <= buf_start) ||
		(uint64_t)(buf_end - buf_start) < (off + sizeof(uint16_t)))
		return 0;

	return archive_be16dec(buf_start + off);
}

static uint32_t
rpm_be32_at(const void *buf_start, const void *buf_end,
	uint64_t off)
{
	off *= sizeof(uint32_t);

	if (!buf_start || (buf_end <= buf_start) ||
		(uint64_t)(buf_end - buf_start) < (off + sizeof(uint32_t)))
		return 0;

	return archive_be32dec(buf_start + off);
}

static uint64_t
rpm_be64_at(const void *buf_start, const void *buf_end,
	uint64_t off)
{
	off *= sizeof(uint64_t);

	if (!buf_start || (buf_end <= buf_start) ||
		(uint64_t)(buf_end - buf_start) < (off + sizeof(uint64_t)))
		return 0;

	return archive_be64dec(buf_start + off);
}

static inline uint64_t
rpm_limit_bytes(const uint64_t bytes, const uint64_t max)
{
	return (bytes > max ? max : bytes);
}

static char *
rpm_safe_strndup(struct archive_read *a, const char *s)
{
	uint64_t len;

	if (s == NULL)
		return NULL;

	len = strnlen(s, STR_SIZE_LIMIT);
	if (len == STR_SIZE_LIMIT) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
						  "malformed rpm header string");
		return NULL;
	}

	return strndup(s, len);
}

static void
rpm_strcat(struct archive_string *a, const void *s, const void *buf_end)
{
	if (!s || s >= buf_end)
		return;

	archive_strncat(a, s, rpm_limit_bytes(buf_end - s, STR_SIZE_LIMIT));
}

static const char *
rpm_strlist_at(const char *p, const void *end,
			   uint64_t i, uint64_t n)
{
	uint64_t k;

	if (p == NULL || i >= n || (const char *)end < p)
		return NULL;

	for (k = 0; k < n; k++) {
		uint64_t max_len = rpm_limit_bytes((const char *)end - p,
										   STR_SIZE_LIMIT);

		/* Unterminated or absurdly long string */
		uint64_t len = strnlen(p, max_len);
		if (len == max_len)
			return NULL;

		if (k == i)
			return p;   /* <-- return BEFORE skipping */

		p += len + 1;   /* move to next string */

		if (p >= (const char *)end)
			return NULL;
	}

	return NULL;
}
