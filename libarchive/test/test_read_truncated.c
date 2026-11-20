/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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

static char buff[1000000];
static char buff2[100000];

DEFINE_TEST(test_read_truncated)
{
	struct archive_entry *ae;
	struct archive *a;
	unsigned int i;
	size_t used;

	/* Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff), &used));

	/*
	 * Write a file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	fill_with_pseudorandom_data(buff2, sizeof(buff2));
	archive_entry_set_size(ae, sizeof(buff2));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, sizeof(buff2), archive_write_data(a, buff2, sizeof(buff2)));

	/* Close out the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Now, read back a truncated version of the archive and
	 * verify that we get an appropriate error. */
	for (i = 1; i < used + 100; i += 100) {
		assert((a = archive_read_new()) != NULL);
		assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
		assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
		if (i < 512) {
			assertEqualIntA(a, ARCHIVE_FATAL, archive_read_open_memory(a, buff, i));
			goto wrap_up;
		} else {
			assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, i));
		}
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

		if (i < 512 + sizeof(buff2)) {
			assertEqualIntA(a, ARCHIVE_FATAL, archive_read_data(a, buff2, sizeof(buff2)));
			goto wrap_up;
		} else {
			assertEqualIntA(a, sizeof(buff2), archive_read_data(a, buff2, sizeof(buff2)));
		}

		/* Verify the end of the archive. */
		/* Archive must be long enough to capture a 512-byte
		 * block of zeroes after the entry.  (POSIX requires a
		 * second block of zeros to be written but libarchive
		 * does not return an error if it can't consume
		 * it.) */
		if (i < 512 + 512*((sizeof(buff2) + 511)/512) + 512) {
			assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
		} else {
			assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
		}
	wrap_up:
		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	}



	/* Same as above, except skip the body instead of reading it. */
	for (i = 1; i < used + 100; i += 100) {
		assert((a = archive_read_new()) != NULL);
		assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
		assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
		if (i < 512) {
			assertEqualIntA(a, ARCHIVE_FATAL, archive_read_open_memory(a, buff, i));
			goto wrap_up2;
		} else {
			assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, i));
		}
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

		if (i < 512 + 512*((sizeof(buff2)+511)/512)) {
			assertEqualIntA(a, ARCHIVE_FATAL, archive_read_data_skip(a));
			goto wrap_up2;
		} else {
			assertEqualIntA(a, ARCHIVE_OK, archive_read_data_skip(a));
		}

		/* Verify the end of the archive. */
		/* Archive must be long enough to capture a 512-byte
		 * block of zeroes after the entry.  (POSIX requires a
		 * second block of zeros to be written but libarchive
		 * does not return an error if it can't consume
		 * it.) */
		if (i < 512 + 512*((sizeof(buff2) + 511)/512) + 512) {
			assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
		} else {
			assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
		}
	wrap_up2:
		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	}
}

/*
 * Cut a cpio archive at every length and check that a failure always comes
 * with an error message, whether the entries are read or skipped.
 */
static void
test_cpio_truncated(int (*set_format)(struct archive *), size_t header_size)
{
	struct archive_entry *ae;
	struct archive *a;
	char tmp[64];
	size_t used, n;
	int r, skip, failed;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, (*set_format)(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file1");
	archive_entry_set_mode(ae, S_IFREG | 0644);
	archive_entry_set_size(ae, 11);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 11, archive_write_data(a, "hello world", 11));
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "link1");
	archive_entry_set_mode(ae, AE_IFLNK | 0777);
	archive_entry_copy_symlink(ae, "file1");
	archive_entry_set_size(ae, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "a_longer_file_name_here");
	archive_entry_set_mode(ae, S_IFREG | 0644);
	archive_entry_set_size(ae, 10);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 10, archive_write_data(a, "0123456789", 10));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	for (skip = 0; skip < 2; skip++) {
		/* Below 6 bytes no format recognizes the archive. */
		for (n = 6; n < used; n++) {
			assert((a = archive_read_new()) != NULL);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_format_all(a));
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_filter_all(a));
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_open_memory(a, buff, n));

			failed = 0;
			while (!failed) {
				r = archive_read_next_header(a, &ae);
				if (r == ARCHIVE_EOF)
					break;
				if (r < ARCHIVE_WARN) {
					failed = 1;
					break;
				}
				if (skip) {
					r = archive_read_data_skip(a);
				} else {
					while ((r = (int)archive_read_data(a,
					    tmp, sizeof(tmp))) > 0)
						;
				}
				if (r < ARCHIVE_WARN)
					failed = 1;
			}
			if (failed) {
				failure("%d of %d bytes, skip=%d", (int)n,
				    (int)used, skip);
				assert(archive_error_string(a) != NULL);
				failure("%d of %d bytes, skip=%d", (int)n,
				    (int)used, skip);
				assert(archive_errno(a) != 0);
			}
			/* Cut in the middle of the first header. */
			if (n == header_size / 2)
				assertEqualString(
				    "End of file trying to read next cpio header",
				    archive_error_string(a));
			/* Cut in the name "file1". */
			if (n == header_size + 2)
				assertEqualString("Truncated cpio file name",
				    archive_error_string(a));
			/* Cut in the data "hello world" while reading it. */
			if (!skip && n == header_size + 6 + 5)
				assertEqualString("Truncated cpio file data",
				    archive_error_string(a));
			assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
			assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		}
	}
}

DEFINE_TEST(test_read_truncated_cpio)
{
	test_cpio_truncated(archive_write_set_format_cpio_odc, 76);
	test_cpio_truncated(archive_write_set_format_cpio_newc, 110);
	test_cpio_truncated(archive_write_set_format_cpio_bin, 26);
}
