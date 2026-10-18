/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Eduard Zateev
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_copy_archive_warning)
{
	const char *refname = "test_copy_lha_bad_crc.lzh";
	FILE *f;
	int c;

	extract_reference_file(refname);
	f = fopen(refname, "r+b");
	assert(f != NULL);
	if (f == NULL)
		return;

	assertEqualInt(0, fseek(f, 322, SEEK_SET));
	c = fgetc(f);
	assert(c != EOF);
	if (c == EOF) {
		fclose(f);
		return;
	}
	assertEqualInt(0, fseek(f, -1, SEEK_CUR));
	assertEqualInt(c ^ 1, fputc(c ^ 1, f));
	fclose(f);

	failure("bsdtar must report warnings while copying archive entries");
	assertEqualInt(1, systemf("%s -cf out.tar @%s >stdout 2>stderr",
	    testprog, refname));
	assertNonEmptyFile("stderr");
}
