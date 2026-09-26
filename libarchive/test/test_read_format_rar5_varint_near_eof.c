#include "test.h"

/*
 * A single-entry RAR5 archive whose file header ends immediately in
 * front of the end-of-archive block: the last fields of the header
 * end within 10 bytes of the end of the file.  read_var() used to
 * demand 10 bytes of look-ahead unconditionally, so such well-formed
 * values were rejected and the entry vanished behind a silent
 * ARCHIVE_EOF.
 */
DEFINE_TEST(test_read_format_rar5_varint_near_eof)
{
	const char *reffile = "test_read_format_rar5_varint_near_eof.rar";
	struct archive *a;
	struct archive_entry *ae;

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_rar5(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 10240));

	/* The entry must be returned instead of silently dropped. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("a", archive_entry_pathname(ae));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
