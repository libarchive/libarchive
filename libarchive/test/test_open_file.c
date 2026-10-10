/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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

#if defined(__linux__) && defined(__GLIBC__)
#include <sys/socket.h>
#endif

#if (defined(HAVE_UNISTD_H) && defined(HAVE_PIPE) && \
    (!defined(_WIN32) || defined(__CYGWIN__))) || defined(__GLIBC__)
static void
make_zip(unsigned char *zip, size_t capacity, size_t *used,
    const void *data, size_t size)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_zip_set_compression_store(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, zip, capacity, used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, size);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, size, archive_write_data(a, data, size));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}

static void
check_zip(struct archive *a, const void *data, size_t size)
{
	struct archive_entry *ae;
	void *result;
	int r;

	r = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, r);
	if (r != ARCHIVE_OK)
		return;
	assertEqualString("file", archive_entry_pathname(ae));
	result = malloc(size);
	assert(result != NULL);
	if (result != NULL) {
		assertEqualIntA(a, size, archive_read_data(a, result, size));
		assertEqualMem(data, result, size);
		free(result);
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
}
#endif

DEFINE_TEST(test_open_file)
{
	char buff[64];
	struct archive_entry *ae;
	struct archive *a;
	FILE *f;

	f = fopen("test.7z", "wb");
	assert(f != NULL);
	if (f == NULL)
		return;

	/* Write an archive through this FILE *. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_7zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_FILE(a, f));

	/*
	 * Write a file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 0);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, 8);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 8, archive_write_data(a, "12345678", 9));

	/*
	 * Write a second file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file2");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, 819200);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Close out the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	fclose(f);

	/*
	 * Now, read the data back. 7z requiring seeking, that also
	 * tests that the seeking support works.
	 */
	f = fopen("test.7z", "rb");
	assert(f != NULL);
	if (f == NULL)
		return;
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_FILE(a, f));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("file", archive_entry_pathname(ae));
	assert((S_IFREG | 0755) == archive_entry_mode(ae));
	assertEqualInt(8, archive_entry_size(ae));
	assertEqualIntA(a, 8, archive_read_data(a, buff, 10));
	assertEqualMem(buff, "12345678", 8);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assert((S_IFREG | 0755) == archive_entry_mode(ae));
	assertEqualInt(819200, archive_entry_size(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_skip(a));

	/* Verify the end of the archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	fclose(f);
}

DEFINE_TEST(test_open_file_pipe_zip)
{
#if defined(HAVE_UNISTD_H) && defined(HAVE_PIPE) && \
    (!defined(_WIN32) || defined(__CYGWIN__))
	unsigned char zip[2048];
	size_t used = 0;
	struct archive *a;
	FILE *f;
	int fd[2];

	make_zip(zip, sizeof(zip), &used, "data", 4);
	if (!assertEqualInt(0, pipe(fd)))
		return;
	assertEqualInt(used, write(fd[1], zip, used));
	assertEqualInt(0, close(fd[1]));
	assert((f = fdopen(fd[0], "rb")) != NULL);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_FILE(a, f));
	check_zip(a, "data", 4);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assertEqualInt(0, fclose(f));
#else
	skipping("pipe() is unavailable on this platform");
#endif
}

DEFINE_TEST(test_open_file_socket_zip)
{
#if defined(__linux__) && defined(__GLIBC__)
	unsigned char zip[2048];
	size_t used = 0;
	struct archive *a;
	FILE *f;
	int fd[2];

	make_zip(zip, sizeof(zip), &used, "data", 4);
	if (!assertEqualInt(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fd)))
		return;
	assertEqualInt(used, write(fd[1], zip, used));
	assertEqualInt(0, close(fd[1]));
	assert((f = fdopen(fd[0], "rb")) != NULL);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_FILE(a, f));
	check_zip(a, "data", 4);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assertEqualInt(0, fclose(f));
#else
	skipping("socket test requires Linux and glibc");
#endif
}

DEFINE_TEST(test_open_file_memory_zip)
{
#if defined(__GLIBC__)
	unsigned char zip[2048];
	size_t used = 0;
	struct archive *a;
	FILE *f;

	make_zip(zip, sizeof(zip), &used, "data", 4);
	assert((f = fmemopen(zip, used, "rb")) != NULL);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_FILE(a, f));
	check_zip(a, "data", 4);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assertEqualInt(0, fclose(f));
#else
	skipping("fmemopen test requires glibc");
#endif
}

#if defined(__GLIBC__)
struct file_cookie {
	const unsigned char *data;
	size_t size;
	size_t position;
	int moved_end;
	int fail_restore;
	int rejected_end;
	int restored;
};

static ssize_t
cookie_read(void *opaque, char *buffer, size_t size)
{
	struct file_cookie *cookie = opaque;
	size_t available = cookie->size - cookie->position;

	if (size > available)
		size = available;
	memcpy(buffer, cookie->data + cookie->position, size);
	cookie->position += size;
	return (size);
}

static int
cookie_seek(void *opaque, off64_t *offset, int whence)
{
	struct file_cookie *cookie = opaque;
	off64_t position;

	if (whence == SEEK_END) {
		cookie->rejected_end++;
		if (cookie->moved_end)
			cookie->position = cookie->size;
		errno = EIO;
		return (-1);
	}
	if (whence == SEEK_SET && cookie->rejected_end) {
		if (cookie->fail_restore) {
			errno = EIO;
			return (-1);
		}
		cookie->restored++;
	}
	position = *offset;
	if (whence == SEEK_CUR)
		position += cookie->position;
	if (position < 0 || (uint64_t)position > cookie->size) {
		errno = EINVAL;
		return (-1);
	}
	cookie->position = position;
	*offset = position;
	return (0);
}

static int
cookie_close(void *opaque)
{
	(void)opaque;
	return (0);
}
#endif

DEFINE_TEST(test_open_file_failed_seek)
{
#if defined(__GLIBC__)
	const size_t payload_size = 160 * 1024;
	struct file_cookie cookie;
	cookie_io_functions_t io = { cookie_read, NULL, cookie_seek,
	    cookie_close };
	struct archive *a;
	unsigned char *zip;
	char *payload;
	FILE *f;
	size_t used = 0;
	int moved;
	int r;

	zip = malloc(payload_size + 2048);
	payload = malloc(payload_size);
	assert(zip != NULL);
	assert(payload != NULL);
	if (zip == NULL || payload == NULL) {
		free(zip);
		free(payload);
		return;
	}
	memset(payload, 'x', payload_size);
	make_zip(zip, payload_size + 2048, &used, payload, payload_size);
	for (moved = 0; moved <= 1; moved++) {
		memset(&cookie, 0, sizeof(cookie));
		cookie.data = zip;
		cookie.size = used;
		cookie.moved_end = moved;
		assert((f = fopencookie(&cookie, "rb", io)) != NULL);
		assertEqualInt(0, setvbuf(f, NULL, _IONBF, 0));
		assert((a = archive_read_new()) != NULL);
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_format_zip(a));
		assertEqualIntA(a, ARCHIVE_OK, archive_read_open_FILE(a, f));
		assert(cookie.rejected_end > 0);
		assert(cookie.restored > 0);
		assertEqualInt(0, ferror(f));
		check_zip(a, payload, payload_size);
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		assertEqualInt(0, fclose(f));
	}

	memset(&cookie, 0, sizeof(cookie));
	cookie.data = zip;
	cookie.size = used;
	cookie.moved_end = 1;
	cookie.fail_restore = 1;
	assert((f = fopencookie(&cookie, "rb", io)) != NULL);
	assertEqualInt(0, setvbuf(f, NULL, _IONBF, 0));
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	r = archive_read_open_FILE(a, f);
	if (r == ARCHIVE_OK) {
		struct archive_entry *ae;
		r = archive_read_next_header(a, &ae);
	}
	assertEqualIntA(a, ARCHIVE_FATAL, r);
	assert(cookie.rejected_end > 0);
	assertEqualInt(0, cookie.restored);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assertEqualInt(0, fclose(f));
	free(zip);
	free(payload);
#else
	skipping("fopencookie test requires glibc");
#endif
}

#if defined(__GLIBC__)
static ssize_t
cookie_read_error(void *opaque, char *buffer, size_t size)
{
	(void)opaque;
	(void)buffer;
	(void)size;
	errno = EIO;
	return (-1);
}
#endif

DEFINE_TEST(test_open_file_read_error)
{
#if defined(__GLIBC__)
	cookie_io_functions_t io = { cookie_read_error, NULL, NULL,
	    cookie_close };
	struct archive *a;
	FILE *f;

	assert((f = fopencookie(NULL, "rb", io)) != NULL);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_open_FILE(a, f));
	assertEqualInt(EIO, archive_errno(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assertEqualInt(0, fclose(f));
#else
	skipping("fopencookie test requires glibc");
#endif
}
