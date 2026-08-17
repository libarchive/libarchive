/*-
 * Copyright (c) 2026 libarchive contributors
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

#ifndef TEST_CLONE_H
#define TEST_CLONE_H

#define BIG 200000
#define SMALL 37
#define PREFIX 123
#define SPARSE_SIZE 4096
#define SUN_HOLE_SIZE 512

static void
fill(char *p, size_t n, char seed)
{
	size_t i;

	for (i = 0; i < n; i++)
		p[i] = (char)(seed + (char)(i & 0x3f));
}

static struct archive *
new_reader(void)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_none(a));
	return (a);
}

static size_t
make_tar(char *buff, size_t buffsize, const char *big, const char *small_data)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t used;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_pax_restricted(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "big");
	archive_entry_set_mode(ae, S_IFREG | 0644);
	archive_entry_set_size(ae, BIG);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, BIG, archive_write_data(a, big, BIG));
	archive_entry_clear(ae);
	archive_entry_set_pathname(ae, "small");
	archive_entry_set_mode(ae, S_IFREG | 0600);
	archive_entry_set_size(ae, SMALL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, SMALL, archive_write_data(a, small_data, SMALL));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	return (used);
}

static size_t
make_all_hole_tar(char *buff, size_t buffsize, const char *after)
{
	char unused = 0;
	size_t i, used;
	struct archive *a;
	struct archive_entry *ae;
	int found = 0;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_pax(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "hole");
	archive_entry_set_mode(ae, S_IFREG | 0644);
	archive_entry_set_size(ae, SPARSE_SIZE);
	archive_entry_sparse_add_entry(ae, 0, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 0, archive_write_data(a, &unused, 0));
	archive_entry_clear(ae);
	archive_entry_set_pathname(ae, "after");
	archive_entry_set_mode(ae, S_IFREG | 0644);
	archive_entry_set_size(ae, SMALL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, SMALL, archive_write_data(a, after, SMALL));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	for (i = 0; i + 512 <= used; i += 512) {
		if (memcmp(buff + i, "2\n0\n0\n", 6) == 0) {
			memset(buff + i, 0, 512);
			memcpy(buff + i, "0\n", 2);
			found = 1;
			break;
		}
	}
	assertEqualInt(1, found);
	return (used);
}

static size_t
make_sun_hole_tar(char *buff, size_t buffsize, const char *after)
{
	static const char gnu_attr[] = "28 GNU.sparse.realsize=4096\n";
	static const char sun_attr[] = "28 SUN.holesdata= 000000512\n";
	size_t i, used;
	int found_attr = 0, found_body = 0;

	used = make_all_hole_tar(buff, buffsize, after);
	for (i = 0; i + sizeof(gnu_attr) - 1 <= used; i++) {
		if (memcmp(buff + i, gnu_attr, sizeof(gnu_attr) - 1) == 0) {
			memcpy(buff + i, sun_attr, sizeof(sun_attr) - 1);
			found_attr = 1;
			break;
		}
	}
	for (i = 0; i + 11 <= used; i++) {
		if (memcmp(buff + i, "GNU.sparse.", 11) == 0)
			buff[i] = 'X';
	}
	for (i = 0; i + 512 <= used; i += 512) {
		if (buff[i] == '0' && buff[i + 1] == '\n'
		    && buff[i + 2] == '\0') {
			memset(buff + i, 'H', 512);
			found_body = 1;
			break;
		}
	}
	assertEqualInt(1, found_attr);
	assertEqualInt(1, found_body);
	return (used);
}

static size_t
make_short_sparse_tar(char *buff, size_t buffsize, const char *after)
{
	size_t i, used;
	int found = 0;

	used = make_all_hole_tar(buff, buffsize, after);
	for (i = 0; i + 512 <= used; i += 512) {
		if (buff[i] == '0' && buff[i + 1] == '\n'
		    && buff[i + 2] == '\0') {
			memset(buff + i, 0, 512);
			memcpy(buff + i, "1\n0\n4096\n", 9);
			found = 1;
			break;
		}
	}
	assertEqualInt(1, found);
	return (used);
}

#endif
