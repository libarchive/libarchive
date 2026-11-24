/*-
 * Copyright (c) 2026 Corey Daley
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
 * Symlinks in encrypted Zip archives. 7-Zip and Info-ZIP encrypt a
 * symlink's target as they do a file's data; the reference files were
 * made with
 *   zip -X -y -P password1234 ..._zipcrypto.zip link file.txt
 *   7zz a -tzip -snl -mem=AES256 -ppassword1234 ..._aes.zip link file.txt
 * where link -> target/elsewhere.
 */

#define	TARGET		"target/elsewhere"
#define	PASSPHRASE	"password1234"

static int
contains(const char *buff, size_t size, const char *s)
{
	size_t i, n = strlen(s);

	for (i = 0; i + n <= size; i++)
		if (memcmp(buff + i, s, n) == 0)
			return (1);
	return (0);
}

/*
 * The streaming reader can't tell a symlink from a file when the mode is
 * only in the central directory, as it is in these archives; it reports a
 * file whose data is the target. Either way the target must decrypt.
 */
static void
verify_entries(struct archive *a, int with_passphrase, int seekable)
{
	struct archive_entry *ae;
	char buff[64];
	int r, links = 0, files = 0;

	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK) {
		if (strcmp(archive_entry_pathname(ae), "link") == 0) {
			++links;
			assertEqualInt(1, archive_entry_is_data_encrypted(ae));
			if (!seekable && archive_entry_filetype(ae) == AE_IFREG) {
				if (with_passphrase) {
					assertEqualInt(16, archive_read_data(a,
					    buff, sizeof(buff)));
					assertEqualMem(buff, TARGET, 16);
				}
				continue;
			}
			assertEqualInt(AE_IFLNK, archive_entry_filetype(ae));
			if (with_passphrase)
				assertEqualString(TARGET,
				    archive_entry_symlink(ae));
			else
				/* Never the ciphertext. */
				assert(archive_entry_symlink(ae) == NULL ||
				    archive_entry_symlink(ae)[0] == '\0');
		} else {
			++files;
			assertEqualString("file.txt",
			    archive_entry_pathname(ae));
			if (with_passphrase) {
				assertEqualInt(14,
				    archive_read_data(a, buff, sizeof(buff)));
				assertEqualMem(buff, "file contents\n", 14);
			}
		}
	}
	assertEqualIntA(a, ARCHIVE_EOF, r);
	assertEqualInt(1, links);
	assertEqualInt(1, files);
}

static void
read_reference(const char *refname, int seekable, int with_passphrase)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	if (seekable)
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_format_zip_seekable(a));
	else
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_format_zip_streamable(a));
	if (with_passphrase)
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_add_passphrase(a, PASSPHRASE));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));
	verify_entries(a, with_passphrase, seekable);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
read_all_ways(const char *refname)
{
	extract_reference_file(refname);
	read_reference(refname, 1, 1);
	read_reference(refname, 0, 1);
	read_reference(refname, 1, 0);
	read_reference(refname, 0, 0);
}

DEFINE_TEST(test_read_format_zip_encrypted_symlink_zipcrypto)
{
	read_all_ways("test_read_format_zip_encrypted_symlink_zipcrypto.zip");
}

DEFINE_TEST(test_read_format_zip_encrypted_symlink_aes)
{
	struct archive *a;

	/* Skip if there is no AES support. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	if (ARCHIVE_OK != archive_write_set_options(a, "zip:encryption=aes256")) {
		skipping("This system does not have cryptographic library");
		archive_write_free(a);
		return;
	}
	archive_write_free(a);

	read_all_ways("test_read_format_zip_encrypted_symlink_aes.zip");
}

static void
write_and_read(const char *encryption)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t buffsize = 8192, used;
	char *buff;
	char option[64];

	buff = malloc(buffsize);
	assert(buff != NULL);

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	snprintf(option, sizeof(option), "zip:encryption=%s", encryption);
	if (ARCHIVE_OK != archive_write_set_options(a, option)) {
		skipping("This system does not have cryptographic library");
		archive_write_free(a);
		free(buff);
		return;
	}
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_passphrase(a, PASSPHRASE));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link");
	archive_entry_set_mode(ae, AE_IFLNK | 0755);
	archive_entry_copy_symlink(ae, TARGET);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file.txt");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 14);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(14, archive_write_data(a, "file contents\n", 14));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* The target isn't in the archive in plain text. */
	failure("%s: the symlink target is stored unencrypted", encryption);
	assert(!contains(buff, used, TARGET));

	/* It reads back, by both strategies, with and without the
	 * passphrase. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_add_passphrase(a, PASSPHRASE));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory_seek(a, buff, used, 7));
	verify_entries(a, 1, 1);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_add_passphrase(a, PASSPHRASE));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory(a, buff, used, 7));
	verify_entries(a, 1, 0);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory_seek(a, buff, used, 7));
	verify_entries(a, 0, 1);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(buff);
}

DEFINE_TEST(test_write_format_zip_encrypted_symlink_zipcrypto)
{
	write_and_read("zipcrypt");
}

DEFINE_TEST(test_write_format_zip_encrypted_symlink_aes128)
{
	write_and_read("aes128");
}

DEFINE_TEST(test_write_format_zip_encrypted_symlink_aes256)
{
	write_and_read("aes256");
}
