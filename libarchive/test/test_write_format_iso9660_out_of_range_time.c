#include "test.h"

DEFINE_TEST(test_write_format_iso9660_out_of_range_time)
{
	static const unsigned char identifier[] = "LEAK.TXT;1";
	static const unsigned char zero_time[7] = {0};
	struct archive *a;
	struct archive_entry *entry;
	unsigned char *buff;
	size_t buffsize = 1024 * 1024;
	size_t i;
	size_t used = 0;
	int found = 0;

	if (sizeof(time_t) < sizeof(int64_t)) {
		skipping("time_t cannot represent the out-of-range timestamp");
		return;
	}

	buff = malloc(buffsize);
	if (!assert(buff != NULL))
		return;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_iso9660(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));

	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "leak.txt");
	archive_entry_set_mode(entry, AE_IFREG | 0644);
	archive_entry_set_size(entry, 0);
	archive_entry_set_mtime(entry, INT64_MAX, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	for (i = 33; i + sizeof(identifier) - 1 <= used; i++) {
		if (memcmp(buff + i, identifier, sizeof(identifier) - 1) == 0) {
			assertEqualMem(zero_time, buff + i - 33 + 18,
			    sizeof(zero_time));
			found = 1;
			break;
		}
	}
	assert(found);
	free(buff);
}
