/* SPDX-License-Identifier: BSD-2-Clause */

#include "test.h"

DEFINE_TEST(test_read_format_tar_sparse_remainder)
{
	const char *refname = "test_read_format_tar_sparse_remainder.tar";
	struct archive_entry *entry;
	struct archive *a;
	const void *data;
	size_t size;
	int64_t offset;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualString("sparse.dat", archive_entry_pathname(entry));
	assertEqualInt(4096, archive_entry_size(entry));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_data_block(a, &data, &size, &offset));
	assertEqualInt(512, size);
	assertEqualInt(1024, offset);
	assertEqualIntA(a, ARCHIVE_EOF,
	    archive_read_data_block(a, &data, &size, &offset));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualString("b.txt", archive_entry_pathname(entry));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualString("c.txt", archive_entry_pathname(entry));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &entry));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
