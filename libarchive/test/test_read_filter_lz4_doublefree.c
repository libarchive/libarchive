#include "test.h"

/*
 * Regression test for double-free in LZ4 filter (CWE-415).
 *
 * Bug: lz4_allocate_out_block() in archive_read_support_filter_lz4.c
 * frees state->out_block without NULLing the pointer, then updates
 * state->out_block_size before checking if the subsequent malloc
 * succeeded. If malloc fails:
 *   1. state->out_block is a dangling pointer (freed but not NULLed)
 *   2. state->out_block_size is already inflated to the new size
 *   3. lz4_filter_close() calls free(state->out_block) again
 *      -> double-free
 *
 * Test data: Two concatenated LZ4 frames with different block sizes
 *   Frame 1: block_maximum_size = 64KB  (BD=0x40)
 *   Frame 2: block_maximum_size = 4MB   (BD=0x70)
 * This forces the reallocation path in lz4_allocate_out_block().
 *
 * To trigger the double-free, malloc must fail on the 4MB allocation.
 * Run with: LD_PRELOAD=failmalloc.so (fail allocations >= 3MB)
 *
 * Without fix + ASAN + failmalloc:
 *   ASAN detects double-free in lz4_filter_close() -> test crashes
 * With fix + ASAN + failmalloc:
 *   state->out_block is NULLed after free, free(NULL) is safe
 *   -> test passes with ARCHIVE_OK from archive_read_free()
 *
 * Without failmalloc, this test exercises the reallocation path
 * and verifies cleanup does not cause memory safety issues.
 */
DEFINE_TEST(test_read_filter_lz4_doublefree)
{
	const char *refname = "test_read_filter_lz4_doublefree.lz4";
	struct archive *a;
	struct archive_entry *ae;
	int r;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	r = archive_read_support_filter_lz4(a);
	if (r == ARCHIVE_WARN) {
		skipping("lz4 reading not fully supported on this platform");
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		return;
	}
	assertEqualIntA(a, ARCHIVE_OK, r);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_raw(a));

	/*
	 * Open the crafted two-frame LZ4 file. The second frame has
	 * a larger block_maximum_size, triggering reallocation of
	 * the output buffer via lz4_allocate_out_block().
	 *
	 * Under normal conditions, the 4MB malloc succeeds and the
	 * test verifies basic correctness. Under failmalloc, the
	 * malloc fails and the double-free path is exercised.
	 */
	r = archive_read_open_filename(a, refname, 10240);
	if (r == ARCHIVE_OK) {
		if (archive_read_next_header(a, &ae) == ARCHIVE_OK) {
			char buf[1024];
			while (archive_read_data(a, buf, sizeof(buf)) > 0) {}
		}
	}

	/*
	 * archive_read_free triggers lz4_filter_close() which calls
	 * free(state->out_block). Without the fix, if frame 2's
	 * malloc failed, state->out_block is a dangling pointer
	 * and this is a double-free.
	 */
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
