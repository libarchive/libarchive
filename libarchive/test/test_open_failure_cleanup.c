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

/*
 * When archive_read_open1()/archive_write_open2() fails, client data
 * registered with the open call must still be released exactly once:
 *
 *   - on the read side the registered closer releases each data block,
 *   - on the write side the registered freer releases the data block
 *     (the write closer is a flush callback and only runs after a
 *     successful open).
 *
 * Several open-failure paths run before the internal filter that normally
 * invokes these callbacks exists (e.g. no reader was provided, or an
 * allocation failed); those paths must release the client data themselves.
 * The counts below are asserted after archive_read_free()/archive_write_free()
 * so that a missing release (leak) and a double release (double free) are
 * both caught, and sanitizer runs of this test confirm the blocks are
 * actually freed.
 */

static int open_calls;
static int close_calls;
static int free_calls;

static int
test_open_ok(struct archive *a, void *d)
{
	(void)a; (void)d;
	open_calls++;
	return (ARCHIVE_OK);
}

static int
test_open_fail(struct archive *a, void *d)
{
	(void)d;
	open_calls++;
	archive_set_error(a, EIO, "opener failed");
	return (ARCHIVE_FATAL);
}

static la_ssize_t
test_read_fail(struct archive *a, void *d, const void **buf)
{
	(void)d;
	*buf = NULL;
	archive_set_error(a, EIO, "read failed");
	return (-1);
}

static la_ssize_t
test_write(struct archive *a, void *d, const void *buf, size_t n)
{
	(void)a; (void)d; (void)buf;
	return ((la_ssize_t)n);
}

/* Read-side closer: the read API has no freer, so the closer releases
 * each client data block. */
static int
test_read_close(struct archive *a, void *d)
{
	(void)a;
	close_calls++;
	free(d);
	return (ARCHIVE_OK);
}

/* Write-side closer: a flush callback; the freer releases the block. */
static int
test_flush(struct archive *a, void *d)
{
	(void)a; (void)d;
	close_calls++;
	return (ARCHIVE_OK);
}

static int
test_free(struct archive *a, void *d)
{
	(void)a;
	free_calls++;
	free(d);
	return (ARCHIVE_OK);
}

static void
reset_counts(void)
{
	open_calls = close_calls = free_calls = 0;
}

/* opener: 0 = none registered, 1 = succeeds, 2 = fails. */
static void
test_read_case(const char *name, int opener, int with_reader)
{
	struct archive *a;
	void *state;
	int r;

	reset_counts();
	a = archive_read_new();
	assert(a != NULL);
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);
	state = malloc(123);
	assert(state != NULL);
	assertEqualInt(ARCHIVE_OK, archive_read_add_callback_data(a, state, 0));
	assertEqualInt(ARCHIVE_OK,
	    archive_read_set_close_callback(a, test_read_close));
	if (opener != 0)
		assertEqualInt(ARCHIVE_OK, archive_read_set_open_callback(a,
		    opener == 2 ? test_open_fail : test_open_ok));
	if (with_reader)
		assertEqualInt(ARCHIVE_OK,
		    archive_read_set_read_callback(a, test_read_fail));

	r = archive_read_open1(a);
	failure("%s: open1 returned %d", name, r);
	assertEqualInt(ARCHIVE_FATAL, r);
	/* However the open failed, the user must learn why. */
	assert(archive_errno(a) != 0);

	archive_read_free(a);
	/* The closer releases the client data: exactly once, never zero
	 * (that would leak) and never twice (that would double free). */
	assertEqualInt(1, close_calls);
}

DEFINE_TEST(test_open_failure_cleanup)
{
	struct archive *w;
	void *state;
	int r;

	/* Read side: no reader callback was provided, so open fails before
	 * any filter exists that could run the closer. The opener would
	 * have succeeded, so the missing reader is the only failure. */
	test_read_case("no reader", 1, 0);
	assertEqualInt(0, open_calls);

	/* Read side: the opener rejects the archive; the closer must run. */
	test_read_case("opener fails", 2, 1);
	assertEqualInt(1, open_calls);

	/* Read side: the reader fails while filters are bidding. */
	test_read_case("read error during bidding", 0, 1);
	assertEqualInt(0, open_calls);

	/* Write side: the opener rejects the archive. The freer releases
	 * the client data exactly once; the flush closer must not run
	 * because the open never succeeded. */
	reset_counts();
	w = archive_write_new();
	assert(w != NULL);
	archive_write_set_format_ustar(w);
	state = malloc(123);
	assert(state != NULL);
	r = archive_write_open2(w, state, test_open_fail, test_write,
	    test_flush, test_free);
	failure("write opener fails: open2 returned %d", r);
	assertEqualInt(ARCHIVE_FATAL, r);
	assert(archive_errno(w) != 0);
	archive_write_free(w);
	assertEqualInt(1, open_calls);
	assertEqualInt(1, free_calls);
	assertEqualInt(0, close_calls);
}
