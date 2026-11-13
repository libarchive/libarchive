/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Wleeaf
 * All rights reserved.
 */
#include "test.h"

/*
 * Pax times are decimal numbers of seconds, so "-1.5" is one and a half
 * seconds before the epoch: -2 seconds plus 500000000 nanoseconds.
 */
DEFINE_TEST(test_pax_negative_fraction_read)
{
	static const struct {
		const char *name;
		int64_t sec;
		long nsec;
	} expected[] = {
		{ "a", -1, 500000000 },	/* -0.5 */
		{ "b", -2, 500000000 },	/* -1.5 */
		{ "c", -1, 999999999 },	/* -0.000000001 */
		{ "d", -2, 999999998 },	/* -1.0000000011 */
		{ "e", -2, 999999999 },	/* -1.0000000010 */
		{ "f", 1, 500000000 },	/* 1.5 */
	};
	char name[] = "test_pax_negative_fraction.tar";
	struct archive_entry *ae;
	struct archive *a;
	size_t i;

	extract_reference_file(name);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, name, 10240));
	for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualString(expected[i].name, archive_entry_pathname(ae));
		assertEqualInt(expected[i].sec, archive_entry_mtime(ae));
		assertEqualInt(expected[i].nsec, archive_entry_mtime_nsec(ae));
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static int
has_record(const char *buff, size_t used, const char *record)
{
	size_t len = strlen(record);
	size_t i;

	for (i = 0; i + len <= used; i++)
		if (memcmp(buff + i, record, len) == 0)
			return (1);
	return (0);
}

DEFINE_TEST(test_pax_negative_fraction_write)
{
	static const struct {
		int64_t sec;
		long nsec;
		const char *record;
	} times[] = {
		{ -1, 500000000, " mtime=-0.5\n" },
		{ -2, 250000000, " mtime=-1.75\n" },
		{ -1, 1, " mtime=-0.999999999\n" },
		{ -3, 0, " mtime=-3\n" },
		{ 1, 500000000, " mtime=1.5\n" },
	};
	char buff[32768];
	struct archive_entry *ae;
	struct archive *a;
	size_t used, i;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_pax(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));
	for (i = 0; i < sizeof(times) / sizeof(times[0]); i++) {
		assert((ae = archive_entry_new()) != NULL);
		archive_entry_set_pathname(ae, "file");
		archive_entry_set_filetype(ae, AE_IFREG);
		archive_entry_set_perm(ae, 0644);
		archive_entry_set_mtime(ae, (time_t)times[i].sec, times[i].nsec);
		assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
		archive_entry_free(ae);
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	for (i = 0; i < sizeof(times) / sizeof(times[0]); i++) {
		failure("%s", times[i].record);
		assert(has_record(buff, used, times[i].record));
	}

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	for (i = 0; i < sizeof(times) / sizeof(times[0]); i++) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualInt(times[i].sec, archive_entry_mtime(ae));
		assertEqualInt(times[i].nsec, archive_entry_mtime_nsec(ae));
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
