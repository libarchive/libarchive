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

#include <errno.h>

struct read_error_state {
	const unsigned char *data;
	size_t size, position, header_start, fail_at;
	int header_seen, reading_pack, failures, password_calls;
};

static la_ssize_t
read_with_error(struct archive *a, void *data, const void **buff)
{
	struct read_error_state *state = data;
	size_t size = state->size - state->position;

	if (state->reading_pack && state->failures == 0 &&
	    state->position >= state->fail_at &&
	    state->position < state->header_start) {
		state->failures++;
		*buff = NULL;
		archive_set_error(a, EIO, "Injected 7-Zip read failure");
		return (-1);
	}
	if (size > 16)
		size = 16;
	*buff = state->data + state->position;
	state->position += size;
	return ((la_ssize_t)size);
}

static la_int64_t
seek_with_error(struct archive *a, void *data, la_int64_t offset, int whence)
{
	struct read_error_state *state = data;
	la_int64_t base;

	(void)a;
	if (whence == SEEK_SET)
		base = 0;
	else if (whence == SEEK_CUR)
		base = (la_int64_t)state->position;
	else if (whence == SEEK_END)
		base = (la_int64_t)state->size;
	else
		return (-1);
	if (offset < -base || offset > (la_int64_t)state->size - base)
		return (-1);
	state->position = (size_t)(base + offset);
	if (state->position >= state->header_start)
		state->header_seen = 1;
	else if (state->header_seen)
		state->reading_pack = 1;
	return ((la_int64_t)state->position);
}

static const char *
read_error_password(struct archive *a, void *data)
{
	struct read_error_state *state = data;

	(void)a;
	state->password_calls++;
	return (NULL);
}

static void
verify_read_error(const char *ref, size_t fail_at)
{
	struct read_error_state state = {0};
	struct archive *a;
	struct archive_entry *ae;
	char buff[4093], *data;
	uint64_t header_offset = 0;
	size_t i;

	failure("%s, read failure at %lu", ref, (unsigned long)fail_at);
	data = slurpfile(&state.size, "%s", ref);
	assert(data != NULL);
	if (data == NULL)
		return;
	assert(state.size >= 32);
	if (state.size < 32) {
		free(data);
		return;
	}
	state.data = (const unsigned char *)data;
	for (i = 0; i < 8; i++)
		header_offset |= (uint64_t)state.data[12 + i] << (8 * i);
	assert(header_offset <= state.size - 32);
	if (header_offset > state.size - 32) {
		free(data);
		return;
	}
	state.header_start = 32 + (size_t)header_offset;
	state.fail_at = fail_at;
	a = archive_read_new();
	assert(a != NULL);
	if (a == NULL) {
		free(data);
		return;
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_7zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_add_passphrase(a, "12345678"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_passphrase_callback(a, &state, read_error_password));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_seek_callback(a, seek_with_error));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open2(a, &state, NULL, read_with_error, NULL, NULL));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_data(a, buff, sizeof(buff)));
	assertEqualInt(EIO, archive_errno(a));
	assertEqualString("Injected 7-Zip read failure", archive_error_string(a));
	assertEqualInt(1, state.failures);
	assertEqualInt(0, state.password_calls);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(data);
}

/* The ordinary reference data is 70000 low bytes of xorshift32, seed 2.
 * 7-Zip PPMd with password "12345678"; the second archive also encrypts
 * its header. Both archives were independently verified with 7-Zip.
 * Decoding a symbol crosses the 64KiB decrypted-input boundary. */
DEFINE_TEST(test_read_format_7zip_encryption_ppmd)
{
	const char *refs[] = {
	    "test_read_format_7zip_encryption_ppmd.7z",
	    "test_read_format_7zip_encryption_ppmd_header.7z"
	};
	const size_t input_blocks[] = {7, 65536};
	const size_t output_blocks[] = {4093, 65536};
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *expected, *actual;
	uint32_t state = 2;
	size_t i, j, k, total, request;
	la_ssize_t n;

	if (!can7zipEncryption()) {
		skipping("7-Zip AES decryption is not supported on this platform");
		return;
	}
	expected = malloc(70000);
	actual = malloc(65536);
	assert(expected != NULL);
	assert(actual != NULL);
	if (expected == NULL || actual == NULL) {
		free(expected);
		free(actual);
		return;
	}
	for (i = 0; i < 70000; i++) {
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		expected[i] = (unsigned char)state;
	}
	for (i = 0; i < sizeof(refs) / sizeof(refs[0]); i++) {
		extract_reference_file(refs[i]);
		for (j = 0; j < sizeof(input_blocks) / sizeof(input_blocks[0]); j++) {
			for (k = 0; k < sizeof(output_blocks) / sizeof(output_blocks[0]); k++) {
				assert((a = archive_read_new()) != NULL);
				assertEqualIntA(a, ARCHIVE_OK,
				    archive_read_support_format_7zip(a));
				assertEqualIntA(a, ARCHIVE_OK,
				    archive_read_add_passphrase(a, "12345678"));
				assertEqualIntA(a, ARCHIVE_OK,
				    archive_read_open_filename(a, refs[i], input_blocks[j]));
				assertEqualIntA(a, ARCHIVE_OK,
				    archive_read_next_header(a, &ae));
				assertEqualString("data.bin", archive_entry_pathname(ae));
				assertEqualInt(70000, archive_entry_size(ae));
				for (total = 0; total < 70000; total += (size_t)n) {
					request = output_blocks[k];
					if (request > 70000 - total)
						request = 70000 - total;
					n = archive_read_data(a, actual, request);
					assertEqualIntA(a, request, n);
					if (n <= 0 || (size_t)n > request)
						break;
					assertEqualMem(expected + total, actual, (size_t)n);
				}
				assertEqualInt(70000, total);
				assertEqualIntA(a, 0, archive_read_data(a, actual, 1));
				assertEqualIntA(a, ARCHIVE_EOF,
				    archive_read_next_header(a, &ae));
				assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
				assertEqualInt(ARCHIVE_OK, archive_read_free(a));
			}
		}
	}
	free(expected);
	free(actual);
}

/* Reading errors must not consume another password or lose their diagnostic. */
DEFINE_TEST(test_read_format_7zip_encryption_ppmd_read_error)
{
	const char *ref = "test_read_format_7zip_encryption_ppmd.7z";

	if (!can7zipEncryption()) {
		skipping("7-Zip AES decryption is not supported on this platform");
		return;
	}
	extract_reference_file(ref);
	verify_read_error(ref, 48);
	verify_read_error(ref, 65504);
	verify_read_error(ref, 65568);
}
