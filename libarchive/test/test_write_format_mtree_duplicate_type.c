#include "test.h"

DEFINE_TEST(test_write_format_mtree_duplicate_type)
{
	char buff[4096];
	struct archive *a;
	struct archive_entry *entry;
	size_t used = 0;
	int r;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_options(a, "mtree:sha256"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "b");
	archive_entry_set_mode(entry, AE_IFREG | 0644);
	archive_entry_set_size(entry, 6);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	assertEqualIntA(a, 6, archive_write_data(a, "hello\n", 6));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_finish_entry(a));
	archive_entry_free(entry);

	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "a");
	archive_entry_set_mode(entry, AE_IFDIR | 0755);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_finish_entry(a));
	archive_entry_free(entry);

	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "a");
	archive_entry_set_mode(entry, AE_IFREG | 0644);
	archive_entry_set_size(entry, 6);
	r = archive_write_header(a, entry);
	assertEqualIntA(a, ARCHIVE_FAILED, r);
	if (r >= ARCHIVE_WARN)
		archive_write_data(a, "world\n", 6);
	archive_entry_free(entry);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}
