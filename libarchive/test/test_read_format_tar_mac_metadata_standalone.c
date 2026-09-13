/*-
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "test.h"

DEFINE_TEST(test_read_format_tar_mac_metadata_standalone)
{
	static const unsigned char appledouble[] = {
		0x00, 0x05, 0x16, 0x07, /* magic */
		0x00, 0x02, 0x00, 0x00, /* version 2 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* filler */
		0x00, 0x01, /* one entry */
		0x00, 0x00, 0x00, 0x02, /* resource fork */
		0x00, 0x00, 0x00, 0x26, /* offset */
		0x00, 0x00, 0x00, 0x04, /* length */
		0xde, 0xad, 0xbe, 0xef,
	};
	static const struct {
		const char *name;
		const void *data;
		size_t size;
	} entries[] = {
		{"._fileC", appledouble, sizeof(appledouble)},
		{"fileC", "content of file C", sizeof("content of file C") - 1},
		{"._fileA", "content of file A", sizeof("content of file A") - 1},
		{"._fileB", appledouble, sizeof(appledouble)},
		{"fileB", "content of file B", sizeof("content of file B") - 1},
		{"._fileD", "content of file D", sizeof("content of file D") - 1},
	};
	static const char long_metadata_path[] =
	    "directory/._aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	static const char long_path[] =
	    "directory/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	char buff[32768], appended[4096], data[64];
	struct archive *a;
	struct archive_entry *ae;
	size_t i, metadata_size, used, appended_used;
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
		archive_entry_set_size(ae, (int64_t)entries[i].size);
		assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
		assertEqualIntA(a, (int)entries[i].size,
		    (int)archive_write_data(a, entries[i].data, entries[i].size));
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
	assertEqualInt(sizeof(appledouble), metadata_size);
	assertEqualMem(appledouble, metadata, metadata_size);
	assertEqualIntA(a, sizeof("content of file C") - 1,
	    archive_read_data(a, data, sizeof(data)));
	assertEqualMem("content of file C", data, sizeof("content of file C") - 1);

	/* A mismatched sidecar remains a standalone entry with its data. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(entries[2].name, archive_entry_pathname(ae));
	assertEqualIntA(a, (int)entries[2].size,
	    (int)archive_read_data(a, data, sizeof(data)));
	assertEqualMem(entries[2].data, data, entries[2].size);

	/* The next sidecar is re-evaluated and attaches to fileB. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(entries[4].name, archive_entry_pathname(ae));
	metadata = archive_entry_mac_metadata(ae, &metadata_size);
	assertEqualInt(sizeof(appledouble), metadata_size);
	assertEqualMem(appledouble, metadata, metadata_size);
	assertEqualIntA(a, (int)entries[4].size,
	    (int)archive_read_data(a, data, sizeof(data)));
	assertEqualMem(entries[4].data, data, entries[4].size);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(entries[5].name, archive_entry_pathname(ae));
	assertEqualIntA(a, (int)entries[5].size,
	    (int)archive_read_data(a, data, sizeof(data)));
	assertEqualMem(entries[5].data, data, entries[5].size);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* A matching filename alone isn't enough to identify AppleDouble data. */
	used = 0;
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "._ordinary");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 1, (int)archive_write_data(a, "x", 1));
	archive_entry_clear(ae);
	archive_entry_set_pathname(ae, "ordinary");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, sizeof("ordinary file") - 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, sizeof("ordinary file") - 1,
	    archive_write_data(a, "ordinary file", sizeof("ordinary file") - 1));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_option(a, "tar", "mac-ext", "1"));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("._ordinary", archive_entry_pathname(ae));
	metadata = archive_entry_mac_metadata(ae, &metadata_size);
	assertEqualInt(0, metadata_size);
	assert(metadata == NULL);
	assertEqualIntA(a, 1, (int)archive_read_data(a, data, sizeof(data)));
	assertEqualMem("x", data, 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("ordinary", archive_entry_pathname(ae));
	metadata = archive_entry_mac_metadata(ae, &metadata_size);
	assertEqualInt(0, metadata_size);
	assert(metadata == NULL);
	assertEqualIntA(a, sizeof("ordinary file") - 1,
	    archive_read_data(a, data, sizeof(data)));
	assertEqualMem("ordinary file", data, sizeof("ordinary file") - 1);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Return a buffered sidecar before reporting a following-header error. */
	used = 0;
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "._truncated");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 1, (int)archive_write_data(a, "x", 1));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	assert(used >= 1025);
	buff[1024] = 'x';
	used = 1025;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_option(a, "tar", "mac-ext", "1"));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("._truncated", archive_entry_pathname(ae));
	assertEqualIntA(a, 1, (int)archive_read_data(a, data, sizeof(data)));
	assertEqualMem("x", data, 1);
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assert(archive_error_string(a) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* EOF after an orphaned sidecar must not expose a following archive. */
	used = 0;
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "._last");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 1, (int)archive_write_data(a, "x", 1));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	appended_used = 0;
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, appended, sizeof(appended), &appended_used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "after");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, sizeof("second archive") - 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, sizeof("second archive") - 1,
	    archive_write_data(a, "second archive", sizeof("second archive") - 1));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	assert(used + appended_used <= sizeof(buff));
	memcpy(buff + used, appended, appended_used);
	used += appended_used;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_option(a, "tar", "mac-ext", "1"));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("._last", archive_entry_pathname(ae));
	assertEqualIntA(a, 1, (int)archive_read_data(a, data, sizeof(data)));
	assertEqualMem("x", data, 1);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	used = 0;
	/* Build a matching pair whose names require PAX extensions. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_pax(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, long_metadata_path);
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, sizeof(appledouble));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, sizeof(appledouble),
	    archive_write_data(a, appledouble, sizeof(appledouble)));
	archive_entry_clear(ae);
	archive_entry_set_pathname(ae, long_path);
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, sizeof("long content") - 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, sizeof("long content") - 1,
	    archive_write_data(a, "long content", sizeof("long content") - 1));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Verify matching after the PAX pathname extensions are applied. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_option(a, "tar", "mac-ext", "1"));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(long_path, archive_entry_pathname(ae));
	metadata = archive_entry_mac_metadata(ae, &metadata_size);
	assertEqualInt(sizeof(appledouble), metadata_size);
	assertEqualMem(appledouble, metadata, metadata_size);
	assertEqualIntA(a, sizeof("long content") - 1,
	    archive_read_data(a, data, sizeof(data)));
	assertEqualMem("long content", data, sizeof("long content") - 1);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* PAX writer/reader round trip for metadata associated with directory . */
	used = 0;
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_pax(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "._.");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, sizeof(appledouble));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, sizeof(appledouble),
	    archive_write_data(a, appledouble, sizeof(appledouble)));
	archive_entry_clear(ae);
	archive_entry_set_pathname(ae, "./");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	archive_entry_set_size(ae, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_option(a, "tar", "mac-ext", "1"));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("./", archive_entry_pathname(ae));
	assertEqualInt(AE_IFDIR, archive_entry_filetype(ae));
	metadata = archive_entry_mac_metadata(ae, &metadata_size);
	assertEqualInt(sizeof(appledouble), metadata_size);
	assertEqualMem(appledouble, metadata, metadata_size);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
