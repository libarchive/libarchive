/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
 * Copyright (c) 2008 Joerg Sonnenberger
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
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_write_private.h"

struct mtree_writer {
	struct archive_entry *entry;
	struct archive_string buf;
	int first;
	int keys;
#define	F_CKSUM		0x00000001		/* check sum */
#define	F_DEV		0x00000002		/* device type */
#define	F_DONE		0x00000004		/* directory done */
#define	F_FLAGS		0x00000008		/* file flags */
#define	F_GID		0x00000010		/* gid */
#define	F_GNAME		0x00000020		/* group name */
#define	F_IGN		0x00000040		/* ignore */
#define	F_MAGIC		0x00000080		/* name has magic chars */
#define	F_MD5		0x00000100		/* MD5 digest */
#define	F_MODE		0x00000200		/* mode */
#define	F_NLINK		0x00000400		/* number of links */
#define	F_NOCHANGE 	0x00000800		/* If owner/mode "wrong", do
						 * not change */
#define	F_OPT		0x00001000		/* existence optional */
#define	F_RMD160 	0x00002000		/* RIPEMD160 digest */
#define	F_SHA1		0x00004000		/* SHA-1 digest */
#define	F_SIZE		0x00008000		/* size */
#define	F_SLINK		0x00010000		/* symbolic link */
#define	F_TAGS		0x00020000		/* tags */
#define	F_TIME		0x00040000		/* modification time */
#define	F_TYPE		0x00080000		/* file type */
#define	F_UID		0x00100000		/* uid */
#define	F_UNAME		0x00200000		/* user name */
#define	F_VISIT		0x00400000		/* file visited */
#define	F_SHA256	0x00800000		/* SHA-256 digest */
#define	F_SHA384	0x01000000		/* SHA-384 digest */
#define	F_SHA512	0x02000000		/* SHA-512 digest */
};

#define DEFAULT_KEYS	(F_DEV | F_FLAGS | F_GID | F_GNAME | F_SLINK | F_MODE\
			 | F_NLINK | F_SIZE | F_TIME | F_TYPE | F_UID\
			 | F_UNAME)

static int
mtree_safe_char(char c)
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return 1;
	if (c >= '0' && c <= '9')
		return 1;
	if (c == 35 || c == 61 || c == 92)
		return 0; /* #, = and \ are always quoted */
	
	if (c >= 33 && c <= 47) /* !"$%&'()*+,-./ */
		return 1;
	if (c >= 58 && c <= 64) /* :;<>?@ */
		return 1;
	if (c >= 91 && c <= 96) /* []^_` */
		return 1;
	if (c >= 123 && c <= 126) /* {|}~ */
		return 1;
	return 0;
}

static void
mtree_quote(struct mtree_writer *mtree, const char *str)
{
	const char *start;
	char buf[4];
	unsigned char c;

	for (start = str; *str != '\0'; ++str) {
		if (mtree_safe_char(*str))
			continue;
		if (start != str)
			archive_strncat(&mtree->buf, start, str - start);
		c = (unsigned char)*str;
		buf[0] = '\\';
		buf[1] = (c / 64) + '0';
		buf[2] = (c / 8 % 8) + '0';
		buf[3] = (c % 8) + '0';
		archive_strncat(&mtree->buf, buf, 4);
		start = str + 1;
	}

	if (start != str)
		archive_strncat(&mtree->buf, start, str - start);
}

static int
archive_write_mtree_header(struct archive_write *a,
    struct archive_entry *entry)
{
	struct mtree_writer *mtree= a->format_data;
	const char *path;

	mtree->entry = archive_entry_clone(entry);
	path = archive_entry_pathname(mtree->entry);

	if (mtree->first) {
		mtree->first = 0;
		archive_strcat(&mtree->buf, "#mtree\n");
	}

	mtree_quote(mtree, path);

	return (ARCHIVE_OK);
}

static int
archive_write_mtree_finish_entry(struct archive_write *a)
{
	struct mtree_writer *mtree = a->format_data;
	struct archive_entry *entry;
	const char *name;
	int ret;

	entry = mtree->entry;
	if (entry == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "Finished entry without being open first.");
		return (ARCHIVE_FATAL);
	}
	mtree->entry = NULL;

	if ((mtree->keys & F_NLINK) != 0 &&
	    archive_entry_nlink(entry) != 1 && 
	    archive_entry_filetype(entry) != AE_IFDIR)
		archive_string_sprintf(&mtree->buf,
		    " nlink=%u", archive_entry_nlink(entry));

	if ((mtree->keys & F_GNAME) != 0 &&
	    (name = archive_entry_gname(entry)) != NULL) {
		archive_strcat(&mtree->buf, " gname=");
		mtree_quote(mtree, name);
	}
	if ((mtree->keys & F_UNAME) != 0 &&
	    (name = archive_entry_uname(entry)) != NULL) {
		archive_strcat(&mtree->buf, " uname=");
		mtree_quote(mtree, name);
	}
	if ((mtree->keys & F_FLAGS) != 0 &&
	    (name = archive_entry_fflags_text(entry)) != NULL) {
		archive_strcat(&mtree->buf, " flags=");
		mtree_quote(mtree, name);
	}
	if ((mtree->keys & F_TIME) != 0)
		archive_string_sprintf(&mtree->buf, " time=%jd",
		    (intmax_t)archive_entry_mtime(entry));
	if ((mtree->keys & F_MODE) != 0)
		archive_string_sprintf(&mtree->buf, " mode=%o",
		    archive_entry_mode(entry) & 07777);
	if ((mtree->keys & F_GID) != 0)
		archive_string_sprintf(&mtree->buf, " gid=%jd",
		    (intmax_t)archive_entry_gid(entry));
	if ((mtree->keys & F_UID) != 0)
		archive_string_sprintf(&mtree->buf, " uid=%jd",
		    (intmax_t)archive_entry_uid(entry));

	switch (archive_entry_filetype(entry)) {
	case AE_IFLNK:
		if ((mtree->keys & F_TYPE) != 0)
			archive_strcat(&mtree->buf, " type=link");
		if ((mtree->keys & F_SLINK) != 0) {
			archive_strcat(&mtree->buf, " link=");
			mtree_quote(mtree, archive_entry_symlink(entry));
		}
		break;
	case AE_IFSOCK:
		if ((mtree->keys & F_TYPE) != 0)
			archive_strcat(&mtree->buf, " type=socket");
		break;
	case AE_IFCHR:
		if ((mtree->keys & F_TYPE) != 0)
			archive_strcat(&mtree->buf, " type=char");
		if ((mtree->keys & F_DEV) != 0) {
			archive_string_sprintf(&mtree->buf,
			    " device=native,%d,%d",
			    archive_entry_rdevmajor(entry),
			    archive_entry_rdevminor(entry));
		}
		break;
	case AE_IFBLK:
		if ((mtree->keys & F_TYPE) != 0)
			archive_strcat(&mtree->buf, " type=block");
		if ((mtree->keys & F_DEV) != 0) {
			archive_string_sprintf(&mtree->buf,
			    " device=native,%d,%d",
			    archive_entry_rdevmajor(entry),
			    archive_entry_rdevminor(entry));
		}
		break;
	case AE_IFDIR:
		if ((mtree->keys & F_TYPE) != 0)
			archive_strcat(&mtree->buf, " type=dir");
		break;
	case AE_IFIFO:
		if ((mtree->keys & F_TYPE) != 0)
			archive_strcat(&mtree->buf, " type=fifo");
		break;
	case AE_IFREG:
	default:	/* Handle unknown file types as regular files. */
		if ((mtree->keys & F_TYPE) != 0)
			archive_strcat(&mtree->buf, " type=file");
		if ((mtree->keys & F_SIZE) != 0)
			archive_string_sprintf(&mtree->buf, " size=%jd",
			    (intmax_t)archive_entry_size(entry));
		break;
	}
	archive_strcat(&mtree->buf, "\n");

	archive_entry_free(entry);

	if (mtree->buf.length > 32768) {
		ret = (a->compressor.write)(a, mtree->buf.s, mtree->buf.length);
		archive_string_empty(&mtree->buf);
	} else
		ret = ARCHIVE_OK;

	return (ret == ARCHIVE_OK ? ret : ARCHIVE_FATAL);
}

static int
archive_write_mtree_finish(struct archive_write *a)
{
	struct mtree_writer *mtree= a->format_data;

	archive_write_set_bytes_in_last_block(&a->archive, 1);

	return (a->compressor.write)(a, mtree->buf.s, mtree->buf.length);
}

static ssize_t
archive_write_mtree_data(struct archive_write *a, const void *buff, size_t n)
{
	(void)a; /* UNUSED */
	(void)buff; /* UNUSED */
	return n;
}

static int
archive_write_mtree_destroy(struct archive_write *a)
{
	struct mtree_writer *mtree= a->format_data;

	if (mtree == NULL)
		return (ARCHIVE_OK);

	archive_entry_free(mtree->entry);
	archive_string_free(&mtree->buf);
	free(mtree);
	a->format_data = NULL;
	return (ARCHIVE_OK);
}

static int
archive_write_mtree_options(struct archive_write *a, const char *key,
    const char *value)
{
	struct mtree_writer *mtree= a->format_data;
	int keybit = 0;

	switch (key[0]) {
	case 'a':
		if (strcmp(key, "all") == 0)
			keybit = -1;
		break;
	case 'd':
		if (strcmp(key, "device") == 0)
			keybit = F_DEV;
		break;
	case 'f':
		if (strcmp(key, "flags") == 0)
			keybit = F_FLAGS;
		break;
	case 'g':
		if (strcmp(key, "gid") == 0)
			keybit = F_GID;
		else if (strcmp(key, "gname") == 0)
			keybit = F_GNAME;
		break;
	case 'l':
		if (strcmp(key, "link") == 0)
			keybit = F_SLINK;
		break;
	case 'm':
		if (strcmp(key, "mode") == 0)
			keybit = F_MODE;
		break;
	case 'n':
		if (strcmp(key, "nlink") == 0)
			keybit = F_NLINK;
		break;
	case 's':
		if (strcmp(key, "size") == 0)
			keybit = F_SIZE;
		break;
	case 't':
		if (strcmp(key, "time") == 0)
			keybit = F_TIME;
		else if (strcmp(key, "type") == 0)
			keybit = F_TYPE;
		break;
	case 'u':
		if (strcmp(key, "uid") == 0)
			keybit = F_UID;
		else if (strcmp(key, "uname") == 0)
			keybit = F_UNAME;
		break;
	}
	if (keybit != 0) {
		if (value != NULL)
			mtree->keys |= keybit;
		else
			mtree->keys &= ~keybit;
		return (ARCHIVE_OK);
	}

	return (ARCHIVE_WARN);
}

int
archive_write_set_format_mtree(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct mtree_writer *mtree;

	if (a->format_destroy != NULL)
		(a->format_destroy)(a);

	if ((mtree = malloc(sizeof(*mtree))) == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate mtree data");
		return (ARCHIVE_FATAL);
	}

	mtree->entry = NULL;
	mtree->first = 1;
	mtree->keys = DEFAULT_KEYS;
	archive_string_init(&mtree->buf);
	a->format_data = mtree;
	a->format_destroy = archive_write_mtree_destroy;

	a->pad_uncompressed = 0;
	a->format_name = "mtree";
	a->format_options = archive_write_mtree_options;
	a->format_write_header = archive_write_mtree_header;
	a->format_finish = archive_write_mtree_finish;
	a->format_write_data = archive_write_mtree_data;
	a->format_finish_entry = archive_write_mtree_finish_entry;
	a->archive.archive_format = ARCHIVE_FORMAT_MTREE;
	a->archive.archive_format_name = "mtree";

	return (ARCHIVE_OK);
}
