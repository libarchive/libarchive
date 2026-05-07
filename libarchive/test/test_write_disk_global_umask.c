/*-
 * Copyright (c) 2026 Omar Sawalha
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

#define UMASK 022

#if !defined(_WIN32) || defined(__CYGWIN__)
static mode_t
custom_umask(void)
{
	return (0077);
}
#endif

DEFINE_TEST(test_write_disk_global_umask)
{
#if !defined(_WIN32) || defined(__CYGWIN__)
	struct archive *a;
	struct archive_entry *ae;
	struct stat st;

	/* Force the umask to be different from the callback
	 * so we can verify the callback is actually being used. */
	assertUmask(UMASK);

	/* Set the global umask callback. */
	assertEqualInt(ARCHIVE_OK,
	    archive_write_disk_set_global_umask_lookup(&custom_umask));

	assert((a = archive_write_disk_new()) != NULL);

	/* Create a file with mode 0777; the callback umask 0077
	 * should reduce it to 0700. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_finish_entry(a));
	archive_entry_free(ae);

	/* Create a directory with mode 0777; should become 0700. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir");
	archive_entry_set_mode(ae, S_IFDIR | 0777);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_finish_entry(a));
	archive_entry_free(ae);

	assertEqualInt(0, archive_write_free(a));

	/* Verify the file was created with the expected permissions. */
	assertEqualInt(0, stat("file", &st));
	failure("File should be 0700 (0777 & ~0077)");
	assertEqualInt(0700, st.st_mode & 0777);

	/* Verify the directory was created with the expected permissions. */
	assertEqualInt(0, stat("dir", &st));
	failure("Dir should be 0700 (0777 & ~0077)");
	assertEqualInt(0700, st.st_mode & 0777);

	/* Reset the callback */
	archive_write_disk_set_global_umask_lookup(NULL);
#else
	skipping("umask test is not supported on Windows");
#endif
}
