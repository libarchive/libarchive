/*-
 * Copyright (c) 2011 Tim Kientzle
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
__FBSDID("$FreeBSD$");

typedef struct archive *constructor();
typedef int enabler(struct archive *);
typedef int destructor(struct archive *);

static void
test_success(constructor new_, enabler enable_, destructor free_)
{
	struct archive *a = new_();
	assertEqualIntA(a, ARCHIVE_OK, enable_(a));
	assert(NULL == archive_error_string(a));
	assertEqualIntA(a, 0, archive_errno(a));
	free_(a);
}

static void
test_failure(constructor new_, enabler enable_, destructor free_)
{
	struct archive *a = new_();
	assertEqualIntA(a, ARCHIVE_FATAL, enable_(a));
	assert(NULL != archive_error_string(a));
	assertEqualIntA(a, -1, archive_errno(a));
	free_(a);
}

static void
test_filter_or_format(enabler enable)
{
	test_success(archive_read_new, enable, archive_read_free);
	test_failure(archive_write_new, enable, archive_write_free);
	test_failure(archive_read_disk_new, enable, archive_read_free);
	test_failure(archive_write_disk_new, enable, archive_write_free);
}
