/*-
 * Copyright (c) 2003-2009 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_read_disk.c 189429 2009-03-06 04:35:31Z kientzle $");

#include "archive.h"
#include "archive_string.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_disk_private.h"

static int	_archive_read_free(struct archive *);
static int	_archive_read_close(struct archive *);
static int	_archive_read_data_block(struct archive *,
		    const void **, size_t *, int64_t *);
static int	_archive_read_next_header2(struct archive *,
		    struct archive_entry *);
#if ARCHIVE_VERSION_NUMBER < 3000000
static const char *trivial_lookup_gname(void *, gid_t gid);
static const char *trivial_lookup_uname(void *, uid_t uid);
#else
static const char *trivial_lookup_gname(void *, int64_t gid);
static const char *trivial_lookup_uname(void *, int64_t uid);
#endif
static struct archive_vtable *
archive_read_disk_vtable(void)
{
	static struct archive_vtable av;
	static int inited = 0;

	if (!inited) {
		av.archive_free = _archive_read_free;
		av.archive_close = _archive_read_close;
		av.archive_read_data_block = _archive_read_data_block;
		av.archive_read_next_header2 = _archive_read_next_header2;
	}
	return (&av);
}

#if ARCHIVE_VERSION_NUMBER < 3000000
const char *
archive_read_disk_gname(struct archive *_a, gid_t gid)
#else
const char *
archive_read_disk_gname(struct archive *_a, int64_t gid)
#endif
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	if (ARCHIVE_OK != __archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC,
		ARCHIVE_STATE_ANY, "archive_read_disk_gname"))
		return (NULL);
	if (a->lookup_gname == NULL)
		return (NULL);
	return ((*a->lookup_gname)(a->lookup_gname_data, gid));
}

#if ARCHIVE_VERSION_NUMBER < 3000000
const char *
archive_read_disk_uname(struct archive *_a, uid_t uid)
#else
const char *
archive_read_disk_uname(struct archive *_a, int64_t uid)
#endif
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	if (ARCHIVE_OK != __archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC,
		ARCHIVE_STATE_ANY, "archive_read_disk_uname"))
		return (NULL);
	if (a->lookup_uname == NULL)
		return (NULL);
	return ((*a->lookup_uname)(a->lookup_uname_data, uid));
}

#if ARCHIVE_VERSION_NUMBER < 3000000
int
archive_read_disk_set_gname_lookup(struct archive *_a,
    void *private_data,
    const char * (*lookup_gname)(void *private, gid_t gid),
    void (*cleanup_gname)(void *private))
#else
int
archive_read_disk_set_gname_lookup(struct archive *_a,
    void *private_data,
    const char * (*lookup_gname)(void *private, int64_t gid),
    void (*cleanup_gname)(void *private))
#endif
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	archive_check_magic(&a->archive, ARCHIVE_READ_DISK_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_read_disk_set_gname_lookup");

	if (a->cleanup_gname != NULL && a->lookup_gname_data != NULL)
		(a->cleanup_gname)(a->lookup_gname_data);

	a->lookup_gname = lookup_gname;
	a->cleanup_gname = cleanup_gname;
	a->lookup_gname_data = private_data;
	return (ARCHIVE_OK);
}

#if ARCHIVE_VERSION_NUMBER < 3000000
int
archive_read_disk_set_uname_lookup(struct archive *_a,
    void *private_data,
    const char * (*lookup_uname)(void *private, uid_t uid),
    void (*cleanup_uname)(void *private))
#else
int
archive_read_disk_set_uname_lookup(struct archive *_a,
    void *private_data,
    const char * (*lookup_uname)(void *private, int64_t uid),
    void (*cleanup_uname)(void *private))
#endif
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	archive_check_magic(&a->archive, ARCHIVE_READ_DISK_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_read_disk_set_uname_lookup");

	if (a->cleanup_uname != NULL && a->lookup_uname_data != NULL)
		(a->cleanup_uname)(a->lookup_uname_data);

	a->lookup_uname = lookup_uname;
	a->cleanup_uname = cleanup_uname;
	a->lookup_uname_data = private_data;
	return (ARCHIVE_OK);
}

/*
 * Create a new archive_read_disk object and initialize it with global state.
 */
struct archive *
archive_read_disk_new(void)
{
	struct archive_read_disk *a;

	a = (struct archive_read_disk *)malloc(sizeof(*a));
	if (a == NULL)
		return (NULL);
	memset(a, 0, sizeof(*a));
	a->archive.magic = ARCHIVE_READ_DISK_MAGIC;
	/* We're ready to write a header immediately. */
	a->archive.state = ARCHIVE_STATE_HEADER;
	a->archive.vtable = archive_read_disk_vtable();
	a->lookup_uname = trivial_lookup_uname;
	a->lookup_gname = trivial_lookup_gname;
	return (&a->archive);
}

static int
_archive_read_free(struct archive *_a)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;

	if (_a == NULL)
		return (ARCHIVE_OK);
	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC,
	    ARCHIVE_STATE_ANY | ARCHIVE_STATE_FATAL, "archive_read_free");
	if (a->cleanup_gname != NULL && a->lookup_gname_data != NULL)
		(a->cleanup_gname)(a->lookup_gname_data);
	if (a->cleanup_uname != NULL && a->lookup_uname_data != NULL)
		(a->cleanup_uname)(a->lookup_uname_data);
	archive_string_free(&a->archive.error_string);
	a->archive.magic = 0;
	free(a);
	return (ARCHIVE_OK);
}

static int
_archive_read_close(struct archive *_a)
{
	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC,
	    ARCHIVE_STATE_ANY | ARCHIVE_STATE_FATAL, "archive_read_close");
	(void)_a; /* UNUSED */
	return (ARCHIVE_OK);
}

int
archive_read_disk_set_symlink_logical(struct archive *_a)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_read_disk_set_symlink_logical");
	a->symlink_mode = 'L';
	a->follow_symlinks = 1;
	return (ARCHIVE_OK);
}

int
archive_read_disk_set_symlink_physical(struct archive *_a)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_read_disk_set_symlink_physical");
	a->symlink_mode = 'P';
	a->follow_symlinks = 0;
	return (ARCHIVE_OK);
}

int
archive_read_disk_set_symlink_hybrid(struct archive *_a)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_read_disk_set_symlink_hybrid");
	a->symlink_mode = 'H';
	a->follow_symlinks = 1; /* Follow symlinks initially. */
	return (ARCHIVE_OK);
}

/*
 * Trivial implementations of gname/uname lookup functions.
 * These are normally overridden by the client, but these stub
 * versions ensure that we always have something that works.
 */
#if ARCHIVE_VERSION_NUMBER < 3000000
static const char *
trivial_lookup_gname(void *private_data, gid_t gid)
#else
static const char *
trivial_lookup_gname(void *private_data, int64_t gid)
#endif
{
	(void)private_data; /* UNUSED */
	(void)gid; /* UNUSED */
	return (NULL);
}

#if ARCHIVE_VERSION_NUMBER < 3000000
static const char *
trivial_lookup_uname(void *private_data, uid_t uid)
#else
static const char *
trivial_lookup_uname(void *private_data, int64_t uid)
#endif
{
	(void)private_data; /* UNUSED */
	(void)uid; /* UNUSED */
	return (NULL);
}

static int
_archive_read_data_block(struct archive *_a, const void **buff,
    size_t *size, int64_t *offset)
{
	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC, ARCHIVE_STATE_DATA,
	    "archive_read_data_block");

	(void)buff; /* UNUSED */
	(void)size; /* UNUSED */
	(void)offset; /* UNUSED */
	/* Not implemented yet. */
	return (ARCHIVE_FAILED);
}

static int
_archive_read_next_header2(struct archive *_a, struct archive_entry *entry)
{
	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC,
	    ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
	    "archive_read_next_header2");

	(void)entry; /* UNUSED */
	/* Not implemented yet. */
	return (ARCHIVE_FAILED);
}
