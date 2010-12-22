/*-
 * Copyright (c) 2010 Michihiro NAKAJIMA
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_private.h"
#include "archive_endian.h"

/* CFHEADER offset */
#define CFHEADER_signature	0
#define CFHEADER_cbCabinet	8
#define CFHEADER_coffFiles	16
#define CFHEADER_versionMinor	24
#define CFHEADER_versionMajor	25
#define CFHEADER_cFolders	26
#define CFHEADER_cFiles		28
#define CFHEADER_flags		30
#define CFHEADER_setID		32
#define CFHEADER_iCabinet	34
#define CFHEADER_cbCFHeader	36
#define CFHEADER_cbCFFolder	38
#define CFHEADER_cbCFData	39

/* CFFOLDER offset */
#define CFFOLDER_coffCabStart	0
#define CFFOLDER_cCFData	4
#define CFFOLDER_typeCompress	6
#define CFFOLDER_abReserve	8

/* CFFILE offset */
#define CFFILE_cbFile		0
#define CFFILE_uoffFolderStart	4
#define CFFILE_iFolder		8
#define CFFILE_date_time	10
#define CFFILE_attribs		14

/* CFDATA offset */
#define CFDATA_csum		0
#define CFDATA_cbData		4
#define CFDATA_cbUncomp		6

static char *compression_name[] = {
	"NONE",
	"MSZIP",
	"Quantum",
	"LZX",
};

struct cfdata {
	/* Sum value of this CFDATA. */
	uint32_t		 sum;
	uint16_t		 compressed_size;
	uint16_t		 compressed_bytes_remaining;
	uint16_t		 uncompressed_size;
	uint16_t		 uncompressed_bytes_remaining;
	/* To know how many bytes we have decompressed. */
	uint16_t		 uncompressed_avail;
	/* Offset from the beginning of compressed data of this CFDATA */
	uint16_t		 read_offset;
	int64_t			 unconsumed;
	/* To keep memory image of this CFDATA to compute the sum. */
	size_t			 memimage_size;
	unsigned char		*memimage;
	/* Result of calculatation of sum. */
	uint32_t		 sum_calculated;
	unsigned char		 sum_extra[4];
	int			 sum_extra_avail;
	const void		*sum_ptr;
};

struct cffolder {
	uint32_t		 cfdata_offset_in_cab;
	uint16_t		 cfdata_count;
	uint16_t		 comptype;
#define COMPTYPE_NONE		0x0000
#define COMPTYPE_MSZIP		0x0001
#define COMPTYPE_QUANTUM	0x0002
#define COMPTYPE_LZX		0x0003
	uint16_t		 compdata;
	const char		*compname;
	/* At the time reading CFDATA */
	struct cfdata		 cfdata;
	int			 cfdata_index;
	/* Flags to mark progress of decompression. */
	char			 decompress_init;
};

struct cffile {
	uint32_t		 uncompressed_size;
	uint32_t		 offset;
	time_t			 mtime;
	uint16_t	 	 folder;
#define iFoldCONTINUED_FROM_PREV	0xFFFD
#define iFoldCONTINUED_TO_NEXT		0xFFFE
#define iFoldCONTINUED_PREV_AND_NEXT	0xFFFF
	unsigned char		 attr;
#define _A_RDONLY	0x01
#define _A_NAME_IS_UTF	0x80
	struct archive_string 	 pathname;
};

struct cfheader {
	/* Total bytes of all file size in a Cabinet. */
	uint32_t		 total_bytes;
	uint32_t		 files_offset;
	uint16_t		 folder_count;
	uint16_t		 file_count;
	uint16_t		 flags;
#define PREV_CABINET	0x0001
#define NEXT_CABINET	0x0002
#define RESERVE_PRESENT	0x0004
	uint16_t		 setid;
	uint16_t		 cabinet;
	/* Version number. */
	unsigned char		 major;
	unsigned char		 minor;
	unsigned char		 cffolder;
	unsigned char		 cfdata;
	/* All folders in a cabinet. */
	struct cffolder		*folder_array;
	/* All files in a cabinet. */
	struct cffile		*file_array;
	int			 file_index;
};

struct cab {
	/* entry_bytes_remaining is the number of bytes we expect.	    */
	int64_t			 entry_offset;
	int64_t			 entry_bytes_remaining;
	int64_t			 entry_unconsumed;
	int64_t			 entry_compressed_bytes_read;
	int64_t			 entry_uncompressed_bytes_read;
	struct cffolder		*entry_cffolder;
	struct cffile		*entry_cffile;
	struct cfdata		*entry_cfdata;

	/* Offset from beginning of a cabinet file. */
	int64_t			 cab_offset;
	struct cfheader		 cfheader;
	struct archive_wstring	 ws;
	struct archive_string	 mbs;

	/* Flag to mark progress that an archive was read ther first header.*/
	char			 found_header;
	char			 end_of_archive;
	char			 end_of_entry;
	char			 end_of_entry_cleanup;

	unsigned char		*uncompressed_buffer;
	size_t			 uncompressed_buffer_size;

	char			 format_name[64];

#ifdef HAVE_ZLIB_H
	z_stream		 stream;
	char			 stream_valid;
#endif
};

static int	archive_read_format_cab_bid(struct archive_read *);
static int	archive_read_format_cab_read_header(struct archive_read *,
		    struct archive_entry *);
static int	archive_read_format_cab_read_data(struct archive_read *,
		    const void **, size_t *, int64_t *);
static int	archive_read_format_cab_read_data_skip(struct archive_read *);
static int	archive_read_format_cab_cleanup(struct archive_read *);

static int	cab_skip_sfx(struct archive_read *);
static time_t	cab_dos_time(const unsigned char *);
static int	cab_read_data(struct archive_read *, const void **,
		    size_t *, int64_t *);
static int	cab_read_header(struct archive_read *);
static uint32_t cab_checksum_cfdata_4(const void *, size_t bytes, uint32_t);
static uint32_t cab_checksum_cfdata(const void *, size_t bytes, uint32_t);
static void	cab_checksum_update(struct archive_read *, size_t);
static int	cab_checksum_finish(struct archive_read *);
static int	cab_next_cfdata(struct archive_read *);
static const void *cab_read_ahead_cfdata(struct archive_read *, ssize_t *);
static const void *cab_read_ahead_cfdata_none(struct archive_read *, ssize_t *);
static const void *cab_read_ahead_cfdata_deflate(struct archive_read *,
		    ssize_t *);
static int64_t	cab_consume_cfdata(struct archive_read *, int64_t);
static int64_t	cab_minimum_consume_cfdata(struct archive_read *, int64_t);


int
archive_read_support_format_cab(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct cab *cab;
	int r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_format_cab");

	cab = (struct cab *)calloc(1, sizeof(*cab));
	if (cab == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate CAB data");
		return (ARCHIVE_FATAL);
	}
	archive_string_init(&cab->ws);
	archive_wstring_ensure(&cab->ws, 256);
	archive_string_init(&cab->mbs);
	archive_string_ensure(&cab->mbs, 256);

	r = __archive_read_register_format(a,
	    cab,
	    "cab",
	    archive_read_format_cab_bid,
	    NULL,
	    archive_read_format_cab_read_header,
	    archive_read_format_cab_read_data,
	    archive_read_format_cab_read_data_skip,
	    archive_read_format_cab_cleanup);

	if (r != ARCHIVE_OK)
		free(cab);
	return (ARCHIVE_OK);
}

static int
is_cab_magic(const void *h)
{
	const unsigned char *p = h;

	switch (p[4]) {
	case 0:
		/*
		 * Note: Self-Extraction program has 'MSCF' string in their
		 * program. If we were finding 'MSCF' string only, we got
		 * wrong place for Cabinet header. thous we have to check
		 * following four bytes which are reserved and must be set
		 * to zero.
		 */
		if (p[0] == 'M' && p[1] == 'S' && p[2] == 'C' && p[3] == 'F' &&
		    p[5] == 0 && p[6] == 0 && p[7] == 0)
			return 0;
		return 5;
	case 'F': return 1;
	case 'C': return 2;
	case 'S': return 3;
	case 'M': return 4;
	default:  return 5;
	}
}

static int
archive_read_format_cab_bid(struct archive_read *a)
{
	const void *h;
	const char *p;
	ssize_t bytes_avail, offset, window;

	if ((p = __archive_read_ahead(a, 4, NULL)) == NULL)
		return (-1);

	if (is_cab_magic(p) == 0)
		return (32);

	/*
	 * Attempt to handle self-extracting archives
	 * by noting a PE header and searching forward
	 * up to 128k for a 'MSCF' marker.
	 */
	if (p[0] == 'M' && p[1] == 'Z') {
		offset = 0;
		window = 4096;
		while (offset < (1024 * 128)) {
			h = __archive_read_ahead(a, offset + window,
			    &bytes_avail);
			if (h == NULL) {
				/* Remaining bytes are less than window. */
				window >>= 1;
				if (window < 128)
					return (0);
				continue;
			}
			p = (const char *)h + offset;
			while (p + 8 < (const char *)h + bytes_avail) {
				int next;
				if ((next = is_cab_magic(p)) == 0)
					return (32);
				p += next;
			}
			offset = p - (const char *)h;
		}
	}
	return (0);
}

static int
cab_skip_sfx(struct archive_read *a)
{
	const void *h;
	const char *p, *q;
	size_t skip;
	ssize_t bytes, window;

	window = 4096;
	for (;;) {
		h = __archive_read_ahead(a, window, &bytes);
		if (h == NULL) {
			/* Remaining size are less than window. */
			window >>= 1;
			if (window < 128) {
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Couldn't find out CAB header");
				return (ARCHIVE_FATAL);
			}
			continue;
		}
		p = h;
		q = p + bytes;

		/*
		 * Scan ahead until we find something that looks
		 * like the cab header.
		 */
		while (p + 8 < q) {
			int next;
			if ((next = is_cab_magic(p)) == 0) {
				skip = p - (const char *)h;
				__archive_read_consume(a, skip);
				return (ARCHIVE_OK);
			}
			p += next;
		}
		skip = p - (const char *)h;
		__archive_read_consume(a, skip);
	}
}

static int
truncated_error(struct archive_read *a)
{
	archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
	    "Truncated CAB header");
	return (ARCHIVE_FATAL);
}

static int
cab_strnlen(const unsigned char *p, size_t maxlen)
{
	size_t i;

	for (i = 0; i <= maxlen; i++) {
		if (p[i] == 0)
			break;
	}
	if (i > maxlen)
		return (-1);/* invalid */
	return (i);
}

/* Read bytes as much as reamining. */
static const void *
cab_read_ahead_remaining(struct archive_read *a, size_t min, ssize_t *avail)
{
	const void *p;

	while (min > 0) {
		p = __archive_read_ahead(a, min, avail);
		if (p != NULL)
			return (p);
		min--;
	}
	return (NULL);
}

/* Convert a path separator '\' -> '/' */
static void
cab_convert_path_separator(struct cab *cab, struct archive_string *pathname,
    unsigned char attr)
{
	int l, r;

	if (strchr(pathname->s, '\\') == NULL)
		return;

	if ((attr & _A_NAME_IS_UTF) != 0 ||
	    archive_wstrcpy_mbs(&(cab->ws), pathname) != 0) {
		for (l = 0; pathname->s[l] != '\0'; l++) {
			if (pathname->s[l] == '\\')
				pathname->s[l] = '/';
		}
		return;
	}

	r = 0;
	for (l = 0; cab->ws.s[l] != L'\0'; l++) {
		if (cab->ws.s[l] == L'\\') {
			cab->ws.s[l] = L'/';
			r = 1;
		}
	}
	if (r) {
		archive_string_empty(&cab->mbs);
		archive_strappend_w_mbs(&cab->mbs, cab->ws.s);
		/* If mbs length is differenct to pathname, we broked the
		 * pathname. We shouldn't use it. */
		if (archive_strlen(&cab->mbs) == archive_strlen(pathname))
			archive_string_copy(pathname, &cab->mbs);
	}
}

/*
 * Read CFHEADER, CFFOLDER and CFFILE.
 */
static int
cab_read_header(struct archive_read *a)
{
	const unsigned char *p;
	struct cab *cab;
	struct cfheader *hd;
	size_t bytes, used;
	int64_t skip;
	int err, i, len;
	int cur_folder, prev_folder;
	uint32_t offset32;
	
	a->archive.archive_format = ARCHIVE_FORMAT_CAB;
	if (a->archive.archive_format_name == NULL)
		a->archive.archive_format_name = "CAB";

	if ((p = __archive_read_ahead(a, 42, NULL)) == NULL)
		return (truncated_error(a));

	cab = (struct cab *)(a->format->data);
	if (cab->found_header == 0 &&
	    p[0] == 'M' && p[1] == 'Z') {
		/* This is an executable?  Must be self-extracting... 	*/
		err = cab_skip_sfx(a);
		if (err < ARCHIVE_WARN)
			return (err);

		if ((p = __archive_read_ahead(a, sizeof(*p), NULL)) == NULL)
			return (truncated_error(a));
	}

	cab->cab_offset = 0;
	/*
	 * Read CFHEADER.
	 */
	hd = &cab->cfheader;
	if (p[CFHEADER_signature+0] != 'M' || p[CFHEADER_signature+1] != 'S' ||
	    p[CFHEADER_signature+2] != 'C' || p[CFHEADER_signature+3] != 'F') {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Couldn't find out CAB header");
		return (ARCHIVE_FATAL);
	}
	hd->total_bytes = archive_le32dec(p + CFHEADER_cbCabinet);
	hd->files_offset = archive_le32dec(p + CFHEADER_coffFiles);
	hd->minor = p[CFHEADER_versionMinor];
	hd->major = p[CFHEADER_versionMajor];
	hd->folder_count = archive_le16dec(p + CFHEADER_cFolders);
	if (hd->folder_count == 0)
		goto invalid;
	hd->file_count = archive_le16dec(p + CFHEADER_cFiles);
	if (hd->file_count == 0)
		goto invalid;
	hd->flags = archive_le16dec(p + CFHEADER_flags);
	hd->setid = archive_le16dec(p + CFHEADER_setID);
	hd->cabinet = archive_le16dec(p + CFHEADER_iCabinet);
	used = CFHEADER_iCabinet + 2;
	if (hd->flags & RESERVE_PRESENT) {
		uint16_t cfheader;
		cfheader = archive_le16dec(p + CFHEADER_cbCFHeader);
		if (cfheader > 60000U)
			goto invalid;
		hd->cffolder = p[CFHEADER_cbCFFolder];
		hd->cfdata = p[CFHEADER_cbCFData];
		used += 4;/* cbCFHeader, cbCFFolder and cbCFData */
		used += cfheader;/* abReserve */
	} else
		hd->cffolder = 0;/* Avoid compiling warning. */
	if (hd->flags & PREV_CABINET) {
		/* How many bytes are used for szCabinetPrev. */
		if ((p = __archive_read_ahead(a, used+256, NULL)) == NULL)
			return (truncated_error(a));
		if ((len = cab_strnlen(p + used, 255)) <= 0)
			goto invalid;
		used += len + 1;
		/* How many bytes are used for szDiskPrev. */
		if ((p = __archive_read_ahead(a, used+256, NULL)) == NULL)
			return (truncated_error(a));
		if ((len = cab_strnlen(p + used, 255)) <= 0)
			goto invalid;
		used += len + 1;
	}
	if (hd->flags & NEXT_CABINET) {
		/* How many bytes are used for szCabinetNext. */
		if ((p = __archive_read_ahead(a, used+256, NULL)) == NULL)
			return (truncated_error(a));
		if ((len = cab_strnlen(p + used, 255)) <= 0)
			goto invalid;
		used += len + 1;
		/* How many bytes are used for szDiskNext. */
		if ((p = __archive_read_ahead(a, used+256, NULL)) == NULL)
			return (truncated_error(a));
		if ((len = cab_strnlen(p + used, 255)) <= 0)
			goto invalid;
		used += len + 1;
	}
	__archive_read_consume(a, used);
	cab->cab_offset += used;
	used = 0;

	/*
	 * Read CFFOLDER.
	 */
	hd->folder_array = (struct cffolder *)calloc(
	    hd->folder_count, sizeof(struct cffolder));
	if (hd->folder_array == NULL)
		goto nomem;
	
	bytes = 8;
	if (hd->flags & RESERVE_PRESENT)
		bytes += hd->cffolder;
	bytes *= hd->folder_count;
	if ((p = __archive_read_ahead(a, bytes, NULL)) == NULL)
		return (truncated_error(a));
	offset32 = 0;
	for (i = 0; i < hd->folder_count; i++) {
		struct cffolder *folder = &(hd->folder_array[i]);
		folder->cfdata_offset_in_cab =
		    archive_le32dec(p + CFFOLDER_coffCabStart);
		folder->cfdata_count = archive_le16dec(p+CFFOLDER_cCFData);
		folder->comptype =
		    archive_le16dec(p+CFFOLDER_typeCompress) & 0x0F;
		folder->compdata =
		    archive_le16dec(p+CFFOLDER_typeCompress) >> 4;
		/* Get a compression name. */
		if (folder->comptype <
		    sizeof(compression_name) / sizeof(compression_name[0]))
			folder->compname = compression_name[folder->comptype];
		else
			folder->compname = "UNKNOWN";
		p += 8;
		used += 8;
		if (hd->flags & RESERVE_PRESENT) {
			p += hd->cffolder;/* abReserve */
			used += hd->cffolder;
		}
		/*
		 * Sanity check if each data is acceptable.
		 */
		if (offset32 >= folder->cfdata_offset_in_cab)
			goto invalid;
		offset32 = folder->cfdata_offset_in_cab;

		/* Set a request to initialize zlib for the CFDATA of
		 * this folder. */
		folder->decompress_init = 0;
	}
	__archive_read_consume(a, used);
	cab->cab_offset += used;

	/*
	 * Read CFFILE.
	 */
	/* Seek read pointer to the offset of CFFILE if needed. */
	skip = (int64_t)hd->files_offset - cab->cab_offset;
	if (skip <  0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Invalid offset of CFFILE %jd < %jd",
		    (intmax_t)hd->files_offset, (intmax_t)cab->cab_offset);
		return (ARCHIVE_FATAL);
	}
	if (skip) {
		__archive_read_consume(a, skip);
		cab->cab_offset += skip;
	}
	/* Allocate memory for CFDATA */
	hd->file_array = (struct cffile *)calloc(
	    hd->file_count, sizeof(struct cffile));
	if (hd->file_array == NULL)
		goto nomem;

	prev_folder = -1;
	for (i = 0; i < hd->file_count; i++) {
		struct cffile *file = &(hd->file_array[i]);
		ssize_t avail;

		if ((p = __archive_read_ahead(a, 16, NULL)) == NULL)
			return (truncated_error(a));
		file->uncompressed_size = archive_le32dec(p + CFFILE_cbFile);
		file->offset = archive_le32dec(p + CFFILE_uoffFolderStart);
		file->folder = archive_le16dec(p + CFFILE_iFolder);
		file->mtime = cab_dos_time(p + CFFILE_date_time);
		file->attr = archive_le16dec(p + CFFILE_attribs);
		__archive_read_consume(a, 16);

		cab->cab_offset += 16;
		if ((p = cab_read_ahead_remaining(a, 256, &avail)) == NULL)
			return (truncated_error(a));
		if ((len = cab_strnlen(p, avail-1)) <= 0)
			goto invalid;
		archive_string_init(&(file->pathname));
		archive_strncpy(&(file->pathname),  p, len);
		__archive_read_consume(a, len + 1);
		cab->cab_offset += len + 1;
		/* Convert a path separator '\' -> '/' */
		cab_convert_path_separator(cab, &(file->pathname),
		    file->attr);

		/*
		 * Sanity check if each data is acceptable.
		 */
		if (file->uncompressed_size > 0x7FFF8000)
			goto invalid;/* Too large */
		if ((int64_t)file->offset + (int64_t)file->uncompressed_size
		    > ARCHIVE_LITERAL_LL(0x7FFF8000))
			goto invalid;/* Too large */
		switch (file->folder) {
		case iFoldCONTINUED_TO_NEXT:
			/* This must be last file in a folder. */
			if (i != hd->file_count -1)
				goto invalid;
			cur_folder = hd->folder_count -1;
			break;
		case iFoldCONTINUED_PREV_AND_NEXT:
			/* This must be only one file in a folder. */
			if (hd->file_count != 1)
				goto invalid;
			/* FALL THROUGH */
		case iFoldCONTINUED_FROM_PREV:
			/* This must be first file in a folder. */
			if (i != 0)
				goto invalid;
			prev_folder = cur_folder = 0;
			offset32 = file->offset;
			break;
		default:
			if (file->folder >= hd->folder_count)
				goto invalid;
			cur_folder = file->folder;
			break;
		}
		/* Dot not back track. */
		if (cur_folder < prev_folder)
			goto invalid;
		if (cur_folder != prev_folder)
			offset32 = 0;
		prev_folder = cur_folder;

		/* Make sure there are not any blanks from last file
		 * contents. */
		if (offset32 != file->offset)
			goto invalid;
		offset32 += file->uncompressed_size;

		/* CFDATA is available for file contents. */
		if (file->uncompressed_size > 0 &&
		    hd->folder_array[cur_folder].cfdata_count == 0)
			goto invalid;
	}

	if (hd->cabinet != 0 || hd->flags & (PREV_CABINET | NEXT_CABINET)) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Multivolume cabinet file is unsupported");
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
invalid:
	archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
	    "Invalid CAB header");
	return (ARCHIVE_FATAL);
nomem:
	archive_set_error(&a->archive, ENOMEM,
	    "Can't allocate memory for CAB data");
	return (ARCHIVE_FATAL);
}

static int
archive_read_format_cab_read_header(struct archive_read *a,
    struct archive_entry *entry)
{
	struct cab *cab;
	struct cfheader *hd;
	struct cffolder *prev_folder;
	struct cffile *file;
	int err = ARCHIVE_OK;
	
	cab = (struct cab *)(a->format->data);
	if (cab->found_header == 0) {
		err = cab_read_header(a); 
		if (err < ARCHIVE_WARN)
			return (err);
		/* We've found the header. */
		cab->found_header = 1;
	}
	hd = &cab->cfheader;

	if (hd->file_index >= hd->file_count) {
		cab->end_of_archive = 1;
		return (ARCHIVE_EOF);
	}
	file = &hd->file_array[hd->file_index++];

	cab->end_of_entry = 0;
	cab->end_of_entry_cleanup = 0;
	cab->entry_compressed_bytes_read = 0;
	cab->entry_uncompressed_bytes_read = 0;
	cab->entry_unconsumed = 0;
	cab->entry_cffile = file;

	/*
	 * Choose a proper folder.
	 */
	prev_folder = cab->entry_cffolder;
	switch (file->folder) {
	case iFoldCONTINUED_FROM_PREV:
	case iFoldCONTINUED_PREV_AND_NEXT:
		cab->entry_cffolder = &hd->folder_array[0];
		break;
	case iFoldCONTINUED_TO_NEXT:
		cab->entry_cffolder = &hd->folder_array[hd->folder_count-1];
		break;
	default:
		cab->entry_cffolder = &hd->folder_array[file->folder];
		break;
	}
	/* If a cffolder of this file is changed, reset a cfdata to read
	 * file contents from next cfdata. */
	if (prev_folder != cab->entry_cffolder)
		cab->entry_cfdata = NULL;

	/*
	 * Set a default value and common data
	 */
	archive_entry_set_pathname(entry, file->pathname.s);
	if (file->attr & _A_NAME_IS_UTF)
		archive_entry_update_pathname_utf8(entry, file->pathname.s);

	archive_entry_set_size(entry, file->uncompressed_size);
	if (file->attr & _A_RDONLY)
		archive_entry_set_mode(entry, AE_IFREG | 0555);
	else
		archive_entry_set_mode(entry, AE_IFREG | 0777);
	archive_entry_set_mtime(entry, file->mtime, 0);

	cab->entry_bytes_remaining = file->uncompressed_size;
	cab->entry_offset = 0;
	/* We don't need compress data. */
	if (file->uncompressed_size == 0)
		cab->end_of_entry_cleanup = cab->end_of_entry = 1;

	/* Set up a more descriptive format name. */
	sprintf(cab->format_name, "CAB %d.%d (%s)",
	    hd->major, hd->minor, cab->entry_cffolder->compname);
	a->archive.archive_format_name = cab->format_name;

	return (err);
}

static int
archive_read_format_cab_read_data(struct archive_read *a,
    const void **buff, size_t *size, int64_t *offset)
{
	struct cab *cab = (struct cab *)(a->format->data);
	int r;

	switch (cab->entry_cffile->folder) {
	case iFoldCONTINUED_FROM_PREV:
	case iFoldCONTINUED_TO_NEXT:
	case iFoldCONTINUED_PREV_AND_NEXT:
		*buff = NULL;
		*size = 0;
		*offset = 0;
		archive_clear_error(&a->archive);
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Cannot restore this file splited in multivolume.");
		return (ARCHIVE_FAILED);
		break;
	}
	if (cab->entry_unconsumed) {
		/* Consume as much as the compressor actually used. */
		r = cab_consume_cfdata(a, cab->entry_unconsumed);
		cab->entry_unconsumed = 0;
		if (r < 0)
			return (r);
	}
	if (cab->end_of_archive || cab->end_of_entry) {
		if (!cab->end_of_entry_cleanup) {
			/* End-of-entry cleanup done. */
			cab->end_of_entry_cleanup = 1;
		}
		*offset = cab->entry_offset;
		*size = 0;
		*buff = NULL;
		return (ARCHIVE_EOF);
	}

	return (cab_read_data(a, buff, size, offset));
}

static uint32_t
cab_checksum_cfdata_4(const void *p, size_t bytes, uint32_t seed)
{
	const unsigned char *b;
	int u32num;
	uint32_t sum;

	u32num = bytes / 4;
	sum = seed;
	b = p;
	while (--u32num >= 0) {
		sum ^= archive_le32dec(b);
		b += 4;
	}
	return (sum);
}

static uint32_t
cab_checksum_cfdata(const void *p, size_t bytes, uint32_t seed)
{
	const unsigned char *b;
	uint32_t sum;
	uint32_t t;

	sum = cab_checksum_cfdata_4(p, bytes, seed);
	b = p;
	b += bytes & ~3;
	t = 0;
	switch (bytes & 3) {
	case 3:
		t |= ((uint32_t)(*b++)) << 16;
		/* FALL THROUGH */
	case 2:
		t |= ((uint32_t)(*b++)) << 8;
		/* FALL THROUGH */
	case 1:
		t |= *b;
		/* FALL THROUGH */
	default:
		break;
	}
	sum ^= t;

	return (sum);
}

static void
cab_checksum_update(struct archive_read *a, size_t bytes)
{
	struct cab *cab = (struct cab *)(a->format->data);
	struct cfdata *cfdata = cab->entry_cfdata;
	const unsigned char *p;
	size_t sumbytes;

	if (cfdata->sum == 0 || cfdata->sum_ptr == NULL)
		return;
	/*
	 * Calculate the sum of this CFDATA.
	 * Make sure CFDATA must be calculated in four bytes.
	 */
	p = cfdata->sum_ptr;
	sumbytes = bytes;
	if (cfdata->sum_extra_avail) {
		while (cfdata->sum_extra_avail < 4 && sumbytes > 0) {
			cfdata->sum_extra[
			    cfdata->sum_extra_avail++] = *p++;
			sumbytes--;
		}
		if (cfdata->sum_extra_avail == 4) {
			cfdata->sum_calculated = cab_checksum_cfdata_4(
			    cfdata->sum_extra, 4, cfdata->sum_calculated);
			cfdata->sum_extra_avail = 0;
		}
	}
	if (sumbytes) {
		int odd = sumbytes & 3;
		if (sumbytes - odd > 0)
			cfdata->sum_calculated = cab_checksum_cfdata_4(
			    p, sumbytes - odd, cfdata->sum_calculated);
		if (odd)
			memcpy(cfdata->sum_extra, p + sumbytes - odd, odd);
		cfdata->sum_extra_avail = odd;
	}
	cfdata->sum_ptr = NULL;
}

static int
cab_checksum_finish(struct archive_read *a)
{
	struct cab *cab = (struct cab *)(a->format->data);
	struct cfdata *cfdata = cab->entry_cfdata;
	int l;

	/* Do not need to compute a sum. */
	if (cfdata->sum == 0)
		return (ARCHIVE_OK);

	/*
	 * Calculate the sum of remaining CFDATA.
	 */
	if (cfdata->sum_extra_avail) {
		cfdata->sum_calculated =
		    cab_checksum_cfdata(cfdata->sum_extra,
		       cfdata->sum_extra_avail, cfdata->sum_calculated);
		cfdata->sum_extra_avail = 0;
	}

	l = 4;
	if (cab->cfheader.flags & RESERVE_PRESENT)
		l += cab->cfheader.cfdata;
	cfdata->sum_calculated = cab_checksum_cfdata(
	    cfdata->memimage + CFDATA_cbData, l, cfdata->sum_calculated);
	if (cfdata->sum_calculated != cfdata->sum) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Checksum error CFDATA[%d] %x:%x in %d bytes",
		    cab->entry_cffolder->cfdata_index -1,
		    cfdata->sum, cfdata->sum_calculated,
		    cfdata->compressed_size);
		return (ARCHIVE_FAILED);
	}
	return (ARCHIVE_OK);
}

/*
 * Read CFDATA if needed.
 */
static int
cab_next_cfdata(struct archive_read *a)
{
	struct cab *cab = (struct cab *)(a->format->data);
	struct cfdata *cfdata = cab->entry_cfdata;

	/* There are remaining bytes in current CFDATA, use it first. */
	if (cfdata != NULL && cfdata->uncompressed_bytes_remaining > 0)
		return (ARCHIVE_OK);

	if (cfdata == NULL) {
		int64_t skip;

		cab->entry_cffolder->cfdata_index = 0;

		/* Seek read pointer to the offset of CFDATA if needed. */
		skip = cab->entry_cffolder->cfdata_offset_in_cab
			- cab->cab_offset;
		if (skip < 0) {
			int folder_index;
			switch (cab->entry_cffile->folder) {
			case iFoldCONTINUED_FROM_PREV:
			case iFoldCONTINUED_PREV_AND_NEXT:
				folder_index = 0;
				break;
			case iFoldCONTINUED_TO_NEXT:
				folder_index = cab->cfheader.folder_count-1;
				break;
			default:
				folder_index = cab->entry_cffile->folder;
				break;
			}
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Invalid offset of CFDATA in folder(%d) %jd < %jd",
			    folder_index,
			    (intmax_t)cab->entry_cffolder->cfdata_offset_in_cab,
			    (intmax_t)cab->cab_offset);
			return (ARCHIVE_FATAL);
		}
		if (skip > 0) {
			if (__archive_read_consume(a, skip) < 0)
				return (ARCHIVE_FATAL);
			cab->cab_offset =
			    cab->entry_cffolder->cfdata_offset_in_cab;
		}
	}

	/*
	 * Read a CFDATA.
	 */
	if (cab->entry_cffolder->cfdata_index <
	    cab->entry_cffolder->cfdata_count) {
		const unsigned char *p;
		int l;

		cfdata = &(cab->entry_cffolder->cfdata);
		cab->entry_cffolder->cfdata_index++;
		cab->entry_cfdata = cfdata;
		cfdata->sum_calculated = 0;
		cfdata->sum_extra_avail = 0;
		cfdata->sum_ptr = NULL;
		l = 8;
		if (cab->cfheader.flags & RESERVE_PRESENT)
			l += cab->cfheader.cfdata;
		if ((p = __archive_read_ahead(a, l, NULL)) == NULL)
			return (truncated_error(a));
		cfdata->sum = archive_le32dec(p + CFDATA_csum);
		cfdata->compressed_size = archive_le16dec(p + CFDATA_cbData);
		cfdata->compressed_bytes_remaining = cfdata->compressed_size;
		cfdata->uncompressed_size =
		    archive_le16dec(p + CFDATA_cbUncomp);
		cfdata->uncompressed_bytes_remaining =
		    cfdata->uncompressed_size;
		cfdata->uncompressed_avail = 0;
		cfdata->read_offset = 0;
		cfdata->unconsumed = 0;

		/*
		 * Sanity check if data size is acceptable.
		 */
		if (cfdata->compressed_size == 0 ||
		    cfdata->compressed_size > (0x8000+6144))
			goto invalid;
		if (cfdata->uncompressed_size > 0x8000)
			goto invalid;
		if (cfdata->uncompressed_size == 0) {
			switch (cab->entry_cffile->folder) {
			case iFoldCONTINUED_PREV_AND_NEXT:
			case iFoldCONTINUED_TO_NEXT:
				break;
			case iFoldCONTINUED_FROM_PREV:
			default:
				goto invalid;
			}
		}
		/* If CFDATA is not last in a folder, an uncompressed
		 * size must be 0x8000(32KBi) */
		if ((cab->entry_cffolder->cfdata_index <
		     cab->entry_cffolder->cfdata_count) &&
		       cfdata->uncompressed_size != 0x8000)
			goto invalid;
		/* A Compressed data size and an uncompressed data size must
		 * be the same in no compression mode. */
		if (cab->entry_cffolder->comptype == COMPTYPE_NONE &&
		    cfdata->compressed_size != cfdata->uncompressed_size)
			goto invalid;

		/*
		 * Save CFDATA image for sum check.
		 */
		if (cfdata->memimage_size < (size_t)l) {
			free(cfdata->memimage);
			cfdata->memimage = malloc(l);
			if (cfdata->memimage == NULL) {
				archive_set_error(&a->archive, ENOMEM,
				    "Can't allocate memory for CAB data");
				return (ARCHIVE_FATAL);
			}
			cfdata->memimage_size = l;
		}
		memcpy(cfdata->memimage, p, l);

		/* Consume bytes as much as we used. */
		__archive_read_consume(a, l);
		cab->cab_offset += l;
	} else if (cab->entry_cffolder->cfdata_count > 0) {
		/* Run out of all CFDATA in a folder. */
		cfdata->compressed_size = 0;
		cfdata->uncompressed_size = 0;
		cfdata->compressed_bytes_remaining = 0;
		cfdata->uncompressed_bytes_remaining = 0;
	} else {
		/* Current folder does not have any CFDATA. */
		cfdata = &(cab->entry_cffolder->cfdata);
		cab->entry_cfdata = cfdata;
		memset(cfdata, 0, sizeof(*cfdata));
	}
	return (ARCHIVE_OK);
invalid:
	archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
	    "Invalid CFDATA");
	return (ARCHIVE_FATAL);
}

/*
 * Read ahead CFDATA.
 */
static const void *
cab_read_ahead_cfdata(struct archive_read *a, ssize_t *avail)
{
	struct cab *cab = (struct cab *)(a->format->data);
	int err;

	err = cab_next_cfdata(a);
	if (err < ARCHIVE_OK) {
		*avail = err;
		return (NULL);
	}

	switch (cab->entry_cffolder->comptype) {
	case COMPTYPE_NONE:
		return (cab_read_ahead_cfdata_none(a, avail));
	case COMPTYPE_MSZIP:
		return (cab_read_ahead_cfdata_deflate(a, avail));
	default: /* Unsupported compression. */
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Unsupported CAB compression : %s",
		    compression_name[cab->entry_cffolder->comptype]);
		*avail = ARCHIVE_FAILED;
		return (NULL);
	}
}

/*
 * Read ahead CFDATA as uncompressed data.
 */
static const void *
cab_read_ahead_cfdata_none(struct archive_read *a, ssize_t *avail)
{
	struct cab *cab = (struct cab *)(a->format->data);
	struct cfdata *cfdata;
	const void *d;
	int64_t skipped_bytes;

	cfdata = cab->entry_cfdata;

	if (cfdata->uncompressed_avail == 0 &&
		cfdata->read_offset > 0) {
		/* we've already skipped some bytes before really read. */
		skipped_bytes = cfdata->read_offset;
		cfdata->read_offset = 0;
		cfdata->uncompressed_bytes_remaining += skipped_bytes;
	} else
		skipped_bytes = 0;
	do {
		/*
		 * Note: '1' here is a performance optimization.
		 * Recall that the decompression layer returns a count of
		 * available bytes; asking for more than that forces the
		 * decompressor to combine reads by copying data.
		 */
		d = __archive_read_ahead(a, 1, avail);
		if (*avail <= 0) {
			*avail = truncated_error(a);
			return (NULL);
		}
		if (*avail > cfdata->uncompressed_bytes_remaining)
			*avail = cfdata->uncompressed_bytes_remaining;
		cfdata->uncompressed_avail = cfdata->uncompressed_size;
		cfdata->unconsumed = *avail;
		cfdata->sum_ptr = d;
		if (skipped_bytes > 0) {
			skipped_bytes =
			    cab_minimum_consume_cfdata(a, skipped_bytes);
			if (skipped_bytes < 0) {
				*avail = ARCHIVE_FATAL;
				return (NULL);
			}
			continue;
		}
	} while (0);

	return (d);
}

/*
 * Read ahead CFDATA as deflate data.
 */
#ifdef HAVE_ZLIB_H
static const void *
cab_read_ahead_cfdata_deflate(struct archive_read *a, ssize_t *avail)
{
	struct cab *cab = (struct cab *)(a->format->data);
	struct cfdata *cfdata;
	const void *d;
	int r, mszip;
	uint16_t uavail;
	char eod = 0;

	cfdata = cab->entry_cfdata;
	/* If the buffer hasn't been allocated, allocate it now. */
	if (cab->uncompressed_buffer == NULL) {
		cab->uncompressed_buffer_size = 0x8000;
		cab->uncompressed_buffer
		    = (unsigned char *)malloc(cab->uncompressed_buffer_size);
		if (cab->uncompressed_buffer == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "No memory for CAB decompression");
			*avail = ARCHIVE_FATAL;
			return (NULL);
		}
	}

	uavail = cfdata->uncompressed_avail;
	if (uavail == cfdata->uncompressed_size) {
		d = cab->uncompressed_buffer + cfdata->read_offset;
		*avail = uavail - cfdata->read_offset;
		return (d);
	}

	if (!cab->entry_cffolder->decompress_init) {
		cab->stream.next_in = NULL;
		cab->stream.avail_in = 0;
		cab->stream.total_in = 0;
		cab->stream.next_out = NULL;
		cab->stream.avail_out = 0;
		cab->stream.total_out = 0;
		if (cab->stream_valid)
			r = inflateReset(&cab->stream);
		else
			r = inflateInit2(&cab->stream,
			    -15 /* Don't check for zlib header */);
		if (r != Z_OK) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Can't initialize deflate decompression.");
			*avail = ARCHIVE_FATAL;
			return (NULL);
		}
		/* Stream structure has been set up. */
		cab->stream_valid = 1;
		/* We've initialized decompression for this stream. */
		cab->entry_cffolder->decompress_init = 1;
	}

	if (cfdata->compressed_bytes_remaining == cfdata->compressed_size)
		mszip = 2;
	else
		mszip = 0;
	eod = 0;
	cab->stream.total_out = uavail;
	/*
	 * We always uncompress all data in current CFDATA.
	 */
	while (!eod && cab->stream.total_out < cfdata->uncompressed_size) {
		ssize_t bytes_avail;

		cab->stream.next_out =
		    cab->uncompressed_buffer + cab->stream.total_out;
		cab->stream.avail_out =
		    cfdata->uncompressed_size - cab->stream.total_out;

		d = __archive_read_ahead(a, 1, &bytes_avail);
		if (bytes_avail <= 0) {
			*avail = truncated_error(a);
			return (NULL);
		}
		if (bytes_avail > cfdata->compressed_bytes_remaining)
			bytes_avail = cfdata->compressed_bytes_remaining;
		/*
		 * A bug in zlib.h: stream.next_in should be marked 'const'
		 * but isn't (the library never alters data through the
		 * next_in pointer, only reads it).  The result: this ugly
		 * cast to remove 'const'.
		 */
		cab->stream.next_in = (Bytef *)(uintptr_t)d;
		cab->stream.avail_in = bytes_avail;
		cab->stream.total_in = 0;

		/* Cut out a tow-byte MSZIP signature(0x43, 0x4b). */
		if (mszip > 0) {
			if (bytes_avail <= mszip) {
				if (mszip == 2) {
					if (cab->stream.next_in[0] != 0x43)
						goto nomszip;
					if (bytes_avail > 1 &&
					    cab->stream.next_in[1] != 0x4b)
						goto nomszip;
				} else if (cab->stream.next_in[0] != 0x4b)
					goto nomszip;
				cfdata->unconsumed = bytes_avail;
				cfdata->sum_ptr = d;
				if (cab_minimum_consume_cfdata(
				    a, cfdata->unconsumed) < 0) {
					*avail = ARCHIVE_FATAL;
					return (NULL);
				}
				mszip -= bytes_avail;
				continue;
			}
			if (mszip == 1 && cab->stream.next_in[0] != 0x4b)
				goto nomszip;
			else if (cab->stream.next_in[0] != 0x43 ||
			    cab->stream.next_in[1] != 0x4b)
				goto nomszip;
			cab->stream.next_in += mszip;
			cab->stream.avail_in -= mszip;
			cab->stream.total_in += mszip;
			mszip = 0;
		}

		r = inflate(&cab->stream, 0);
		switch (r) {
		case Z_OK:
			break;
		case Z_STREAM_END:
			eod = 1;
			break;
		default:
			goto zlibfailed;
		}
		cfdata->unconsumed = cab->stream.total_in;
		cfdata->sum_ptr = d;
		if (cab_minimum_consume_cfdata(a, cfdata->unconsumed) < 0) {
			*avail = ARCHIVE_FATAL;
			return (NULL);
		}
	}
	uavail = cab->stream.total_out;

	if (uavail < cfdata->uncompressed_size) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Invalid decompression size (%d < %d)",
		    uavail, cfdata->uncompressed_size);
		*avail = ARCHIVE_FATAL;
		return (NULL);
	}

	/*
	 * Note: I doubt there is a bug in makecab.exe because compressed
	 * bytes are still remaining regardless we have gotten all
	 * uncompressed bytes, which size is recoded in CFDATA, as much as
	 * we need and we have to use the garbage to compute the sum of
	 * CFDATA.
	 */
	if (cfdata->compressed_bytes_remaining > 0) {
		ssize_t bytes_avail;

		d = __archive_read_ahead(a, cfdata->compressed_bytes_remaining,
		    &bytes_avail);
		if (bytes_avail <= 0) {
			*avail = truncated_error(a);
			return (NULL);
		}
		cfdata->unconsumed = cfdata->compressed_bytes_remaining;
		cfdata->sum_ptr = d;
		if (cab_minimum_consume_cfdata(a, cfdata->unconsumed) < 0) {
			*avail = ARCHIVE_FATAL;
			return (NULL);
		}
	}

	/*
	 * Set dictionary data for decompressing of next CFDATA, which
	 * in the same folder. This is why we always do decompress CFDATA
	 * even if beginning CFDATA or some of CFDATA are not used in
	 * skipping file data.
	 */
	if (cab->entry_cffolder->cfdata_index <
	    cab->entry_cffolder->cfdata_count) {
		r = inflateReset(&cab->stream);
		if (r != Z_OK)
			goto zlibfailed;
		r = inflateSetDictionary(&cab->stream,
		    cab->uncompressed_buffer, cfdata->uncompressed_size);
		if (r != Z_OK)
			goto zlibfailed;
	}

	d = cab->uncompressed_buffer + cfdata->read_offset;
	*avail = uavail - cfdata->read_offset;
	cfdata->uncompressed_avail = uavail;

	return (d);

zlibfailed:
	switch (r) {
	case Z_MEM_ERROR:
		archive_set_error(&a->archive, ENOMEM,
		    "Out of memory for deflate decompression");
		break;
	default:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Deflate decompression failed (%d)", r);
		break;
	}
	*avail = ARCHIVE_FATAL;
	return (NULL);
nomszip:
	archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
	    "CFDATA incorrect(no MSZIP signature)");
	*avail = ARCHIVE_FATAL;
	return (NULL);
}

#else /* HAVE_ZLIB_H */

static const void *
cab_read_ahead_cfdata_deflate(struct archive_read *a, ssize_t *avail)
{
	*avail = ARCHIVE_FATAL;
	archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
	    "libarchive compiled without deflate support (no libz)");
	return (NULL);
}

#endif /* HAVE_ZLIB_H */

/*
 * Consume CFDATA.
 * We always decompress CFDATA to consume CFDATA as much as we need
 * in uncompressed bytes because all CFDATA in a folder are related
 * so we do not skip any CFDATA without decompressing.
 * Note: If the folder of a CFFILE is iFoldCONTINUED_PREV_AND_NEXT or
 * iFoldCONTINUED_FROM_PREV, we won't decompress because a CFDATA for
 * the CFFILE is remaining bytes of previous Multivolume CAB file.
 */
static int64_t
cab_consume_cfdata(struct archive_read *a, int64_t consumed_bytes)
{
	struct cab *cab = (struct cab *)(a->format->data);
	struct cfdata *cfdata;
	int64_t cbytes, rbytes;
	int err;

	rbytes = cab_minimum_consume_cfdata(a, consumed_bytes);
	if (rbytes < 0)
		return (ARCHIVE_FATAL);

	cfdata = cab->entry_cfdata;
	while (rbytes > 0) {
		ssize_t avail;

		if (cfdata->compressed_size == 0) {
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_FILE_FORMAT,
			    "Invalid CFDATA");
			return (ARCHIVE_FATAL);
		}
		cbytes = cfdata->uncompressed_bytes_remaining;
		if (cbytes > rbytes)
			cbytes = rbytes;
		rbytes -= cbytes;

		if (cfdata->uncompressed_avail == 0 &&
		    (cab->entry_cffolder->comptype == COMPTYPE_NONE ||
		     cab->entry_cffile->folder == iFoldCONTINUED_PREV_AND_NEXT ||
			 cab->entry_cffile->folder == iFoldCONTINUED_FROM_PREV)) {
			/* We have not read any data yet. */
			if (cbytes == cfdata->uncompressed_bytes_remaining) {
				/* Skip whole current CFDATA. */
				__archive_read_consume(a,
				    cfdata->compressed_size);
				cab->cab_offset += cfdata->compressed_size;
				cfdata->compressed_bytes_remaining = 0;
				cfdata->uncompressed_bytes_remaining = 0;
				err = cab_next_cfdata(a);
				if (err < 0)
					return (err);
				cfdata = cab->entry_cfdata;
				if (cfdata->uncompressed_size == 0) {
					switch (cab->entry_cffile->folder) {
					case iFoldCONTINUED_PREV_AND_NEXT:
					case iFoldCONTINUED_TO_NEXT:
					case iFoldCONTINUED_FROM_PREV:
						rbytes = 0;
						break;
					default:
						break;
					}
				}
				continue;
			}
			cfdata->read_offset += cbytes;
			cfdata->uncompressed_bytes_remaining -= cbytes;
			break;
		} else if (cbytes == 0) {
			err = cab_next_cfdata(a);
			if (err < 0)
				return (err);
			cfdata = cab->entry_cfdata;
			if (cfdata->uncompressed_size == 0) {
				switch (cab->entry_cffile->folder) {
				case iFoldCONTINUED_PREV_AND_NEXT:
				case iFoldCONTINUED_TO_NEXT:
				case iFoldCONTINUED_FROM_PREV:
				return (ARCHIVE_FATAL);
					rbytes = 0;
					break;
				default:
					break;
				}
			}
			continue;
		}
		while (cbytes > 0) {
			(void)cab_read_ahead_cfdata(a, &avail);
			if (avail <= 0)
				return (ARCHIVE_FATAL);
			if (avail > cbytes)
				avail = cbytes;
			if (cab_minimum_consume_cfdata(a, avail) < 0)
				return (ARCHIVE_FATAL);
			cbytes -= avail;
		}
	}
	return (consumed_bytes);
}

/*
 * Consume CFDATA as much as we have already gotten and
 * compute the sum of CFDATA.
 */
static int64_t
cab_minimum_consume_cfdata(struct archive_read *a, int64_t consumed_bytes)
{
	struct cab *cab = (struct cab *)(a->format->data);
	struct cfdata *cfdata;
	int64_t cbytes, rbytes;
	int err;

	cfdata = cab->entry_cfdata;
	rbytes = consumed_bytes;
	if (cab->entry_cffolder->comptype == COMPTYPE_NONE) {
		if (consumed_bytes < cfdata->unconsumed)
			cbytes = consumed_bytes;
		else
			cbytes = cfdata->unconsumed;
		rbytes -= cbytes; 
		cfdata->read_offset += cbytes;
		cfdata->uncompressed_bytes_remaining -= cbytes;
		cfdata->unconsumed -= cbytes;
	} else {
		cbytes = cfdata->uncompressed_avail - cfdata->read_offset;
		if (cbytes > 0) {
			if (consumed_bytes < cbytes)
				cbytes = consumed_bytes;
			rbytes -= cbytes;
			cfdata->read_offset += cbytes;
			cfdata->uncompressed_bytes_remaining -= cbytes;
		}

		if (cfdata->unconsumed) {
			cbytes = cfdata->unconsumed;
			cfdata->unconsumed = 0;
		} else
			cbytes = 0;
	}
	if (cbytes) {
		/* Compute the sum. */
		cab_checksum_update(a, cbytes);

		/* Consume as much as the compressor actually used. */
		__archive_read_consume(a, cbytes);
		cab->cab_offset += cbytes;
		cfdata->compressed_bytes_remaining -= cbytes;
		if (cfdata->compressed_bytes_remaining == 0) {
			err = cab_checksum_finish(a);
			if (err < 0)
				return (err);
		}
	}
	return (rbytes);
}

/*
 * Returns ARCHIVE_OK if successful, ARCHIVE_FATAL otherwise, sets
 * cab->end_of_entry if it consumes all of the data.
 */
static int
cab_read_data(struct archive_read *a, const void **buff,
    size_t *size, int64_t *offset)
{
	struct cab *cab = (struct cab *)(a->format->data);
	ssize_t bytes_avail;

	if (cab->entry_bytes_remaining == 0) {
		*buff = NULL;
		*size = 0;
		*offset = cab->entry_offset;
		cab->end_of_entry = 1;
		return (ARCHIVE_OK);
	}

	*buff = cab_read_ahead_cfdata(a, &bytes_avail);
	if (bytes_avail <= 0) {
		*buff = NULL;
		*size = 0;
		*offset = 0;
		return (bytes_avail);
	}
	if (bytes_avail > cab->entry_bytes_remaining)
		bytes_avail = cab->entry_bytes_remaining;

	*size = bytes_avail;
	*offset = cab->entry_offset;
	cab->entry_offset += bytes_avail;
	cab->entry_bytes_remaining -= bytes_avail;
	if (cab->entry_bytes_remaining == 0)
		cab->end_of_entry = 1;
	cab->entry_unconsumed = bytes_avail;
	return (ARCHIVE_OK);
}

static int
archive_read_format_cab_read_data_skip(struct archive_read *a)
{
	struct cab *cab;
	int64_t bytes_skipped;
	int r;

	cab = (struct cab *)(a->format->data);

	if (cab->end_of_archive)
		return (ARCHIVE_EOF);

	if (cab->entry_unconsumed) {
		/* Consume as much as the compressor actually used. */
		r = cab_consume_cfdata(a, cab->entry_unconsumed);
		cab->entry_unconsumed = 0;
		if (r < 0)
			return (r);
	} else if (cab->entry_cfdata == NULL) {
		r = cab_next_cfdata(a);
		if (r < 0)
			return (r);
	}

	/* if we've already read to end of data, we're done. */
	if (cab->end_of_entry_cleanup)
		return (ARCHIVE_OK);

	/*
	 * If the length is at the beginning, we can skip the
	 * compressed data much more quickly.
	 */
	bytes_skipped = cab_consume_cfdata(a, cab->entry_bytes_remaining);
	if (bytes_skipped < 0)
		return (ARCHIVE_FATAL);

	/* This entry is finished and done. */
	cab->end_of_entry_cleanup = cab->end_of_entry = 1;
	return (ARCHIVE_OK);
}

static int
archive_read_format_cab_cleanup(struct archive_read *a)
{
	struct cab *cab = (struct cab *)(a->format->data);
	struct cfheader *hd = &cab->cfheader;
	int i;

	if (hd->folder_array != NULL) {
		for (i = 0; i < hd->folder_count; i++)
			free(hd->folder_array[i].cfdata.memimage);
		free(hd->folder_array);
	}
	if (hd->file_array != NULL) {
		for (i = 0; i < cab->cfheader.file_count; i++)
			archive_string_free(&(hd->file_array[i].pathname));
		free(hd->file_array);
	}
#ifdef HAVE_ZLIB_H
	if (cab->stream_valid)
		inflateEnd(&cab->stream);
#endif
	archive_wstring_free(&cab->ws);
	archive_string_free(&cab->mbs);
	free(cab->uncompressed_buffer);
	free(cab);
	(a->format->data) = NULL;
	return (ARCHIVE_OK);
}

/* Convert an MSDOS-style date/time into Unix-style time. */
static time_t
cab_dos_time(const unsigned char *p)
{
	int msTime, msDate;
	struct tm ts;

	msDate = archive_le16dec(p);
	msTime = archive_le16dec(p+2);

	memset(&ts, 0, sizeof(ts));
	ts.tm_year = ((msDate >> 9) & 0x7f) + 80;   /* Years since 1900. */
	ts.tm_mon = ((msDate >> 5) & 0x0f) - 1;     /* Month number.     */
	ts.tm_mday = msDate & 0x1f;		    /* Day of month.     */
	ts.tm_hour = (msTime >> 11) & 0x1f;
	ts.tm_min = (msTime >> 5) & 0x3f;
	ts.tm_sec = (msTime << 1) & 0x3e;
	ts.tm_isdst = -1;
	return (mktime(&ts));
}

