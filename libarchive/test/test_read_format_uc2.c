/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2026 Eremey Valetov. */
#include "test.h"

DEFINE_TEST(test_read_format_uc2)
{
	struct archive *a = archive_read_new();
	int result = archive_read_support_format_uc2(a);
#ifndef HAVE_LIBUC2READ
	assertEqualInt(ARCHIVE_WARN, result);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
#else
	struct archive_entry *e;
	char data[2000];
	assertEqualInt(ARCHIVE_OK, result);
	/* Keep room for explicit raw fallback after registering every format.
	 */
	struct archive *all = archive_read_new();
	assertEqualInt(ARCHIVE_OK, archive_read_support_format_all(all));
	assertEqualIntA(all, ARCHIVE_OK, archive_read_support_format_raw(all));
	assertEqualInt(ARCHIVE_OK, archive_read_free(all));
	extract_reference_file("test_read_format_uc2_4.uc2");
	assertEqualIntA(
	    a, ARCHIVE_OK,
	    archive_read_open_filename(a, "test_read_format_uc2_4.uc2", 7));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &e));
	assertEqualInt(ARCHIVE_FORMAT_UC2, archive_format(a));
	assertEqualString("fixture.txt", archive_entry_pathname(e));
	assertEqualInt(2000, archive_entry_size(e));
	assertEqualIntA(a, 2000, archive_read_data(a, data, sizeof data));
	for (int i = 0; i < 100; ++i)
		assertEqualMem(data + 20 * i, "UC2 optional reader\n", 20);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &e));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Random-access callbacks must work across arbitrarily short reads. */
	size_t archive_size;
	char *archive_data =
	    slurpfile(&archive_size, "%s", "test_read_format_uc2_4.uc2");
	assert(archive_data != NULL);
	if (archive_data != NULL) {
		const size_t read_sizes[] = {1, 7, 65536};
		char exact_limit[80];
		snprintf(exact_limit, sizeof exact_limit, "uc2:input-limit=%ju",
			 (uintmax_t)archive_size);
		for (unsigned k = 0; k < sizeof read_sizes / sizeof *read_sizes;
		     k++) {
			a = archive_read_new();
			assertEqualInt(ARCHIVE_OK,
				       archive_read_support_format_uc2(a));
			assertEqualIntA(
			    a, ARCHIVE_OK,
			    archive_read_set_options(a, exact_limit));
			assertEqualIntA(a, ARCHIVE_OK,
					read_open_memory_seek(a, archive_data,
							      archive_size,
							      read_sizes[k]));
			assertEqualIntA(a, ARCHIVE_OK,
					archive_read_next_header(a, &e));
			assertEqualIntA(
			    a, 2000, archive_read_data(a, data, sizeof data));
			for (int i = 0; i < 100; i++)
				assertEqualMem(data + 20 * i,
					       "UC2 optional reader\n", 20);
			assertEqualIntA(a, ARCHIVE_EOF,
					archive_read_next_header(a, &e));
			assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		}
		/* A pipe must fail before entries are exposed; no implicit
		 * spooling. */
		a = archive_read_new();
		assertEqualInt(ARCHIVE_OK, archive_read_support_format_uc2(a));
		assertEqualIntA(
		    a, ARCHIVE_OK,
		    read_open_memory(a, archive_data, archive_size, 7));
		assertEqualIntA(a, ARCHIVE_FATAL,
				archive_read_next_header(a, &e));
		assert(strstr(archive_error_string(a),
			      "seekable input required") != NULL);
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		free(archive_data);
	}

	const char *limits[] = {"uc2:memory-limit=1", "uc2:input-limit=20",
				"uc2:entry-limit=1999", "uc2:total-limit=1999"};
	for (unsigned k = 0; k < sizeof limits / sizeof *limits; k++) {
		a = archive_read_new();
		assertEqualInt(ARCHIVE_OK, archive_read_support_format_uc2(a));
		assertEqualIntA(a, ARCHIVE_OK,
				archive_read_set_options(a, limits[k]));
		assertEqualIntA(a, ARCHIVE_OK,
				archive_read_open_filename(
				    a, "test_read_format_uc2_4.uc2", 7));
		if (k < 2)
			assertEqualIntA(a, ARCHIVE_FATAL,
					archive_read_next_header(a, &e));
		else {
			assertEqualIntA(a, ARCHIVE_OK,
					archive_read_next_header(a, &e));
			assertEqualIntA(
			    a, ARCHIVE_FATAL,
			    archive_read_data(a, data, sizeof data));
		}
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	}
	a = archive_read_new();
	assertEqualInt(ARCHIVE_OK, archive_read_support_format_uc2(a));
	extract_reference_file("test_read_format_uc2_80.uc2");
	assertEqualIntA(
	    a, ARCHIVE_OK,
	    archive_read_open_filename(a, "test_read_format_uc2_80.uc2", 7));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &e));
	assertEqualInt(70001, archive_entry_size(e));
	unsigned char *turbo = malloc(70001);
	assert(turbo != NULL);
	if (turbo) {
		assertEqualIntA(a, 70001, archive_read_data(a, turbo, 70001));
		unsigned x = 80;
		for (unsigned i = 0; i < 70001; i++) {
			x = x * 1664525u + 1013904223u;
			assertEqualInt(i < 30000 ? (unsigned char)(x >> 24)
						 : (unsigned char)(i % 251),
				       turbo[i]);
		}
		free(turbo);
	}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	a = archive_read_new();
	assertEqualInt(ARCHIVE_OK, archive_read_support_format_by_code(
				       a, ARCHIVE_FORMAT_UC2));
	extract_reference_file("test_read_format_uc2_6.uc2");
	assertEqualIntA(
	    a, ARCHIVE_OK,
	    archive_read_open_filename(a, "test_read_format_uc2_6.uc2", 7));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &e));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
#endif
}
