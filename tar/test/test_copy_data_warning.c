/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Martin Mirchev
 * All rights reserved.
 */
#include "test.h"

/*
 * A warning while reading entry data from an @archive input (here an LHA
 * data CRC error) must be reported and reflected in the exit status.
 */
DEFINE_TEST(test_copy_data_warning)
{
	const char *refname = "test_copy_data_warning.lzh";
	char *buff;
	size_t size;
	int r;

	extract_reference_file(refname);

	/* The intact archive copies without errors. */
	r = systemf("%s -cf intact.tar @%s >intact.out 2>intact.err",
	    testprog, refname);
	assertEqualInt(r, 0);
	assertEmptyFile("intact.err");

	/* Change one bit of the compressed data of "file1". */
	buff = slurpfile(&size, "%s", refname);
	assert(buff != NULL && size > 322);
	buff[322] ^= 1;
	assertMakeBinFile("damaged.lzh", 0644, size, buff);
	free(buff);

	r = systemf("%s -cf damaged.tar @damaged.lzh >damaged.out 2>damaged.err",
	    testprog);
	failure("The data CRC error must be reported in the exit status");
	assert(r != 0);
	assertNonEmptyFile("damaged.err");
}
