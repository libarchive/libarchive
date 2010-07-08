/*-
 * Copyright (c) 2003-2009 Tim Kientzle
 * Copyright (c) 2010 Michihiro NAKAJIMA
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

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#ifdef HAVE_LINUX_MAGIC_H
#include <linux/magic.h>
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "archive.h"
#include "archive_string.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_disk_private.h"

#ifndef O_BINARY
#define O_BINARY	0
#endif
#ifndef IO_REPARSE_TAG_SYMLINK
/* Old SDKs do not provide IO_REPARSE_TAG_SYMLINK */
#define	IO_REPARSE_TAG_SYMLINK 0xA000000CL
#endif

/*-
 * This is a new directory-walking system that addresses a number
 * of problems I've had with fts(3).  In particular, it has no
 * pathname-length limits (other than the size of 'int'), handles
 * deep logical traversals, uses considerably less memory, and has
 * an opaque interface (easier to modify in the future).
 *
 * Internally, it keeps a single list of "tree_entry" items that
 * represent filesystem objects that require further attention.
 * Non-directories are not kept in memory: they are pulled from
 * readdir(), returned to the client, then freed as soon as possible.
 * Any directory entry to be traversed gets pushed onto the stack.
 *
 * There is surprisingly little information that needs to be kept for
 * each item on the stack.  Just the name, depth (represented here as the
 * string length of the parent directory's pathname), and some markers
 * indicating how to get back to the parent (via chdir("..") for a
 * regular dir or via fchdir(2) for a symlink).
 */
/*
 * TODO:
 *    1) Loop checking.
 *    3) Arbitrary logical traversals by closing/reopening intermediate fds.
 */

struct tree_entry {
	int depth;
	struct tree_entry *next;
	struct tree_entry *parent;
	struct archive_string name;
	size_t dirname_length;
	dev_t dev;
	ino_t ino;
	int flags;
	/* How to return back to the parent of a symlink. */
#ifdef HAVE_FCHDIR
	int symlink_parent_fd;
#elif !defined(_WIN32) || defined(__CYGWIN__)
#error fchdir function required.
#endif
};

struct filesystem {
	int64_t		dev;
	int		synthetic;
	int		remote;
};

/* Definitions for tree_entry.flags bitmap. */
#define	isDir 1 /* This entry is a regular directory. */
#define	isDirLink 2 /* This entry is a symbolic link to a directory. */
#define	needsFirstVisit 4 /* This is an initial entry. */
#define	needsDescent 8 /* This entry needs to be previsited. */
#define	needsOpen 16 /* This is a directory that needs to be opened. */
#define	needsAscent 32 /* This entry needs to be postvisited. */

/*
 * On Windows, "first visit" is handled as a pattern to be handed to
 * _findfirst().  This is consistent with Windows conventions that
 * file patterns are handled within the application.  On Posix,
 * "first visit" is just returned to the client.
 */

/*
 * Local data for this package.
 */
struct tree {
	struct tree_entry	*stack;
	struct tree_entry	*current;
#if defined(HAVE_WINDOWS_H) && !defined(__CYGWIN__)
	HANDLE d;
#define	INVALID_DIR_HANDLE INVALID_HANDLE_VALUE
	WIN32_FIND_DATA _findData;
	WIN32_FIND_DATA *findData;
#else
	DIR	*d;
#define	INVALID_DIR_HANDLE NULL
	struct dirent *de;
#endif
	int	 flags;
	int	 visit_type;
	int	 tree_errno; /* Error code from last failed operation. */

	/* Dynamically-sized buffer for holding path */
	struct archive_string path;

	const char *basename; /* Last path element */
	size_t	 dirname_length; /* Leading dir length */

	int	 depth;
	int	 openCount;
	int	 maxOpenCount;

#if defined(_WIN32) && !defined(__CYGWIN__)
	BY_HANDLE_FILE_INFORMATION	lst;
	BY_HANDLE_FILE_INFORMATION	st;
#else
	struct stat	lst;
	struct stat	st;
#endif
	int	 descend;

	char	 symlink_mode;
	struct filesystem *current_filesystem;
	struct filesystem *filesystem_table;
	int		current_filesystem_id;
	int		max_filesystem_id;
	int		allocated_filesytem;
};

#if defined(_WIN32) && !defined(__CYGWIN__)
#define dev_no(st)	st->dwVolumeSerialNumber
#else
#define dev_no(st)	st->st_dev
#endif

/* Definitions for tree.flags bitmap. */
#define	hasStat 16  /* The st entry is valid. */
#define	hasLstat 32 /* The lst entry is valid. */
#define	hasFileInfo 64 /* The Windows fileInfo entry is valid. */

#if defined(_WIN32) && !defined(__CYGWIN__)
static int
tree_dir_next_windows(struct tree *t, const char *pattern);
#else
static int
tree_dir_next_posix(struct tree *t);
#endif

#ifdef HAVE_DIRENT_D_NAMLEN
/* BSD extension; avoids need for a strlen() call. */
#define	D_NAMELEN(dp)	(dp)->d_namlen
#else
#define	D_NAMELEN(dp)	(strlen((dp)->d_name))
#endif

/* Initiate/terminate a tree traversal. */
static struct tree *tree_open(const char *);
static void tree_close(struct tree *);
static void tree_push(struct tree *, const char *);

/*
 * tree_next() returns Zero if there is no next entry, non-zero if
 * there is.  Note that directories are visited three times.
 * Directories are always visited first as part of enumerating their
 * parent; that is a "regular" visit.  If tree_descend() is invoked at
 * that time, the directory is added to a work list and will
 * subsequently be visited two more times: once just after descending
 * into the directory ("postdescent") and again just after ascending
 * back to the parent ("postascent").
 *
 * TREE_ERROR_DIR is returned if the descent failed (because the
 * directory couldn't be opened, for instance).  This is returned
 * instead of TREE_POSTDESCENT/TREE_POSTASCENT.  TREE_ERROR_DIR is not a
 * fatal error, but it does imply that the relevant subtree won't be
 * visited.  TREE_ERROR_FATAL is returned for an error that left the
 * traversal completely hosed.  Right now, this is only returned for
 * chdir() failures during ascent.
 */
#define	TREE_REGULAR	1
#define	TREE_POSTDESCENT	2
#define	TREE_POSTASCENT	3
#define	TREE_ERROR_DIR	-1
#define	TREE_ERROR_FATAL -2

static int tree_next(struct tree *);

/*
 * Return information about the current entry.
 */

/*
 * The current full pathname, length of the full pathname, and a name
 * that can be used to access the file.  Because tree does use chdir
 * extensively, the access path is almost never the same as the full
 * current path.
 *
 * TODO: Flesh out this interface to provide other information.  In
 * particular, Windows can provide file size, mode, and some permission
 * information without invoking stat() at all.
 *
 * TODO: On platforms that support it, use openat()-style operations
 * to eliminate the chdir() operations entirely while still supporting
 * arbitrarily deep traversals.  This makes access_path troublesome to
 * support, of course, which means we'll need a rich enough interface
 * that clients can function without it.  (In particular, we'll need
 * tree_current_open() that returns an open file descriptor.)
 *
 */
static const char *tree_current_path(struct tree *);
static const char *tree_current_access_path(struct tree *);

/*
 * Request the lstat() or stat() data for the current path.  Since the
 * tree package needs to do some of this anyway, and caches the
 * results, you should take advantage of it here if you need it rather
 * than make a redundant stat() or lstat() call of your own.
 */
#if defined(_WIN32) && !defined(__CYGWIN__)
static const BY_HANDLE_FILE_INFORMATION *tree_current_stat(struct tree *);
static const BY_HANDLE_FILE_INFORMATION *tree_current_lstat(struct tree *);
#else
static const struct stat *tree_current_stat(struct tree *);
static const struct stat *tree_current_lstat(struct tree *);
#endif

/* The following functions use tricks to avoid a certain number of
 * stat()/lstat() calls. */
/* "is_physical_dir" is equivalent to S_ISDIR(tree_current_lstat()->st_mode) */
static int tree_current_is_physical_dir(struct tree *);
#if defined(_WIN32) && !defined(__CYGWIN__)
/* "is_physical_link" is equivalent to S_ISLNK(tree_current_lstat()->st_mode) */
static int tree_current_is_physical_link(struct tree *);
/* Instead of archive_entry_copy_stat for BY_HANDLE_FILE_INFORMATION */
static void tree_archive_entry_copy_bhfi(struct archive_entry *,
		    struct tree *, const BY_HANDLE_FILE_INFORMATION *);
#endif
/* "is_dir" is equivalent to S_ISDIR(tree_current_stat()->st_mode) */
static int tree_current_is_dir(struct tree *);
static int update_filesystem(struct archive_read_disk *a,
		    int64_t dev);
static int setup_current_filesystem(struct archive_read_disk *);

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
	a->archive.state = ARCHIVE_STATE_NEW;
	a->archive.vtable = archive_read_disk_vtable();
	a->lookup_uname = trivial_lookup_uname;
	a->lookup_gname = trivial_lookup_gname;
	a->entry_fd = -1;
	return (&a->archive);
}

static int
_archive_read_free(struct archive *_a)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	int r;

	if (_a == NULL)
		return (ARCHIVE_OK);
	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC,
	    ARCHIVE_STATE_ANY | ARCHIVE_STATE_FATAL, "archive_read_free");

	if (a->archive.state != ARCHIVE_STATE_CLOSED)
		r = _archive_read_close(&a->archive);
	else
		r = ARCHIVE_OK;

	if (a->cleanup_gname != NULL && a->lookup_gname_data != NULL)
		(a->cleanup_gname)(a->lookup_gname_data);
	if (a->cleanup_uname != NULL && a->lookup_uname_data != NULL)
		(a->cleanup_uname)(a->lookup_uname_data);
	archive_string_free(&a->archive.error_string);
	free(a->entry_buff);
	a->archive.magic = 0;
	free(a);
	return (r);
}

static int
_archive_read_close(struct archive *_a)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;

	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC,
	    ARCHIVE_STATE_ANY | ARCHIVE_STATE_FATAL, "archive_read_close");

	if (a->archive.state != ARCHIVE_STATE_FATAL)
		a->archive.state = ARCHIVE_STATE_CLOSED;

	if (a->tree != NULL) {
		tree_close(a->tree);
		a->tree = NULL;
	}
	if (a->entry_fd >= 0) {
		close(a->entry_fd);
		a->entry_fd = -1;
	}

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
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	int r;
	ssize_t bytes;
	size_t buffbytes;

	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC, ARCHIVE_STATE_DATA,
	    "archive_read_data_block");

	if (a->entry_eof || a->entry_remaining_bytes <= 0) {
		r = ARCHIVE_EOF;
		goto abort_read_data;
	}

	if (a->entry_fd < 0) {
		a->entry_fd = open(tree_current_access_path(a->tree),
		    O_RDONLY | O_BINARY);
		if (a->entry_fd < 0) {
			archive_set_error(&a->archive, errno,
			    "Couldn't open %s", tree_current_path(a->tree));
			r = ARCHIVE_FAILED;
			goto abort_read_data;
		}
	}
	if (a->entry_buff == NULL) {
		a->entry_buff = malloc(1024 * 64);
		if (a->entry_buff == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Couldn't allocate memory");
			r = ARCHIVE_FATAL;
			a->archive.state = ARCHIVE_STATE_FATAL;
			goto abort_read_data;
		}
		a->entry_buff_size = 1024 * 64;
	}

	buffbytes = a->entry_buff_size;
	if ((int64_t)buffbytes > a->entry_remaining_bytes)
		buffbytes = (size_t)a->entry_remaining_bytes;
	bytes = read(a->entry_fd, a->entry_buff, buffbytes);
	if (bytes < 0) {
		archive_set_error(&a->archive, errno,
		    "Read error");
		r = ARCHIVE_FATAL;
		a->archive.state = ARCHIVE_STATE_FATAL;
		goto abort_read_data;
	}
	if (bytes == 0) {
		/* Get EOF */
		a->entry_eof = 1;
		r = ARCHIVE_EOF;
		goto abort_read_data;
	}
	*buff = a->entry_buff;
	*size = bytes;
	*offset = a->entry_total;
	a->entry_total += bytes;
	a->entry_remaining_bytes -= bytes;
	if (a->entry_remaining_bytes == 0) {
		/* Close the current file descriptor */
		close(a->entry_fd);
		a->entry_fd = -1;
		a->entry_eof = 1;
	}
	return (ARCHIVE_OK);

abort_read_data:
	*buff = NULL;
	*size = 0;
	*offset = a->entry_total;
	if (a->entry_fd >= 0) {
		/* Close the current file descriptor */
		close(a->entry_fd);
		a->entry_fd = -1;
	}
	return (r);
}

static int
_archive_read_next_header2(struct archive *_a, struct archive_entry *entry)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	struct tree *t;
#if defined(_WIN32) && !defined(__CYGWIN__)
	const BY_HANDLE_FILE_INFORMATION *st;
	const BY_HANDLE_FILE_INFORMATION *lst;
#else
	const struct stat *st; /* info to use for this entry */
	const struct stat *lst;/* lstat() information */
#endif
	int descend, r;

	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC,
	    ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
	    "archive_read_next_header2");

	if (a->entry_fd >= 0) {
		close(a->entry_fd);
		a->entry_fd = -1;
	}
	t = a->tree;
	st = NULL;
	lst = NULL;
	do {
		switch (tree_next(t)) {
		case TREE_ERROR_FATAL:
			archive_set_error(&a->archive, t->tree_errno,
			    "%s: Unable to continue traversing directory tree",
			    tree_current_path(t));
			a->archive.state = ARCHIVE_STATE_FATAL;
			return (ARCHIVE_FATAL);
		case TREE_ERROR_DIR:
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "%s: Couldn't visit directory",
			    tree_current_path(t));
			return (ARCHIVE_FAILED);
		case 0:
			return (ARCHIVE_EOF);
		case TREE_POSTDESCENT:
		case TREE_POSTASCENT:
			break;
		case TREE_REGULAR:
			lst = tree_current_lstat(t);
			if (lst == NULL) {
				archive_set_error(&a->archive, errno,
				    "%s: Cannot stat",
				    tree_current_path(t));
				return (ARCHIVE_FAILED);
			}
			break;
		}	
	} while (lst == NULL);

	/*
	 * Distinguish 'L'/'P'/'H' symlink following.
	 */
	switch(t->symlink_mode) {
	case 'H':
		/* 'H': After the first item, rest like 'P'. */
		t->symlink_mode = 'P';
		/* 'H': First item (from command line) like 'L'. */
		/* FALLTHROUGH */
	case 'L':
		/* 'L': Do descend through a symlink to dir. */
		descend = tree_current_is_dir(t);
		/* 'L': Follow symlinks to files. */
		a->symlink_mode = 'L';
		a->follow_symlinks = 1;
		/* 'L': Archive symlinks as targets, if we can. */
		st = tree_current_stat(t);
		if (st != NULL)
			break;
		/* If stat fails, we have a broken symlink;
		 * in that case, don't follow the link. */
		/* FALLTHROUGH */
	default:
		/* 'P': Don't descend through a symlink to dir. */
		descend = tree_current_is_physical_dir(t);
		/* 'P': Don't follow symlinks to files. */
		a->symlink_mode = 'P';
		a->follow_symlinks = 0;
		/* 'P': Archive symlinks as symlinks. */
		st = lst;
		break;
	}

	if (update_filesystem(a, dev_no(lst)) != ARCHIVE_OK) {
		a->archive.state = ARCHIVE_STATE_FATAL;
		return (ARCHIVE_FATAL);
	}
	t->descend = descend;

	archive_entry_set_pathname(entry, tree_current_path(t));
	archive_entry_copy_sourcepath(entry, tree_current_access_path(t));

	/* Populate the archive_entry with metadata from the disk. */
#if defined(_WIN32) && !defined(__CYGWIN__)
	tree_archive_entry_copy_bhfi(entry, t, st);
	r = archive_read_disk_entry_from_file(&(a->archive), entry, -1, NULL);
#else
	archive_entry_copy_stat(entry, st);
	r = archive_read_disk_entry_from_file(&(a->archive), entry, -1, st);
#endif

	/*
	 * EOF and FATAL are persistent at this layer.  By
	 * modifying the state, we guarantee that future calls to
	 * read a header or read data will fail.
	 */
	switch (r) {
	case ARCHIVE_EOF:
		a->archive.state = ARCHIVE_STATE_EOF;
		break;
	case ARCHIVE_OK:
	case ARCHIVE_WARN:
		a->entry_fd = -1;
		a->entry_total = 0;
		if (archive_entry_filetype(entry) == AE_IFREG) {
			a->entry_remaining_bytes = archive_entry_size(entry);
			a->entry_eof = (a->entry_remaining_bytes == 0)? 1: 0;
		} else {
			a->entry_remaining_bytes = 0;
			a->entry_eof = 1;
		}
		a->archive.state = ARCHIVE_STATE_DATA;
		break;
	case ARCHIVE_RETRY:
		break;
	case ARCHIVE_FATAL:
		a->archive.state = ARCHIVE_STATE_FATAL;
		break;
	}

	return (r);
}

/*
 * Called by the client to mark the directory just returned from
 * tree_next() as needing to be visited.
 */
int
archive_read_disk_descend(struct archive *_a)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	struct tree *t = a->tree;

	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC, ARCHIVE_STATE_DATA,
	    "archive_read_disk_descend");

	if (t->visit_type != TREE_REGULAR || !t->descend) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Ignored the request descending the current object");
		return (ARCHIVE_WARN);
	}

	if (tree_current_is_physical_dir(t)) {
		tree_push(t, t->basename);
		t->stack->flags |= isDir;
	} else if (tree_current_is_dir(t)) {
		tree_push(t, t->basename);
		t->stack->flags |= isDirLink;
	}
	t->descend = 0;
	return (ARCHIVE_OK);
}

int
archive_read_disk_open(struct archive *_a, const char *pathname)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	struct tree *tree;

	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC, ARCHIVE_STATE_NEW,
	    "archive_read_disk_open");
	archive_clear_error(&a->archive);

	tree = tree_open(pathname);
	if (tree == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate tar data");
		a->archive.state = ARCHIVE_STATE_FATAL;
		return (ARCHIVE_FATAL);
	}
	tree->symlink_mode = a->symlink_mode;
	tree->current_filesystem_id = -1;
	a->tree = tree;
	a->archive.state = ARCHIVE_STATE_HEADER;
	a->entry_eof = 0;
	a->entry_fd = -1;

	return (ARCHIVE_OK);
}

/*
 * Return a current filesystem ID which is index of the filesystem entry
 * you've visited through archive_read_disk.
 */
int
archive_read_disk_current_filesystem(struct archive *_a)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;

	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC, ARCHIVE_STATE_DATA,
	    "archive_read_disk_current_filesystem");

	return (a->tree->current_filesystem_id);
}

static int
update_filesystem(struct archive_read_disk *a, int64_t dev)
{
	struct tree *t = a->tree;
	int i, fid;

	if (t->current_filesystem != NULL &&
	    t->current_filesystem->dev == dev)
		return (ARCHIVE_OK);

	for (i = 0; i < t->max_filesystem_id; i++) {
		if (t->filesystem_table[i].dev == dev) {
			/* There is the filesytem ID we've already generated. */
			t->current_filesystem_id = i;
			t->current_filesystem = &(t->filesystem_table[i]);
			return (ARCHIVE_OK);
		}
	}

	/*
	 * There is a new filesytem, we generate a new ID for.
	 */
	fid = t->max_filesystem_id++;
	if (t->max_filesystem_id > t->allocated_filesytem) {
		size_t s;

		s = t->max_filesystem_id * 2;
		t->filesystem_table = realloc(t->filesystem_table,
		    s * sizeof(*t->filesystem_table));
		if (t->filesystem_table == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate tar data");
			return (ARCHIVE_FATAL);
		}
		t->allocated_filesytem = s;
	}
	t->current_filesystem_id = fid;
	t->current_filesystem = &(t->filesystem_table[fid]);
	t->current_filesystem->dev = dev;
	return (setup_current_filesystem(a));
}

/*
 * Returns 1 if current filesystem is generated filesystem, 0 if it is not
 * or -1 if it is unknown.
 */
int
archive_read_disk_current_filesystem_is_synthetic(struct archive *_a)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;

	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC, ARCHIVE_STATE_DATA,
	    "archive_read_disk_current_filesystem");

	return (a->tree->current_filesystem->synthetic);
}

/*
 * Returns 1 if current filesystem is remote filesystem, 0 if it is not
 * or -1 if it is unknown.
 */
int
archive_read_disk_current_filesystem_is_remote(struct archive *_a)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;

	archive_check_magic(_a, ARCHIVE_READ_DISK_MAGIC, ARCHIVE_STATE_DATA,
	    "archive_read_disk_current_filesystem");

	return (a->tree->current_filesystem->remote);
}

#if defined(__FreeBSD__)

/*
 * Get conditions of synthetic and remote on FreeBSD.
 */
static int
setup_current_filesystem(struct archive_read_disk *a)
{
	struct tree *t = a->tree;
	struct statfs sfs;
	struct xvfsconf vfc;
	int r;

	t->current_filesystem->synthetic = -1;
	t->current_filesystem->remote = -1;
	r = statfs(tree_current_access_path(t), &sfs);
	if (r == -1) {
		archive_set_error(&a->archive, errno, "statfs failed");
		return (ARCHIVE_FAILED);
	}
	if (sfs.f_flags & MNT_LOCAL)
		t->current_filesystem->remote = 0;
	else
		t->current_filesystem->remote = 1;
	r = getvfsbyname(sfs.f_fstypename, &vfc);
	if (r == -1) {
		archive_set_error(&a->archive, errno, "getvfsbyname failed");
		return (ARCHIVE_FAILED);
	}
	if (vfc.vfc_flags & VFCF_SYNTHETIC)
		t->current_filesystem->synthetic = 1;
	else
		t->current_filesystem->synthetic = 0;
	return (ARCHIVE_OK);
}

#elif defined(HAVE_STATVFS) && defined(ST_LOCAL)

/*
 * Get conditions of synthetic and remote on NetBSD
 */
static int
setup_current_filesystem(struct archive_read_disk *a)
{
	struct tree *t = a->tree;
	struct statvfs sfs;
	int r;

	t->current_filesystem->synthetic = -1;
	r = statvfs(tree_current_access_path(t), &sfs);
	if (r == -1) {
		t->current_filesystem->remote = -1;
		archive_set_error(&a->archive, errno, "statfs failed");
		return (ARCHIVE_FAILED);
	}
	if (sfs.f_flag & ST_LOCAL)
		t->current_filesystem->remote = 0;
	else
		t->current_filesystem->remote = 1;
	return (ARCHIVE_OK);
}

#elif defined(HAVE_SYS_VFS_H) && defined(HAVE_LINUX_MAGIC_H)
#ifndef CIFS_SUPER_MAGIC
#define CIFS_SUPER_MAGIC 0xFF534D42
#endif
#ifndef DEVFS_SUPER_MAGIC
#define DEVFS_SUPER_MAGIC 0x1373
#endif

/*
 * Get conditions of synthetic and remote on Linux
 */
static int
setup_current_filesystem(struct archive_read_disk *a)
{
	struct tree *t = a->tree;
	struct statfs sfs;
	int r;

	r = statfs(tree_current_access_path(t), &sfs);
	if (r == -1) {
		t->current_filesystem->synthetic = -1;
		t->current_filesystem->remote = -1;
		archive_set_error(&a->archive, errno, "statfs failed");
		return (ARCHIVE_FAILED);
	}
	switch (sfs.f_type) {
	case AFS_SUPER_MAGIC:
	case CIFS_SUPER_MAGIC:
	case CODA_SUPER_MAGIC:
	case NCP_SUPER_MAGIC:/* NetWare */
	case NFS_SUPER_MAGIC:
	case SMB_SUPER_MAGIC:
		t->current_filesystem->remote = 1;
		t->current_filesystem->synthetic = 0;
		break;
	case DEVFS_SUPER_MAGIC:
	case PROC_SUPER_MAGIC:
	case USBDEVICE_SUPER_MAGIC:
		t->current_filesystem->remote = 0;
		t->current_filesystem->synthetic = 1;
		break;
	default:
		t->current_filesystem->remote = 0;
		t->current_filesystem->synthetic = 0;
		break;
	}
	return (ARCHIVE_OK);
}

#elif defined(_WIN32) && !defined(__CYGWIN__)

/*
 * Get conditions of synthetic and remote on Windows
 */
static int
setup_current_filesystem(struct archive_read_disk *a)
{
	struct tree *t = a->tree;
	char vol[256];

	t->current_filesystem->synthetic = -1;/* Not supported */
	if (!GetVolumePathName(tree_current_access_path(t), vol, sizeof(vol))) {
		t->current_filesystem->remote = -1;
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
                        "GetVolumePathName failed: %d", (int)GetLastError());
		return (ARCHIVE_FAILED);
	}
	switch (GetDriveType(vol)) {
	case DRIVE_UNKNOWN:
	case DRIVE_NO_ROOT_DIR:
		t->current_filesystem->remote = -1;
		break;
	case DRIVE_REMOTE:
		t->current_filesystem->remote = 1;
		break;
	default:
		t->current_filesystem->remote = 0;
		break;
	}

	return (ARCHIVE_OK);
}

#else

/*
 * Generic
 */
static int
setup_current_filesystem(struct archive_read_disk *a)
{
	struct tree *t = a->tree;
	t->current_filesystem->synthetic = -1;/* Not supported */
	t->current_filesystem->remote = -1;/* Not supported */
	return (ARCHIVE_OK);
}

#endif


/*
 * Add a directory path to the current stack.
 */
static void
tree_push(struct tree *t, const char *path)
{
	struct tree_entry *te;

	te = malloc(sizeof(*te));
	memset(te, 0, sizeof(*te));
	te->next = t->stack;
	te->parent = t->current;
	if (te->parent)
		te->depth = te->parent->depth + 1;
	t->stack = te;
	archive_string_init(&te->name);
#ifdef HAVE_FCHDIR
	te->symlink_parent_fd = -1;
#endif
	archive_strcpy(&te->name, path);
	te->flags = needsDescent | needsOpen | needsAscent;
	te->dirname_length = t->dirname_length;
}

/*
 * Append a name to the current dir path.
 */
static void
tree_append(struct tree *t, const char *name, size_t name_length)
{
	size_t size_needed;

	t->path.s[t->dirname_length] = '\0';
	t->path.length = t->dirname_length;
	/* Strip trailing '/' from name, unless entire name is "/". */
	while (name_length > 1 && name[name_length - 1] == '/')
		name_length--;

	/* Resize pathname buffer as needed. */
	size_needed = name_length + 1 + t->dirname_length;
	archive_string_ensure(&t->path, size_needed);
	/* Add a separating '/' if it's needed. */
	if (t->dirname_length > 0 && t->path.s[archive_strlen(&t->path)-1] != '/')
		archive_strappend_char(&t->path, '/');
	t->basename = t->path.s + archive_strlen(&t->path);
	archive_strncat(&t->path, name, name_length);
}

/*
 * Open a directory tree for traversal.
 */
static struct tree *
tree_open(const char *path)
{
#ifdef HAVE_FCHDIR
	struct tree *t;

	t = malloc(sizeof(*t));
	memset(t, 0, sizeof(*t));
	archive_string_init(&t->path);
	archive_string_ensure(&t->path, 31);
	/* First item is set up a lot like a symlink traversal. */
	tree_push(t, path);
	t->stack->flags = needsFirstVisit | isDirLink | needsAscent;
	t->stack->symlink_parent_fd = open(".", O_RDONLY);
	t->openCount++;
	t->d = INVALID_DIR_HANDLE;
	return (t);
#elif defined(_WIN32) && !defined(__CYGWIN__)
	struct tree *t;
	char *pathname = strdup(path), *p, *base;

	if (pathname == NULL)
		abort();
	for (p = pathname; *p != '\0'; ++p) {
		if (*p == '\\')
			*p = '/';
	}
	base = pathname;

	t = malloc(sizeof(*t));
	memset(t, 0, sizeof(*t));
	archive_string_init(&t->path);
	archive_string_ensure(&t->path, 31);
	/* First item is set up a lot like a symlink traversal. */
	/* printf("Looking for wildcard in %s\n", path); */
	/* TODO: wildcard detection here screws up on \\?\c:\ UNC names */
	if (strchr(base, '*') || strchr(base, '?')) {
		// It has a wildcard in it...
		// Separate the last element.
		p = strrchr(base, '/');
		if (p != NULL) {
			*p = '\0';
			tree_append(t, base, p - base);
			t->dirname_length = archive_strlen(&t->path);
			base = p + 1;
		}
	}
	tree_push(t, base);
	free(pathname);
	t->stack->flags = needsFirstVisit | isDirLink | needsAscent;
	t->d = INVALID_DIR_HANDLE;
	return (t);
#endif
}

/*
 * We've finished a directory; ascend back to the parent.
 */
static int
tree_ascend(struct tree *t)
{
	struct tree_entry *te;
	int r = 0;

	te = t->stack;
	t->depth--;
	if (te->flags & isDirLink) {
#ifdef HAVE_FCHDIR
		if (fchdir(te->symlink_parent_fd) != 0) {
			t->tree_errno = errno;
			r = TREE_ERROR_FATAL;
		}
		close(te->symlink_parent_fd);
#endif
		t->openCount--;
#if !defined(_WIN32) || defined(__CYGWIN__)
	} else {
		if (chdir("..") != 0) {
			t->tree_errno = errno;
			r = TREE_ERROR_FATAL;
		}
#endif
	}
	return (r);
}

/*
 * Pop the working stack.
 */
static void
tree_pop(struct tree *t)
{
	struct tree_entry *te;

	t->path.s[t->dirname_length] = '\0';
	t->path.length = t->dirname_length;
	if (t->stack == t->current && t->current != NULL)
		t->current = t->current->parent;
	te = t->stack;
	t->stack = te->next;
	t->dirname_length = te->dirname_length;
	t->basename = t->path.s + t->dirname_length;
	while (t->basename[0] == '/')
		t->basename++;
	archive_string_free(&te->name);
	free(te);
}

/*
 * Get the next item in the tree traversal.
 */
static int
tree_next(struct tree *t)
{
	int r;

	while (t->stack != NULL) {
		/* If there's an open dir, get the next entry from there. */
		if (t->d != INVALID_DIR_HANDLE) {
#if defined(_WIN32) && !defined(__CYGWIN__)
			r = tree_dir_next_windows(t, NULL);
#else
			r = tree_dir_next_posix(t);
#endif
			if (r == 0)
				continue;
			return (r);
		}

		if (t->stack->flags & needsFirstVisit) {
#if defined(_WIN32) && !defined(__CYGWIN__)
			char *d = t->stack->name.s;
			t->stack->flags &= ~needsFirstVisit;
			if (strchr(d, '*') || strchr(d, '?')) {
				r = tree_dir_next_windows(t, d);
				if (r == 0)
					continue;
				return (r);
			} else {
				HANDLE h = FindFirstFile(d, &t->_findData);
				if (h == INVALID_DIR_HANDLE) {
					t->tree_errno = errno;
					t->visit_type = TREE_ERROR_DIR;
					return (t->visit_type);
				}
				t->findData = &t->_findData;
				FindClose(h);
			}
#endif
			/* Top stack item needs a regular visit. */
			t->current = t->stack;
			tree_append(t, t->stack->name.s, archive_strlen(&(t->stack->name)));
			//t->dirname_length = t->path_length;
			//tree_pop(t);
			t->stack->flags &= ~needsFirstVisit;
			return (t->visit_type = TREE_REGULAR);
		} else if (t->stack->flags & needsDescent) {
			/* Top stack item is dir to descend into. */
			t->current = t->stack;
			tree_append(t, t->stack->name.s, archive_strlen(&(t->stack->name)));
			t->stack->flags &= ~needsDescent;
#ifdef HAVE_FCHDIR
			/* If it is a link, set up fd for the ascent. */
			if (t->stack->flags & isDirLink) {
				t->stack->symlink_parent_fd = open(".", O_RDONLY);
				t->openCount++;
				if (t->openCount > t->maxOpenCount)
					t->maxOpenCount = t->openCount;
			}
#endif
			t->dirname_length = archive_strlen(&t->path);
#if !defined(_WIN32) || defined(__CYGWIN__)
			if (chdir(t->stack->name.s) != 0)
			{
				/* chdir() failed; return error */
				tree_pop(t);
				t->tree_errno = errno;
				return (t->visit_type = TREE_ERROR_DIR);
			}
#endif
			t->depth++;
			return (t->visit_type = TREE_POSTDESCENT);
		} else if (t->stack->flags & needsOpen) {
			t->stack->flags &= ~needsOpen;
#if defined(_WIN32) && !defined(__CYGWIN__)
			r = tree_dir_next_windows(t, "*");
#else
			r = tree_dir_next_posix(t);
#endif
			if (r == 0)
				continue;
			return (r);
		} else if (t->stack->flags & needsAscent) {
		        /* Top stack item is dir and we're done with it. */
			r = tree_ascend(t);
			tree_pop(t);
			t->visit_type = r != 0 ? r : TREE_POSTASCENT;
			return (t->visit_type);
		} else {
			/* Top item on stack is dead. */
			tree_pop(t);
			t->flags &= ~hasLstat;
			t->flags &= ~hasStat;
		}
	}
	return (t->visit_type = 0);
}

#if defined(_WIN32) && !defined(__CYGWIN__)
static int
tree_dir_next_windows(struct tree *t, const char *pattern)
{
	const char *name;
	size_t namelen;
	int r;

	for (;;) {
		if (pattern != NULL) {
			struct archive_string pt;

			archive_string_init(&pt);
			archive_string_ensure(&pt,
			    archive_strlen(&(t->path)) + 2 + strlen(pattern));
			archive_string_copy(&pt, &(t->path));
			archive_strappend_char(&pt, '/');
			archive_strcat(&pt, pattern);
			t->d = FindFirstFile(pt.s, &t->_findData);
			archive_string_free(&pt);
			if (t->d == INVALID_DIR_HANDLE) {
				r = tree_ascend(t); /* Undo "chdir" */
				tree_pop(t);
				t->tree_errno = errno;
				t->visit_type = r != 0 ? r : TREE_ERROR_DIR;
				return (t->visit_type);
			}
			t->findData = &t->_findData;
			pattern = NULL;
		} else if (!FindNextFile(t->d, &t->_findData)) {
			FindClose(t->d);
			t->d = INVALID_DIR_HANDLE;
			t->findData = NULL;
			return (0);
		}
		name = t->findData->cFileName;
		namelen = strlen(name);
		t->flags &= ~hasLstat;
		t->flags &= ~hasStat;
		if (name[0] == '.' && name[1] == '\0')
			continue;
		if (name[0] == '.' && name[1] == '.' && name[2] == '\0')
			continue;
		tree_append(t, name, namelen);
		return (t->visit_type = TREE_REGULAR);
	}
}

#define EPOC_TIME ARCHIVE_LITERAL_ULL(116444736000000000)

static void
fileTimeToUtc(const FILETIME *filetime, time_t *time, long *ns)
{
	ULARGE_INTEGER utc;

	utc.HighPart = filetime->dwHighDateTime;
	utc.LowPart  = filetime->dwLowDateTime;
	if (utc.QuadPart >= EPOC_TIME) {
		utc.QuadPart -= EPOC_TIME;
		/* milli seconds base */
		*time = (time_t)(utc.QuadPart / 10000000);
		/* nano seconds base */
		*ns = (long)(utc.QuadPart % 10000000) * 100;
	} else {
		*time = 0;
		*ns = 0;
	}
}

static void
tree_archive_entry_copy_bhfi(struct archive_entry *entry, struct tree *t,
	const BY_HANDLE_FILE_INFORMATION *bhfi)
{
	time_t secs;
	long nsecs;
	mode_t mode;

	fileTimeToUtc(&bhfi->ftLastAccessTime, &secs, &nsecs);
	archive_entry_set_atime(entry, secs, nsecs);
	fileTimeToUtc(&bhfi->ftLastWriteTime, &secs, &nsecs);
	archive_entry_set_mtime(entry, secs, nsecs);
	fileTimeToUtc(&bhfi->ftCreationTime, &secs, &nsecs);
	archive_entry_set_birthtime(entry, secs, nsecs);
	archive_entry_set_dev(entry, bhfi->dwVolumeSerialNumber);
	/* Set FileIndex as i-node. We should remove a sequence number
	 * which is high-16-bits of nFileIndexHigh. */
	archive_entry_set_ino64(entry,
	    (((int64_t)(bhfi->nFileIndexHigh & 0x0000FFFFUL)) << 32)
	    + bhfi->nFileIndexLow);
	if (bhfi->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		archive_entry_set_nlink(entry, bhfi->nNumberOfLinks + 1);
	else
		archive_entry_set_nlink(entry, bhfi->nNumberOfLinks);
	archive_entry_set_size(entry,
	    (((int64_t)bhfi->nFileSizeHigh) << 32)
	    + bhfi->nFileSizeLow);
	archive_entry_set_dev(entry, bhfi->dwVolumeSerialNumber);
	archive_entry_set_uid(entry, 0);
	archive_entry_set_gid(entry, 0);
	archive_entry_set_rdev(entry, 0);

	mode = S_IRUSR | S_IRGRP | S_IROTH;
	if ((bhfi->dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0)
		mode |= S_IWUSR | S_IWGRP | S_IWOTH;
	if ((bhfi->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
	    t->findData->dwReserved0 == IO_REPARSE_TAG_SYMLINK)
		mode |= S_IFLNK;
	else if (bhfi->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
	else {
		const char *p;

		mode |= S_IFREG;
		p = strrchr(tree_current_path(t), '.');
		if (p != NULL && strlen(p) == 4) {
			switch (p[1]) {
			case 'B': case 'b':
				if ((p[2] == 'A' || p[2] == 'a' ) &&
				    (p[3] == 'T' || p[3] == 't' ))
					mode |= S_IXUSR | S_IXGRP | S_IXOTH;
				break;
			case 'C': case 'c':
				if (((p[2] == 'M' || p[2] == 'm' ) &&
				    (p[3] == 'D' || p[3] == 'd' )) ||
				    ((p[2] == 'M' || p[2] == 'm' ) &&
				    (p[3] == 'D' || p[3] == 'd' )))
					mode |= S_IXUSR | S_IXGRP | S_IXOTH;
				break;
			case 'E': case 'e':
				if ((p[2] == 'X' || p[2] == 'x' ) &&
				    (p[3] == 'E' || p[3] == 'e' ))
					mode |= S_IXUSR | S_IXGRP | S_IXOTH;
				break;
			default:
				break;
			}
		}
	}
	archive_entry_set_mode(entry, mode);
}

static int
tree_current_file_information(struct tree *t, BY_HANDLE_FILE_INFORMATION *st,
 int sim_lstat)
{
	HANDLE h;
	int r;
	DWORD flag = FILE_FLAG_BACKUP_SEMANTICS;
	
	if (sim_lstat && tree_current_is_physical_link(t))
		flag |= FILE_FLAG_OPEN_REPARSE_POINT;
	h = CreateFile(tree_current_access_path(t),
		0, 0, NULL,	OPEN_EXISTING, flag, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return (0);
	r = GetFileInformationByHandle(h, st);
	CloseHandle(h);
	return (r);
}

/*
 * Get the stat() data for the entry just returned from tree_next().
 */
static const BY_HANDLE_FILE_INFORMATION *
tree_current_stat(struct tree *t)
{
	if (!(t->flags & hasStat)) {
		if (!tree_current_file_information(t, &t->st, 0))
			return NULL;
		t->flags |= hasStat;
	}
	return (&t->st);
}

/*
 * Get the lstat() data for the entry just returned from tree_next().
 */
static const BY_HANDLE_FILE_INFORMATION *
tree_current_lstat(struct tree *t)
{
	if (!(t->flags & hasLstat)) {
		if (!tree_current_file_information(t, &t->lst, 1))
			return NULL;
		t->flags |= hasLstat;
	}
	return (&t->lst);
}

/*
 * Test whether current entry is a dir or link to a dir.
 */
static int
tree_current_is_dir(struct tree *t)
{
	if (t->findData)
		return (t->findData->dwFileAttributes
		    & FILE_ATTRIBUTE_DIRECTORY);
	return (0);
}

/*
 * Test whether current entry is a physical directory.  Usually, we
 * already have at least one of stat() or lstat() in memory, so we
 * use tricks to try to avoid an extra trip to the disk.
 */
static int
tree_current_is_physical_dir(struct tree *t)
{
	if (tree_current_is_physical_link(t))
		return (0);
	return (tree_current_is_dir(t));
}

/*
 * Test whether current entry is a symbolic link.
 */
static int
tree_current_is_physical_link(struct tree *t)
{
	if (t->findData)
		return ((t->findData->dwFileAttributes
			        & FILE_ATTRIBUTE_REPARSE_POINT) &&
			(t->findData->dwReserved0
			    == IO_REPARSE_TAG_SYMLINK));
	return (0);
}

#else

static int
tree_dir_next_posix(struct tree *t)
{
	int r;
	const char *name;
	size_t namelen;

	if (t->d == NULL) {
		if ((t->d = opendir(".")) == NULL) {
			r = tree_ascend(t); /* Undo "chdir" */
			tree_pop(t);
			t->tree_errno = errno;
			t->visit_type = r != 0 ? r : TREE_ERROR_DIR;
			return (t->visit_type);
		}
	}
	for (;;) {
		t->de = readdir(t->d);
		if (t->de == NULL) {
			closedir(t->d);
			t->d = INVALID_DIR_HANDLE;
			return (0);
		}
		name = t->de->d_name;
		namelen = D_NAMELEN(t->de);
		t->flags &= ~hasLstat;
		t->flags &= ~hasStat;
		if (name[0] == '.' && name[1] == '\0')
			continue;
		if (name[0] == '.' && name[1] == '.' && name[2] == '\0')
			continue;
		tree_append(t, name, namelen);
		return (t->visit_type = TREE_REGULAR);
	}
}


/*
 * Get the stat() data for the entry just returned from tree_next().
 */
static const struct stat *
tree_current_stat(struct tree *t)
{
	if (!(t->flags & hasStat)) {
		if (stat(tree_current_access_path(t), &t->st) != 0)
			return NULL;
		t->flags |= hasStat;
	}
	return (&t->st);
}

/*
 * Get the lstat() data for the entry just returned from tree_next().
 */
static const struct stat *
tree_current_lstat(struct tree *t)
{
	if (!(t->flags & hasLstat)) {
		if (lstat(tree_current_access_path(t), &t->lst) != 0)
			return NULL;
		t->flags |= hasLstat;
	}
	return (&t->lst);
}

/*
 * Test whether current entry is a dir or link to a dir.
 */
static int
tree_current_is_dir(struct tree *t)
{
	const struct stat *st;
	/*
	 * If we already have lstat() info, then try some
	 * cheap tests to determine if this is a dir.
	 */
	if (t->flags & hasLstat) {
		/* If lstat() says it's a dir, it must be a dir. */
		if (S_ISDIR(tree_current_lstat(t)->st_mode))
			return 1;
		/* Not a dir; might be a link to a dir. */
		/* If it's not a link, then it's not a link to a dir. */
		if (!S_ISLNK(tree_current_lstat(t)->st_mode))
			return 0;
		/*
		 * It's a link, but we don't know what it's a link to,
		 * so we'll have to use stat().
		 */
	}

	st = tree_current_stat(t);
	/* If we can't stat it, it's not a dir. */
	if (st == NULL)
		return 0;
	/* Use the definitive test.  Hopefully this is cached. */
	return (S_ISDIR(st->st_mode));
}

/*
 * Test whether current entry is a physical directory.  Usually, we
 * already have at least one of stat() or lstat() in memory, so we
 * use tricks to try to avoid an extra trip to the disk.
 */
static int
tree_current_is_physical_dir(struct tree *t)
{
	const struct stat *st;

	/*
	 * If stat() says it isn't a dir, then it's not a dir.
	 * If stat() data is cached, this check is free, so do it first.
	 */
	if ((t->flags & hasStat)
	    && (!S_ISDIR(tree_current_stat(t)->st_mode)))
		return 0;

	/*
	 * Either stat() said it was a dir (in which case, we have
	 * to determine whether it's really a link to a dir) or
	 * stat() info wasn't available.  So we use lstat(), which
	 * hopefully is already cached.
	 */

	st = tree_current_lstat(t);
	/* If we can't stat it, it's not a dir. */
	if (st == NULL)
		return 0;
	/* Use the definitive test.  Hopefully this is cached. */
	return (S_ISDIR(st->st_mode));
}
#endif

/*
 * Return the access path for the entry just returned from tree_next().
 */
static const char *
tree_current_access_path(struct tree *t)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	return (t->path.s);
#else
	return (t->basename);
#endif
}

/*
 * Return the full path for the entry just returned from tree_next().
 */
static const char *
tree_current_path(struct tree *t)
{
	return (t->path.s);
}

/*
 * Terminate the traversal and release any resources.
 */
static void
tree_close(struct tree *t)
{
	/* Release anything remaining in the stack. */
	while (t->stack != NULL)
		tree_pop(t);
	archive_string_free(&t->path);
	free(t->filesystem_table);
	/* TODO: Ensure that premature close() resets cwd */
#if 0
#ifdef HAVE_FCHDIR
	if (t->initialDirFd >= 0) {
		int s = fchdir(t->initialDirFd);
		(void)s; /* UNUSED */
		close(t->initialDirFd);
		t->initialDirFd = -1;
	}
#endif
#endif
	free(t);
}

