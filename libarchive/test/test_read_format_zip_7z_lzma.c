#include "test.h"
__FBSDID("$FreeBSD$");

DEFINE_TEST(test_read_format_zip_7z_lzma)
{
	const char *refname = "test_read_format_zip_7z_lzma.zip";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
		archive_read_open_filename(a, refname, 10240));

	while(1) {
		int res = archive_read_next_header(a, &ae);
		if (res == ARCHIVE_EOF)
			break;

		assertEqualInt(ARCHIVE_OK, res);
		if (archive_entry_filetype(ae) == AE_IFLNK)
			assertEqualString("../samples/abc_measurement_analysis_sample"
				"/src/abc_measurement_analysis_sample.py",
				archive_entry_symlink(ae));
	}

	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
