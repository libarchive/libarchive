/* SPDX-License-Identifier: BSD-2-Clause */

#include "test.h"

DEFINE_TEST(test_read_format_zip_streamable_length_at_end)
{
	const char *refname =
	    "test_read_format_zip_streamable_length_at_end.zip";
	struct archive_entry *entry;
	struct archive *a;
	char *archive_data;
	size_t archive_size;

	extract_reference_file(refname);
	archive_data = slurpfile(&archive_size, "%s", refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_streamable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    read_open_memory(a, archive_data, archive_size, 7));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualString("readme.txt", archive_entry_pathname(entry));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_skip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualString("hidden.sh", archive_entry_pathname(entry));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_skip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualString("b.txt", archive_entry_pathname(entry));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &entry));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(archive_data);
}
