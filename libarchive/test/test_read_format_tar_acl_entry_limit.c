/*-
 * Copyright (c) 2026 Dickson Wu
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

#define __LIBARCHIVE_TEST
#include "archive_acl_private.h"

static size_t
tar_header(char *h, const char *name, size_t size, char typeflag)
{
	unsigned int sum = 0;
	size_t i;

	memset(h, 0, 512);
	strcpy(h, name);
	strcpy(h + 100, "0000644");
	strcpy(h + 108, "0000000");
	strcpy(h + 116, "0000000");
	snprintf(h + 124, 12, "%011o", (unsigned int)size);
	strcpy(h + 136, "00000000000");
	memset(h + 148, ' ', 8);
	h[156] = typeflag;
	memcpy(h + 257, "ustar", 6);
	memcpy(h + 263, "00", 2);
	for (i = 0; i < 512; i++)
		sum += (unsigned char)h[i];
	snprintf(h + 148, 8, "%06o", sum);
	return (512);
}

static size_t
tar_member(char *p, const char *name, const char *body, size_t size,
    char typeflag)
{
	size_t n = tar_header(p, name, size, typeflag);

	memcpy(p + n, body, size);
	return (n + ((size + 511) & ~(size_t)511));
}

static char *
acl_text(int count, size_t *len)
{
	char *text = malloc(count * 6 + 1);
	int i;

	assert(text != NULL);
	for (i = 0; i < count; i++)
		memcpy(text + i * 6, "u:a:r,", 6);
	text[count * 6 - 1] = '\0';
	*len = count * 6 - 1;
	return (text);
}

static void
read_limited_acl(char *buff, size_t used, const char *message)
{
	struct archive *a;
	struct archive_entry *ae;
	char data[16];

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_WARN, archive_read_next_header(a, &ae));
	assertEqualString(message, archive_error_string(a));
	assertEqualString("f0.txt", archive_entry_pathname(ae));
	assertEqualInt(ARCHIVE_ACL_MAX_ENTRIES,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS) - 3);
	assertEqualInt(6, archive_read_data(a, data, sizeof(data)));
	assertEqualMem("hello\n", data, 6);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_tar_solaris_acl_entry_limit)
{
	size_t len, used = 0;
	char *text = acl_text(ARCHIVE_ACL_MAX_ENTRIES + 1, &len);
	char *body = malloc(len + 9);
	char *buff = malloc(len + 4096);

	assert(body != NULL);
	assert(buff != NULL);
	memcpy(body, "1000000", 8);
	memcpy(body + 8, text, len + 1);
	used += tar_member(buff + used, "acl", body, len + 9, 'A');
	used += tar_member(buff + used, "f0.txt", "hello\n", 6, '0');
	memset(buff + used, 0, 1024);
	used += 1024;
	read_limited_acl(buff, used,
	    "Solaris ACL attribute has more than 16384 entries; "
	    "extra entries ignored");
	free(buff);
	free(body);
	free(text);
}

DEFINE_TEST(test_read_format_tar_pax_acl_entry_limit)
{
	const char *key = " SCHILY.acl.access=";
	size_t len, rlen, digits, used = 0;
	char *text = acl_text(ARCHIVE_ACL_MAX_ENTRIES + 1, &len);
	char *record = malloc(len + 64);
	char *buff = malloc(len + 4096);

	assert(record != NULL);
	assert(buff != NULL);
	rlen = strlen(key) + len + 1;
	for (digits = 1; snprintf(NULL, 0, "%zu", rlen + digits) != (int)digits;)
		digits++;
	rlen += digits;
	snprintf(record, len + 64, "%zu%s%s\n", rlen, key, text);
	assertEqualInt(rlen, strlen(record));
	used += tar_member(buff + used, "PaxHeader/f0.txt", record, rlen, 'x');
	used += tar_member(buff + used, "f0.txt", "hello\n", 6, '0');
	memset(buff + used, 0, 1024);
	used += 1024;
	read_limited_acl(buff, used,
	    "SCHILY.acl.access has more than 16384 entries; "
	    "extra entries ignored");
	free(buff);
	free(record);
	free(text);
}
