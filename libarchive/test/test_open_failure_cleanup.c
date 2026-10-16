/*-
 * Copyright (c) 2026 libarchive contributors
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
#include "test.h"

#if defined(_WIN32) && !defined(__CYGWIN__)

DEFINE_TEST(test_open_failure_cleanup)
{
	skipping("requires dlsym(3)-based allocator interposition");
}

#else /* ^^^ _WIN32; POSIX vvv */

#include <dlfcn.h>

/*
 * Fail exactly one allocation at a time during archive_read_open_memory()
 * and archive_write_open_memory() and check that:
 *   - a failed open always reports an error message, and
 *   - the client data block handed to the open call is released on failure.
 *
 * The second point is enforced by running this test under LeakSanitizer or
 * Valgrind: the open functions allocate their client data block before
 * archive_read_open1()/archive_write_open2() allocate the internal object
 * that will run the client closer, so an allocation failure in between used
 * to leak the client data block.
 *
 * libarchive has no custom allocator hook, so the one-shot failure is
 * injected by interposing malloc(3) and friends; outside the injection
 * window everything is passed straight through.
 */

static long fail_at = -1;
static long alloc_count = 0;

static void *(*real_malloc)(size_t);
static void *(*real_calloc)(size_t, size_t);
static void *(*real_realloc)(void *, size_t);
static void (*real_free)(void *);

static char bootstrap[8192];
static size_t bootstrap_used = 0;

static void
real_init(void)
{
	if (real_malloc != NULL)
		return;
	real_malloc = (void *(*)(size_t))dlsym(RTLD_NEXT, "malloc");
	real_calloc = (void *(*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
	real_realloc = (void *(*)(void *, size_t))dlsym(RTLD_NEXT, "realloc");
	real_free = (void (*)(void *))dlsym(RTLD_NEXT, "free");
}

void *
malloc(size_t size)
{
	void *p;

	real_init();
	if (fail_at >= 0 && alloc_count++ == fail_at) {
		errno = ENOMEM;
		return (NULL);
	}
	if (real_malloc == NULL) {
		p = bootstrap + bootstrap_used;
		bootstrap_used += (size + 15) & ~(size_t)15;
		if (bootstrap_used > sizeof(bootstrap))
			abort();
		return (p);
	}
	return (real_malloc(size));
}

void *
calloc(size_t n, size_t size)
{
	void *p;

	real_init();
	if (fail_at >= 0 && alloc_count++ == fail_at) {
		errno = ENOMEM;
		return (NULL);
	}
	if (real_calloc == NULL) {
		p = malloc(n * size);
		if (p != NULL)
			memset(p, 0, n * size);
		return (p);
	}
	return (real_calloc(n, size));
}

void *
realloc(void *ptr, size_t size)
{
	real_init();
	if (fail_at >= 0 && alloc_count++ == fail_at) {
		errno = ENOMEM;
		return (NULL);
	}
	return (real_realloc(ptr, size));
}

void
free(void *ptr)
{
	real_init();
	if (real_free == NULL) {
		if (ptr >= (void *)bootstrap
		    && ptr < (void *)(bootstrap + sizeof(bootstrap)))
			return;
		abort();
	}
	real_free(ptr);
}

DEFINE_TEST(test_open_failure_cleanup)
{
	static const char data[] = "not an archive";
	unsigned char out[256];
	long i;

	real_init();

	for (i = -1; i < 150; i++) {
		struct archive *a;
		struct archive *w;
		size_t used = 0;
		int r;

		/* Read side. */
		a = archive_read_new();
		assert(a != NULL);
		archive_read_support_filter_all(a);
		archive_read_support_format_all(a);
		alloc_count = 0;
		fail_at = i;
		r = archive_read_open_memory(a, data, sizeof(data));
		fail_at = -1;
		if (r != ARCHIVE_OK) {
			/* Whatever failed, the user must learn why. */
			assert(archive_error_string(a) != NULL);
		}
		/* Any client data left behind by a failed open shows up as a
		 * leak in sanitizer/valgrind runs of this test. */
		archive_read_free(a);

		/* Write side. */
		w = archive_write_new();
		assert(w != NULL);
		archive_write_set_format_ustar(w);
		alloc_count = 0;
		fail_at = i;
		r = archive_write_open_memory(w, out, sizeof(out), &used);
		fail_at = -1;
		if (r != ARCHIVE_OK) {
			assert(archive_error_string(w) != NULL);
		}
		archive_write_free(w);
	}
}

#endif /* _WIN32 */
