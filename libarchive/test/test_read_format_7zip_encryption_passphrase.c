/*-
 * Copyright (c) 2026 Witt Kung
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

enum password_candidate {
	PASSWORD_NONE,
	PASSWORD_WRONG,
	PASSWORD_OTHER_WRONG,
	PASSWORD_CORRECT,
	PASSWORD_TOO_LONG,
	PASSWORD_INVALID_UTF8,
	PASSWORD_UNUSED
};

enum password_callback_mode {
	CALLBACK_NONE,
	CALLBACK_CORRECT,
	CALLBACK_WRONG_THEN_CORRECT,
	CALLBACK_EXHAUSTED,
	CALLBACK_INVALID_THEN_CORRECT
};

struct password_test {
	const char *name;
	enum password_candidate candidates[3];
	enum password_callback_mode callback;
	int succeeds;
	int callback_calls;
};

static const struct password_test password_tests[] = {
	{"next registered password", {PASSWORD_WRONG, PASSWORD_CORRECT},
	    CALLBACK_NONE, 1, 0},
	{"registered password then callback", {PASSWORD_WRONG},
	    CALLBACK_CORRECT, 1, 1},
	{"callback retries", {PASSWORD_NONE},
	    CALLBACK_WRONG_THEN_CORRECT, 1, 2},
	{"registered password avoids callback", {PASSWORD_CORRECT},
	    CALLBACK_CORRECT, 1, 0},
	{"third registered password",
	    {PASSWORD_WRONG, PASSWORD_OTHER_WRONG, PASSWORD_CORRECT},
	    CALLBACK_NONE, 1, 0},
	{"callback exhausted", {PASSWORD_WRONG},
	    CALLBACK_EXHAUSTED, 0, 1},
	{"registered passwords exhausted", {PASSWORD_WRONG, PASSWORD_OTHER_WRONG},
	    CALLBACK_NONE, 0, 0},
	{"callback only", {PASSWORD_NONE},
	    CALLBACK_CORRECT, 1, 1},
	{"correct password before unused candidate", {PASSWORD_CORRECT, PASSWORD_UNUSED},
	    CALLBACK_NONE, 1, 0},
	{"oversized registered password", {PASSWORD_TOO_LONG, PASSWORD_CORRECT},
	    CALLBACK_NONE, 1, 0},
	{"invalid UTF-8 registered password", {PASSWORD_INVALID_UTF8, PASSWORD_CORRECT},
	    CALLBACK_NONE, 1, 0},
	{"invalid UTF-8 callback password", {PASSWORD_NONE},
	    CALLBACK_INVALID_THEN_CORRECT, 1, 2}
};

struct password_state {
	int calls;
	int wrong_first;
	int exhausted;
	int invalid_first;
};

static const char *
password_callback(struct archive *a, void *data)
{
	struct password_state *state = data;

	(void)a;
	state->calls++;
	if (state->exhausted || state->calls > state->wrong_first + 1)
		return (NULL);
	if (state->calls <= state->wrong_first)
		return (state->invalid_first ? "\xff" : "another wrong password");
	return ("12345678");
}

static void
verify_passwords(const char *ref, const struct password_test *test,
    const char *wrong_password)
{
	struct archive *a, *control;
	struct archive_entry *ae, *ce;
	struct password_state state = {0,
	    test->callback == CALLBACK_WRONG_THEN_CORRECT ||
	    test->callback == CALLBACK_INVALID_THEN_CORRECT,
	    test->callback == CALLBACK_EXHAUSTED,
	    test->callback == CALLBACK_INVALID_THEN_CORRECT};
	unsigned char buff[4093], expected[4093];
	char unsupported[258];
	const char *passwords[] = {NULL, wrong_password, "another wrong password",
	    "12345678", unsupported, "\xff", "unused wrong password"};
	size_t i;
	la_ssize_t n, count;
	int r;

	failure("%s, %s", ref, test->name);
	memset(unsupported, 'a', sizeof(unsupported) - 1);
	unsupported[sizeof(unsupported) - 1] = '\0';
	assert((a = archive_read_new()) != NULL);
	assert((control = archive_read_new()) != NULL);
	if (a == NULL || control == NULL)
		goto done;
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_7zip(a));
	assertEqualIntA(control, ARCHIVE_OK,
	    archive_read_support_format_7zip(control));
	assertEqualIntA(control, ARCHIVE_OK,
	    archive_read_add_passphrase(control, "12345678"));
	for (i = 0; i < sizeof(test->candidates) / sizeof(test->candidates[0]); i++) {
		if (test->candidates[i] == PASSWORD_NONE)
			break;
		assertEqualIntA(a, ARCHIVE_OK, archive_read_add_passphrase(a,
		    passwords[test->candidates[i]]));
	}
	if (test->callback != CALLBACK_NONE)
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_set_passphrase_callback(a, &state,
		    password_callback));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, ref, 7));
	assertEqualIntA(control, ARCHIVE_OK,
	    archive_read_open_filename(control, ref, 65536));
	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK) {
		if (!test->succeeds) {
			r = (int)archive_read_data(a, buff, sizeof(buff));
			assert(r < 0);
			break;
		}
		assertEqualIntA(control, ARCHIVE_OK,
		    archive_read_next_header(control, &ce));
		assertEqualString(archive_entry_pathname(ce),
		    archive_entry_pathname(ae));
		assertEqualInt(archive_entry_size(ce), archive_entry_size(ae));
		while ((n = archive_read_data(control, expected,
		    sizeof(expected))) > 0) {
			count = archive_read_data(a, buff, (size_t)n);
			assertEqualIntA(a, n, count);
			if (count != n)
				break;
			assertEqualMem(expected, buff, (size_t)n);
		}
		assertEqualIntA(control, 0, n);
		assertEqualIntA(a, 0, archive_read_data(a, buff, 1));
	}
	if (test->succeeds) {
		assertEqualIntA(a, ARCHIVE_EOF, r);
		assertEqualIntA(control, ARCHIVE_EOF,
		    archive_read_next_header(control, &ce));
	} else {
		assert(r < ARCHIVE_OK);
		assertEqualInt(-1, archive_errno(a));
		assertEqualString("Incorrect passphrase", archive_error_string(a));
	}
	assertEqualInt(test->callback_calls, state.calls);
 done:
	if (a != NULL)
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	if (control != NULL)
		assertEqualInt(ARCHIVE_OK, archive_read_free(control));
}

struct folder_passwords {
	int calls;
	char password[32];
};

static const char *
folder_password_callback(struct archive *a, void *data)
{
	struct folder_passwords *state = data;

	(void)a;
	if (++state->calls > 2)
		return (NULL);
	/* Reuse callback storage to check that the reader owns its copy. */
	snprintf(state->password, sizeof(state->password), "%s",
	    state->calls == 1 ? "12345678" : "other-password");
	return (state->password);
}

static void
verify_folder_passwords(int callback, int reverse)
{
	const char *ref = "test_read_format_7zip_encryption_passphrase_folders.7z";
	const char *names[] = {"first.txt", "second.txt"};
	const char *lines[] = {"first password\n", "second password\n"};
	struct folder_passwords state = {0, {0}};
	struct archive *a;
	struct archive_entry *ae;
	char buff[256], expected[256];
	size_t i, j, size;
	int r;

	assert((a = archive_read_new()) != NULL);
	if (a == NULL)
		return;
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_7zip(a));
	if (callback) {
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_set_passphrase_callback(a, &state,
		    folder_password_callback));
	} else {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_add_passphrase(a,
		    reverse ? "other-password" : "12345678"));
		assertEqualIntA(a, ARCHIVE_OK, archive_read_add_passphrase(a,
		    reverse ? "12345678" : "other-password"));
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, ref, 7));
	for (i = 0; i < 2; i++) {
		r = archive_read_next_header(a, &ae);
		assertEqualIntA(a, ARCHIVE_OK, r);
		if (r != ARCHIVE_OK)
			break;
		assertEqualString(names[i], archive_entry_pathname(ae));
		size = strlen(lines[i]);
		for (j = 0; j < 16; j++)
			memcpy(expected + j * size, lines[i], size);
		assertEqualIntA(a, size * 16,
		    archive_read_data(a, buff, sizeof(buff)));
		assertEqualMem(expected, buff, size * 16);
		assertEqualIntA(a, 0, archive_read_data(a, buff, 1));
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(callback ? 2 : 0, state.calls);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* Copy references: 7zz a -m0=Copy -p12345678 -mhe=off/on, with
 * copy.txt containing 4096 repetitions of "7zip password candidate test\n".
 * The folder reference was updated with first.txt/password 12345678,
 * then second.txt/password other-password, using -m0=Copy -mhe=off.
 * The Zstandard reference wraps zstd-compressed data in the same AES
 * coder, uses the writer's five-byte Zstandard properties, and includes
 * the decoded CRC. Its content is 4096 repetitions of
 * "Zstandard password candidate regression\n". */
DEFINE_TEST(test_read_format_7zip_encryption_passphrase)
{
	const char *refs[] = {
	    "test_read_format_7zip_encryption.7z",
	    "test_read_format_7zip_encryption_header.7z",
	    "test_read_format_7zip_encryption_lzma2.7z",
	    "test_read_format_7zip_encryption_ppmd.7z",
	    "test_read_format_7zip_encryption_ppmd_header.7z",
	    "test_read_format_7zip_encryption_bcj2.7z",
	    "test_read_format_7zip_encryption_bcj2_header.7z",
	    "test_read_format_7zip_encryption_bcj2_four.7z",
	    "test_read_format_7zip_encryption_bcj2_mixed.7z",
	    "test_read_format_7zip_encryption_passphrase_copy.7z",
	    "test_read_format_7zip_encryption_passphrase_copy_header.7z",
	    "test_read_format_7zip_encryption_passphrase_zstd.7z"
	};
	size_t i, j;

	if (!can7zipEncryption() || archive_liblzma_version() == NULL) {
		skipping("7-Zip AES and LZMA decryption are required");
		return;
	}
	extract_reference_file("test_read_format_7zip_encryption_passphrase_folders.7z");
	verify_folder_passwords(0, 0);
	verify_folder_passwords(0, 1);
	verify_folder_passwords(1, 0);
	for (i = 0; i < sizeof(refs) / sizeof(refs[0]); i++) {
		if (i == 8 && (archive_bzlib_version() == NULL ||
		    archive_zlib_version() == NULL)) {
			skipping("Mixed BCJ2 inputs require BZip2 and Deflate");
			continue;
		}
		if (i == 11 && archive_libzstd_version() == NULL) {
			skipping("Zstandard input requires libzstd");
			continue;
		}
		extract_reference_file(refs[i]);
		for (j = 0; j < sizeof(password_tests) / sizeof(password_tests[0]); j++)
			verify_passwords(refs[i], &password_tests[j], "wrong password");
	}
}

/* AES (one SHA-256 round, no salt or IV) followed by Deflate. The content
 * is 200 repetitions of "ordinary Deflate password retry\n". Independently
 * verified with 7-Zip. "candidate-4" decrypts to an early Deflate end marker:
 * that must reject this candidate, not prevent trying the correct password. */
DEFINE_TEST(test_read_format_7zip_encryption_passphrase_deflate)
{
	const char *ref = "test_read_format_7zip_encryption_passphrase_deflate.7z";
	size_t i;

	if (!can7zipEncryption() || archive_zlib_version() == NULL) {
		skipping("7-Zip AES and Deflate decryption are required");
		return;
	}
	extract_reference_file(ref);
	for (i = 0; i < sizeof(password_tests) / sizeof(password_tests[0]); i++)
		verify_passwords(ref, &password_tests[i], "candidate-4");
}
