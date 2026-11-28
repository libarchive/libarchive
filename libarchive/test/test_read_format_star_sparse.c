/* SPDX-License-Identifier: BSD-2-Clause */
#include "test.h"

/*
 * star -c -sparse artype=xustar f=test.tar sparse next
 * sparse contains one letter at 8192*i+7 for each of 24 extents.
 * This forces a second map block and leaves a partial final data block.
 */
DEFINE_TEST(test_read_format_star_sparse)
{
	const char *refname = "test_read_format_star_sparse.tar.Z";
	char expected[8192] = {0}, buf[8192];
	struct archive_entry *ae;
	struct archive *a;
	int i, skip;

	extract_reference_file(refname);
	for (skip = 0; skip <= 1; ++skip) {
		assert((a = archive_read_new()) != NULL);
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_filter_compress(a));
		assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_open_filename(a, refname, 512));
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualString("sparse", archive_entry_pathname(ae));
		assertEqualInt(23 * 8192 + 8, archive_entry_size(ae));
		assertEqualInt(24, archive_entry_sparse_count(ae));
		if (!skip) {
			for (i = 0; i < 24; ++i) {
				size_t size = i == 23 ? 8 : sizeof(buf);

				expected[7] = (char)('A' + i);
				assertEqualIntA(a, size,
				    archive_read_data(a, buf, size));
				assertEqualMem(expected, buf, size);
			}
			assertEqualIntA(a, 0, archive_read_data(a, buf, 1));
		}
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualString("next", archive_entry_pathname(ae));
		assertEqualIntA(a, 6, archive_read_data(a, buf, sizeof(buf)));
		assertEqualMem("after\n", buf, 6);
		assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	}
}
