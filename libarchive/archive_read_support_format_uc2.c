/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2026, Eremey Valetov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* UC2 reader through the optional shared libuc2read dependency.
 *
 * Input and decompressed entries are spooled to anonymous temporary
 * files. Decoder allocations, cached masters, metadata and paths share a
 * memory budget. Input, entry count and decoded output have separate limits.
 */

#include "archive_platform.h"

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_private.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32) && !defined(__CYGWIN__)
#include <io.h>
#else
#include <unistd.h>
#endif

#if !defined(HAVE_LIBUC2READ)
int
archive_read_support_format_uc2(struct archive *a)
{
	archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
			    "archive_read_support_format_uc2");
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
			  "UC2 support was not enabled in this build");
	return ARCHIVE_WARN;
}
#else
#include <uc2/libuc2.h>

union uc2_allocation {
	size_t size;
	long double alignment;
	void *pointer_alignment;
};
struct uc2_directory {
	unsigned id;
	int index;
};

struct uc2_la_state {
	FILE *input;
	uint64_t len;
	int prepared;
	uint64_t memory_used, memory_limit, input_limit, entry_limit,
	    total_limit;
	uint64_t total_decoded;
	unsigned entries_limit;
	int memory_refused, allocation_failed;
	struct uc2_directory *directories;
	int n_directories;

	/* libuc2 */
	uc2_handle handle;

	/* Cached cdir entries.  uc2_read_cdir is single-pass; we capture
	 * everything on the first read_header call. */
	struct uc2_entry *entries;
	char **paths; /* composed full path per entry */
	int n_entries;
	int n_capacity;
	int next_entry;
	char label[12];

	FILE *entry_file;
	uint8_t entry_data[65536];
	uint64_t entry_pos;
	int entry_ready;
	int entry_yielded;
};

static int
seek_file(FILE *f, uint64_t pos)
{
#ifdef _WIN32
	return _fseeki64(f, (__int64)pos, SEEK_SET);
#else
	return fseeko(f, (off_t)pos, SEEK_SET);
#endif
}

static FILE *
spool_file(void)
{
	/* libarchive's helper honors the temporary directory and creates an
	   anonymous file. MSVC tmpfile() instead requires drive-root access. */
	int fd = __archive_mktemp(NULL);
	FILE *file;
	if (fd < 0)
		return NULL;
#if defined(_WIN32) && !defined(__CYGWIN__)
	file = _fdopen(fd, "w+b");
	if (!file) {
		int saved = errno;
		_close(fd);
		errno = saved;
	}
#else
	file = fdopen(fd, "w+b");
	if (!file) {
		int saved = errno;
		close(fd);
		errno = saved;
	}
#endif
	return file;
}

/* libuc2 requires random access, including on non-seekable input. */
static int
slurp_read(void *ctx, unsigned pos, void *buf, unsigned len)
{
	struct uc2_la_state *st = (struct uc2_la_state *)ctx;
	if ((uint64_t)pos >= st->len)
		return 0;
	uint64_t avail = st->len - pos;
	if (len > avail)
		len = (unsigned)avail;
	if (len > INT_MAX)
		len = INT_MAX;
	if (seek_file(st->input, pos) != 0)
		return -1;
	size_t n = fread(buf, 1, len, st->input);
	return ferror(st->input) ? -1 : (int)n;
}

static void *
budget_alloc(struct uc2_la_state *st, size_t size)
{
	union uc2_allocation *allocation;
	if (size > SIZE_MAX - sizeof *allocation ||
	    (uint64_t)size + sizeof *allocation >
		st->memory_limit - st->memory_used) {
		st->memory_refused = 1;
		return NULL;
	}
	allocation = malloc(sizeof *allocation + size);
	if (!allocation) {
		st->allocation_failed = 1;
		return NULL;
	}
	allocation->size = size + sizeof *allocation;
	st->memory_used += allocation->size;
	return allocation + 1;
}

static void *
slurp_alloc(void *ctx, unsigned size)
{
	return budget_alloc(ctx, size);
}

static void
slurp_free(void *ctx, void *ptr)
{
	struct uc2_la_state *st = ctx;
	if (ptr) {
		union uc2_allocation *allocation =
		    (union uc2_allocation *)ptr - 1;
		st->memory_used -= allocation->size;
		free(allocation);
	}
}

static void *
budget_resize(struct uc2_la_state *st, void *old, size_t size)
{
	void *next = budget_alloc(st, size);
	if (next && old) {
		size_t previous = ((union uc2_allocation *)old - 1)->size -
				  sizeof(union uc2_allocation);
		memcpy(next, old, previous < size ? previous : size);
		slurp_free(st, old);
	}
	return next;
}

static struct uc2_io slurp_io = {
    .read = slurp_read,
    .alloc = slurp_alloc,
    .free = slurp_free,
    .warn = NULL,
};

/* Bridge libuc2's push API to libarchive's pull API through a temp file. */
struct extract_buf {
	FILE *file;
	uint64_t len, limit;
};

static int
extract_write(void *ctx, const void *p, unsigned len)
{
	struct extract_buf *eb = (struct extract_buf *)ctx;
	if (len > eb->limit - eb->len || fwrite(p, 1, len, eb->file) != len)
		return -1;
	eb->len += len;
	return 0;
}

/* DOS timestamps carry no timezone. Use a timezone-independent UTC mapping
 * without timegm extensions or modifying process-global TZ.
 * Invalid/unrepresentable times map to zero; no permission/attribute decision
 * depends on them. */
static time_t
dos_to_unix_time(unsigned dos_time)
{
	static const unsigned month_days[] = {31, 28, 31, 30, 31, 30,
					      31, 31, 30, 31, 30, 31};
	unsigned year = 1980 + (dos_time >> 25), month = (dos_time >> 21) & 15;
	unsigned day = (dos_time >> 16) & 31, hour = (dos_time >> 11) & 31;
	unsigned minute = (dos_time >> 5) & 63, second = (dos_time & 31) * 2;
	int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
	if (month < 1 || month > 12 || day < 1 ||
	    day > month_days[month - 1] + (month == 2 && leap) || hour > 23 ||
	    minute > 59 || second > 59)
		return 0;
	int64_t days = 0;
	for (unsigned y = 1970; y < year; y++)
		days += 365 + (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
	for (unsigned m = 1; m < month; m++)
		days += month_days[m - 1] + (m == 2 && leap);
	int64_t seconds =
	    (days + day - 1) * 86400 + hour * 3600 + minute * 60 + second;
	time_t result = (time_t)seconds;
	return (int64_t)result == seconds ? result : 0;
}

static int uc2_la_bid(struct archive_read *, int);
static int uc2_la_options(struct archive_read *, const char *, const char *);
static int uc2_la_read_header(struct archive_read *, struct archive_entry *);
static int uc2_la_read_data(struct archive_read *, const void **, size_t *,
			    int64_t *);
static int uc2_la_read_data_skip(struct archive_read *);
static int uc2_la_cleanup(struct archive_read *);

int
archive_read_support_format_uc2(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct uc2_la_state *state;
	int r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
			    "archive_read_support_format_uc2");

	state = (struct uc2_la_state *)calloc(1, sizeof(*state));
	if (state == NULL) {
		archive_set_error(_a, ENOMEM,
				  "Out of memory allocating UC2 reader state");
		return (ARCHIVE_FATAL);
	}

	state->memory_limit = 64ULL * 1024 * 1024;
	state->input_limit = UINT32_MAX;
	state->entry_limit = 1024ULL * 1024 * 1024;
	state->total_limit = 8ULL * 1024 * 1024 * 1024;
	state->entries_limit = 100000;
	r = __archive_read_register_format(
	    a, state, "uc2", uc2_la_bid, uc2_la_options, uc2_la_read_header,
	    uc2_la_read_data, uc2_la_read_data_skip, NULL, uc2_la_cleanup, NULL,
	    NULL);

	if (r != ARCHIVE_OK)
		free(state);
	return (r);
}

/* Decimal byte/count values. Zero and signed values never disable a limit. */
static int
uc2_la_options(struct archive_read *a, const char *key, const char *value)
{
	struct uc2_la_state *st = a->format->data;
	uint64_t *target = NULL, maximum = UINT64_MAX, number = 0;
	if (strcmp(key, "memory-limit") == 0) {
		target = &st->memory_limit;
		maximum = SIZE_MAX;
	} else if (strcmp(key, "input-limit") == 0) {
		target = &st->input_limit;
		maximum = UINT32_MAX;
	} else if (strcmp(key, "entry-limit") == 0) {
		target = &st->entry_limit;
		maximum = UINT32_MAX;
	} else if (strcmp(key, "total-limit") == 0) {
		target = &st->total_limit;
	} else if (strcmp(key, "entries-limit") == 0) {
		maximum = INT_MAX;
	} else
		return ARCHIVE_WARN;
	if (!value || !*value || st->prepared)
		goto invalid;
	for (const char *p = value; *p; p++) {
		if (*p < '0' || *p > '9' ||
		    number > (maximum - (*p - '0')) / 10)
			goto invalid;
		number = number * 10 + (*p - '0');
	}
	if (!number)
		goto invalid;
	if (target)
		*target = number;
	else
		st->entries_limit = (unsigned)number;
	return ARCHIVE_OK;
invalid:
	archive_set_error(
	    &a->archive, EINVAL,
	    "UC2: invalid %s (positive decimal value required before reading)",
	    key);
	return ARCHIVE_FAILED;
}

static int
uc2_la_bid(struct archive_read *a, int best_bid)
{
	const unsigned char *p;

	if (best_bid >= 32)
		return (0);

	p = __archive_read_ahead(a, 4, NULL);
	if (p == NULL)
		return (-1);

	if (p[0] == 0x55 && p[1] == 0x43 && p[2] == 0x32 && p[3] == 0x1A)
		return (32);
	return (0);
}

/* Spool without holding compressed payloads in RAM. */
static int
slurp_archive(struct archive_read *a, struct uc2_la_state *st)
{
	st->input = spool_file();
	if (!st->input) {
		archive_set_error(&a->archive, errno,
				  "UC2: cannot create input spool");
		return ARCHIVE_FATAL;
	}
	for (;;) {
		ssize_t avail;
		const void *p = __archive_read_ahead(a, 1, &avail);
		if (p == NULL) {
			if (avail < 0) {
				archive_set_error(
				    &a->archive, EIO,
				    "UC2: read error while slurping archive");
				return (ARCHIVE_FATAL);
			}
			break; /* clean EOF */
		}
		if (avail <= 0)
			break;

		if (avail > 65536)
			avail = 65536;
		if ((uint64_t)avail > st->input_limit - st->len) {
			archive_set_error(
			    &a->archive, EFBIG,
			    "UC2: input-limit exceeded (at most 4 GiB)");
			return ARCHIVE_FATAL;
		}
		if (fwrite(p, 1, (size_t)avail, st->input) != (size_t)avail ||
		    __archive_read_consume(a, avail) != avail) {
			archive_set_error(&a->archive, EIO,
					  "UC2: input spool write failed");
			return ARCHIVE_FATAL;
		}
		st->len += (uint64_t)avail;
	}
	if (fflush(st->input) != 0) {
		archive_set_error(&a->archive, EIO,
				  "UC2: input spool flush failed");
		return ARCHIVE_FATAL;
	}
	return (ARCHIVE_OK);
}

/* Walk uc2_read_cdir and cache all entries.  Tagged entries have
 * uc2_get_tag called to fully resolve names. */
static int
collect_entries(struct archive_read *a, struct uc2_la_state *st)
{
	st->handle = uc2_open(&slurp_io, st);
	if (st->handle == NULL) {
		archive_set_error(&a->archive, ENOMEM,
				  "UC2: uc2_open failed (memory-limit or "
				  "allocation failure)");
		return (ARCHIVE_FATAL);
	}

	for (;;) {
		struct uc2_entry entry, *e = &entry;
		int ret = uc2_read_cdir(st->handle, e);
		if (ret == UC2_End)
			break;
		if (ret < 0) {
			archive_set_error(
			    &a->archive,
			    (st->memory_refused || st->allocation_failed)
				? ENOMEM
				: EINVAL,
			    "UC2: directory read failed: %s",
			    st->memory_refused ? "memory-limit exceeded"
					       : uc2_message(st->handle, ret));
			return ARCHIVE_FATAL;
		}
		if ((unsigned)st->n_entries >= st->entries_limit) {
			archive_set_error(&a->archive, EFBIG,
					  "UC2: entries-limit exceeded");
			return ARCHIVE_FATAL;
		}
		if (st->n_entries == st->n_capacity) {
			unsigned ncap =
			    st->n_capacity ? (unsigned)st->n_capacity * 2 : 32;
			if (ncap > st->entries_limit)
				ncap = st->entries_limit;
			if ((uint64_t)ncap * sizeof *st->entries > SIZE_MAX) {
				archive_set_error(
				    &a->archive, ENOMEM,
				    "UC2: entry allocation size overflow");
				return ARCHIVE_FATAL;
			}
			void *next =
			    budget_resize(st, st->entries,
					  (size_t)ncap * sizeof *st->entries);
			if (!next) {
				archive_set_error(
				    &a->archive, ENOMEM,
				    "UC2: memory-limit or allocation failure "
				    "collecting entries");
				return ARCHIVE_FATAL;
			}
			st->entries = next;
			st->n_capacity = (int)ncap;
		}

		while (ret == UC2_TaggedEntry) {
			char *tag;
			void *data;
			unsigned size;
			ret = uc2_get_tag(st->handle, e, &tag, &data, &size);
			if (ret < 0) {
				archive_set_error(
				    &a->archive,
				    (st->memory_refused ||
				     st->allocation_failed)
					? ENOMEM
					: EINVAL,
				    "UC2: uc2_get_tag failed: %s",
				    st->memory_refused
					? "memory-limit exceeded"
					: uc2_message(st->handle, ret));
				return (ARCHIVE_FATAL);
			}
		}

		st->entries[st->n_entries++] = entry;
	}

	if (uc2_finish_cdir(st->handle, st->label) < 0) {
		archive_set_error(&a->archive, EINVAL,
				  "UC2: invalid directory trailer");
		return ARCHIVE_FATAL;
	}
	return (ARCHIVE_OK);
}

static int
compare_directory(const void *left, const void *right)
{
	unsigned a = ((const struct uc2_directory *)left)->id;
	unsigned b = ((const struct uc2_directory *)right)->id;
	return (a > b) - (a < b);
}

/* Append the full path of directory `id` (with a trailing slash) to
 * buf.  Returns the new offset, or -1 on overflow.  UC2 directory ids
 * are archive-global; root is 0.  The depth cap breaks cycles in
 * damaged directories. */
static int
build_dir_path(struct uc2_la_state *st, unsigned id, char *buf, size_t cap,
	       int depth)
{
	if (id == 0)
		return (0);
	if (depth > 64)
		return (-1); /* cyclic or pathologically deep: corrupt cdir */
	struct uc2_directory key = {id, 0};
	struct uc2_directory *found =
	    st->n_directories
		? bsearch(&key, st->directories, (size_t)st->n_directories,
			  sizeof key, compare_directory)
		: NULL;
	if (found) {
		struct uc2_entry *d = &st->entries[found->index];
		{
			int off =
			    build_dir_path(st, d->dirid, buf, cap, depth + 1);
			int n;
			if (off < 0)
				return (-1);
			n = snprintf(buf + off, cap - off, "%s/", d->name);
			if (n < 0 || (size_t)n >= cap - off)
				return (-1);
			return (off + n);
		}
	}
	return (-1); /* a missing parent is a damaged directory */
}

/* Compose a full path for every entry: parent directories joined with
 * '/', directories themselves carrying a trailing slash. */
static int
compose_paths(struct archive_read *a, struct uc2_la_state *st)
{
	int i;

	for (i = 0; i < st->n_entries; i++)
		st->n_directories += st->entries[i].is_dir;
	if (st->n_directories) {
		st->directories = budget_alloc(st, (size_t)st->n_directories *
						       sizeof *st->directories);
		if (!st->directories)
			goto memory_error;
		int n = 0;
		for (i = 0; i < st->n_entries; i++)
			if (st->entries[i].is_dir)
				st->directories[n++] = (struct uc2_directory){
				    st->entries[i].id, i};
		qsort(st->directories, (size_t)n, sizeof *st->directories,
		      compare_directory);
		for (i = 0; i < n; i++) {
			if (!st->directories[i].id ||
			    (i && st->directories[i - 1].id ==
				      st->directories[i].id)) {
				archive_set_error(
				    &a->archive, EINVAL,
				    "UC2: duplicate or zero directory ID");
				return ARCHIVE_FATAL;
			}
		}
	}
	st->paths = budget_alloc(st, (size_t)st->n_entries * sizeof *st->paths);
	if (st->paths)
		memset(st->paths, 0, (size_t)st->n_entries * sizeof *st->paths);
	if (st->paths == NULL && st->n_entries > 0) {
	memory_error:
		archive_set_error(
		    &a->archive, ENOMEM,
		    "UC2: memory-limit or allocation failure composing paths");
		return (ARCHIVE_FATAL);
	}

	for (i = 0; i < st->n_entries; i++) {
		struct uc2_entry *e = &st->entries[i];
		char buf[2048];
		int off = build_dir_path(st, e->dirid, buf, sizeof buf, 0);
		int n;
		if (off < 0) {
			archive_set_error(&a->archive, EINVAL,
					  "UC2: directory path too long");
			return (ARCHIVE_FATAL);
		}
		n = snprintf(buf + off, sizeof buf - off, "%s%s", e->name,
			     e->is_dir ? "/" : "");
		if (n < 0 || (size_t)n >= sizeof buf - off) {
			archive_set_error(&a->archive, EINVAL,
					  "UC2: entry path too long");
			return (ARCHIVE_FATAL);
		}
		st->paths[i] = budget_alloc(st, strlen(buf) + 1);
		if (st->paths[i] == NULL) {
			archive_set_error(&a->archive, ENOMEM,
					  "UC2: memory-limit or allocation "
					  "failure composing paths");
			return (ARCHIVE_FATAL);
		}
		strcpy(st->paths[i], buf);
	}
	return (ARCHIVE_OK);
}

static int
uc2_la_read_header(struct archive_read *a, struct archive_entry *entry)
{
	struct uc2_la_state *st = (struct uc2_la_state *)a->format->data;

	a->archive.archive_format = ARCHIVE_FORMAT_UC2;
	a->archive.archive_format_name = "UC2";

	if (!st->prepared) {
		int r = slurp_archive(a, st);
		if (r != ARCHIVE_OK)
			return r;
		st->prepared = 1;

		r = collect_entries(a, st);
		if (r != ARCHIVE_OK)
			return r;

		r = compose_paths(a, st);
		if (r != ARCHIVE_OK)
			return r;
	}

	if (st->next_entry >= st->n_entries) {
		if (st->entry_file)
			fclose(st->entry_file);
		st->entry_file = NULL;
		return (ARCHIVE_EOF);
	}

	struct uc2_entry *e = &st->entries[st->next_entry++];

	if (st->entry_file)
		fclose(st->entry_file);
	st->entry_file = NULL;
	st->entry_pos = 0;
	st->entry_ready = 0;
	st->entry_yielded = 0;

	archive_entry_set_pathname(entry, st->paths[st->next_entry - 1]);
	archive_entry_set_size(entry, (la_int64_t)e->size);
	archive_entry_set_mtime(entry, dos_to_unix_time(e->dos_time), 0);

	if (e->is_dir) {
		archive_entry_set_filetype(entry, AE_IFDIR);
		archive_entry_set_perm(entry, 0755);
	} else {
		archive_entry_set_filetype(entry, AE_IFREG);
		mode_t mode = 0644;
		if (e->attr & UC2_Attr_R)
			mode &= ~0222;
		archive_entry_set_perm(entry, mode);
	}

	return (ARCHIVE_OK);
}

static int
uc2_la_read_data(struct archive_read *a, const void **buff, size_t *size,
		 int64_t *offset)
{
	struct uc2_la_state *st = (struct uc2_la_state *)a->format->data;

	if (st->next_entry == 0 || st->entry_yielded) {
		*buff = NULL;
		*size = 0;
		*offset = 0;
		return (ARCHIVE_EOF);
	}

	struct uc2_entry *e = &st->entries[st->next_entry - 1];
	if (e->is_dir) {
		st->entry_yielded = 1;
		*buff = NULL;
		*size = 0;
		*offset = 0;
		return (ARCHIVE_EOF);
	}

	if (!st->entry_ready) {
		if (e->size > st->entry_limit ||
		    e->size > st->total_limit - st->total_decoded) {
			archive_set_error(
			    &a->archive, EFBIG,
			    "UC2: entry-limit or total-limit exceeded");
			return ARCHIVE_FATAL;
		}
		st->total_decoded += e->size;
		st->entry_file = spool_file();
		if (!st->entry_file) {
			archive_set_error(&a->archive, errno,
					  "UC2: cannot create entry spool");
			return ARCHIVE_FATAL;
		}
		struct extract_buf eb = {.file = st->entry_file,
					 .limit = e->size};
		int ret = uc2_extract(st->handle, &e->xi, e->size,
				      extract_write, &eb);
		if (ret < 0 || eb.len != e->size ||
		    fflush(st->entry_file) != 0 ||
		    seek_file(st->entry_file, 0) != 0) {
			archive_set_error(
			    &a->archive,
			    (st->memory_refused || st->allocation_failed)
				? ENOMEM
				: EIO,
			    "UC2: extraction or entry spool failed (%d): %s",
			    ret,
			    st->memory_refused ? "memory-limit exceeded"
			    : ret < 0	       ? uc2_message(st->handle, ret)
					       : "entry spool I/O failure");
			return ARCHIVE_FATAL;
		}
		st->entry_ready = 1;
	}
	size_t n =
	    fread(st->entry_data, 1, sizeof st->entry_data, st->entry_file);
	if (ferror(st->entry_file)) {
		archive_set_error(&a->archive, EIO,
				  "UC2: entry spool read failed");
		return ARCHIVE_FATAL;
	}
	*buff = st->entry_data;
	*size = n;
	*offset = (int64_t)st->entry_pos;
	st->entry_pos += n;
	if (!n)
		st->entry_yielded = 1;
	return n ? ARCHIVE_OK : ARCHIVE_EOF;
}

static int
uc2_la_read_data_skip(struct archive_read *a)
{
	struct uc2_la_state *st = (struct uc2_la_state *)a->format->data;
	if (st->entry_file)
		fclose(st->entry_file);
	st->entry_file = NULL;
	st->entry_yielded = 1;
	return (ARCHIVE_OK);
}

static int
uc2_la_cleanup(struct archive_read *a)
{
	struct uc2_la_state *st = (struct uc2_la_state *)a->format->data;
	if (st == NULL)
		return (ARCHIVE_OK);
	if (st->handle)
		uc2_close(st->handle);
	if (st->paths) {
		int i;
		for (i = 0; i < st->n_entries; i++)
			slurp_free(st, st->paths[i]);
		slurp_free(st, st->paths);
	}
	if (st->input)
		fclose(st->input);
	if (st->entry_file)
		fclose(st->entry_file);
	slurp_free(st, st->entries);
	slurp_free(st, st->directories);
	free(st);
	a->format->data = NULL;
	return (ARCHIVE_OK);
}

#endif /* HAVE_LIBUC2READ */
