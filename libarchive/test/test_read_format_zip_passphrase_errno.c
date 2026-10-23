/*-
 * Copyright (c) 2026 François Degros
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"

/*
 * archive_errno() tells apart the reasons why an encrypted ZIP entry cannot
 * be read, so that a caller needn't parse the error message: no passphrase
 * was supplied, or none of the passphrases that were supplied is right.
 */

/*
 * Tries to read the first entry of the given reference file with the given
 * passphrases, and checks that this fails with the given error code.
 */
static void
check_read_fails(const char *refname, const char **passphrases,
    int want_errno)
{
	struct archive_entry *ae;
	struct archive *a;
	char buff[512];

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	for (; *passphrases != NULL; passphrases++)
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_add_passphrase(a, *passphrases));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_entry_is_data_encrypted(ae));
	assertEqualInt(ARCHIVE_FAILED,
	    archive_read_data(a, buff, sizeof(buff)));
	assertEqualInt(want_errno, archive_errno(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* Checks the three cases with the given reference file. */
static void
check_passphrase_errno(const char *refname)
{
	const char *none[] = { NULL };
	const char *one_wrong[] = { "wrong", NULL };
	const char *two_wrong[] = { "wrong", "also wrong", NULL };

	extract_reference_file(refname);
	check_read_fails(refname, none, ARCHIVE_ERRNO_PASSPHRASE_REQUIRED);
	check_read_fails(refname, one_wrong,
	    ARCHIVE_ERRNO_PASSPHRASE_INCORRECT);
	check_read_fails(refname, two_wrong,
	    ARCHIVE_ERRNO_PASSPHRASE_INCORRECT);
}

DEFINE_TEST(test_read_format_zip_passphrase_errno_traditional)
{
	struct archive *a;

	/* Check if running system has cryptographic functionality. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	if (ARCHIVE_OK != archive_write_set_options(a,
				"zip:encryption=traditional")) {
		skipping("This system does not have cryptographic library");
		archive_write_free(a);
		return;
	}
	archive_write_free(a);

	/* The data of this file is deflated. */
	if (archive_zlib_version() == NULL) {
		skipping("This system does not have zlib");
		return;
	}

	check_passphrase_errno(
	    "test_read_format_zip_traditional_encryption_data.zip");
}

DEFINE_TEST(test_read_format_zip_passphrase_errno_winzip_aes)
{
	struct archive *a;

	/* Check if running system has cryptographic functionality. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	if (ARCHIVE_OK != archive_write_set_options(a,
				"zip:encryption=aes256")) {
		skipping("This system does not have cryptographic library");
		archive_write_free(a);
		return;
	}
	archive_write_free(a);

	/* The data of this file is stored, not deflated. */
	check_passphrase_errno(
	    "test_read_format_zip_winzip_aes256_stored.zip");
}
