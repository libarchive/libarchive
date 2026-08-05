/*-
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "test.h"

DEFINE_TEST(test_read_format_tar_mac_metadata_standalone)
{
	static const struct {
		const char *name;
		const char *data;
	} entries[] = {
		{"._fileC", "metadata for file C"},
		{"fileC", "content of file C"},
		{"._fileA", "content of file A"},
		{"._fileB", "content of file B"},
	};
	char buff[10240], data[32];
	struct archive *a;
	struct archive_entry *ae;
	size_t i, metadata_size, used;
	const void *metadata;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));
	assert((ae = archive_entry_new()) != NULL);
	for (i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
		archive_entry_clear(ae);
		archive_entry_set_pathname(ae, entries[i].name);
		archive_entry_set_mode(ae, AE_IFREG | 0644);
		archive_entry_set_size(ae, strlen(entries[i].data));
		assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
		assertEqualIntA(a, (int)strlen(entries[i].data),
		    (int)archive_write_data(a, entries[i].data,
		    strlen(entries[i].data)));
	}
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_option(a, "tar", "mac-ext", "1"));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("fileC", archive_entry_pathname(ae));
	metadata = archive_entry_mac_metadata(ae, &metadata_size);
	assertEqualInt(sizeof("metadata for file C") - 1, metadata_size);
	assertEqualMem("metadata for file C", metadata, metadata_size);
	assertEqualIntA(a, sizeof("content of file C") - 1,
	    archive_read_data(a, data, sizeof(data)));
	assertEqualMem("content of file C", data, sizeof("content of file C") - 1);

	for (i = 2; i < sizeof(entries) / sizeof(entries[0]); i++) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualString(entries[i].name, archive_entry_pathname(ae));
		assertEqualIntA(a, (int)strlen(entries[i].data),
		    (int)archive_read_data(a, data, sizeof(data)));
		assertEqualMem(entries[i].data, data, strlen(entries[i].data));
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
