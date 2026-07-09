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
#include "test.h"
#include "test_clone.h"

DEFINE_TEST(test_read_data_into_fd_clone)
{
	char *big, *small_data, *buff;
	size_t used;
	struct archive *a;
	struct archive_entry *ae;
	int fd, out;

	big = malloc(BIG);
	small_data = malloc(SMALL);
	buff = malloc(BIG + 100000 + PREFIX);
	assert(big != NULL && small_data != NULL && buff != NULL);
	fill(big, BIG, 'A');
	fill(small_data, SMALL, 'z');
	used = make_tar(buff, BIG + 100000, big, small_data);
	memmove(buff + PREFIX, buff, used);
	memset(buff, 'P', PREFIX);
	assertMakeBinFile("prefixed.tar", 0644, (int)used + PREFIX, buff);

	fd = open("prefixed.tar", O_RDONLY | O_BINARY);
	assert(fd >= 0);
	assertEqualInt(PREFIX, lseek(fd, PREFIX, SEEK_SET));
	a = new_reader();
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_fd(a, fd, 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualStringA(a, "big", archive_entry_pathname(ae));
	out = open("big", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(out >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, out));
	assertEqualInt(0, close(out));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualStringA(a, "small", archive_entry_pathname(ae));
	out = open("small", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(out >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, out));
	assertEqualInt(0, close(out));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assertEqualInt(0, close(fd));
	assertFileContents(big, BIG, "big");
	assertFileContents(small_data, SMALL, "small");

	free(big);
	free(small_data);
	free(buff);
}

DEFINE_TEST(test_read_data_into_fd_clone_truncated)
{
#if HAVE_FTRUNCATE
	char *big, *small_data, *buff;
	size_t used;
	struct archive *a;
	struct archive_entry *ae;
	int fd, out, truncfd;

	big = malloc(BIG);
	small_data = malloc(SMALL);
	buff = malloc(BIG + 100000);
	assert(big != NULL && small_data != NULL && buff != NULL);
	fill(big, BIG, 'A');
	fill(small_data, SMALL, 'z');
	used = make_tar(buff, BIG + 100000, big, small_data);
	assertMakeBinFile("truncated.tar", 0644, (int)used, buff);
	fd = open("truncated.tar", O_RDONLY | O_BINARY);
	assert(fd >= 0);
	a = new_reader();
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_fd(a, fd, 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	truncfd = open("truncated.tar", O_WRONLY | O_BINARY);
	out = open("output", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(truncfd >= 0 && out >= 0);
	assertEqualInt(0, ftruncate(truncfd, (off_t)(used / 2)));
	assertEqualInt(0, close(truncfd));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_data_into_fd(a, out));
	assertEqualInt(0, close(out));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assertEqualInt(0, close(fd));

	free(big);
	free(small_data);
	free(buff);
#else
	skipping("ftruncate is not available");
#endif
}

DEFINE_TEST(test_read_data_into_fd_clone_cpio_symlink)
{
	char buff[4096], small_data[SMALL];
	size_t used;
	struct archive *a, *w;
	struct archive_entry *ae;
	int out;

	fill(small_data, sizeof(small_data), 'z');
	assert((w = archive_write_new()) != NULL);
	assertEqualIntA(w, ARCHIVE_OK, archive_write_set_format_cpio_newc(w));
	assertEqualIntA(w, ARCHIVE_OK, archive_write_add_filter_none(w));
	assertEqualIntA(w, ARCHIVE_OK,
	    archive_write_open_memory(w, buff, sizeof(buff), &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "symlink");
	archive_entry_set_filetype(ae, AE_IFLNK);
	archive_entry_set_perm(ae, 0755);
	archive_entry_set_size(ae, 0);
	archive_entry_set_symlink(ae, "target/elsewhere");
	assertEqualIntA(w, ARCHIVE_OK, archive_write_header(w, ae));
	archive_entry_clear(ae);
	archive_entry_set_pathname(ae, "after");
	archive_entry_set_mode(ae, S_IFREG | 0644);
	archive_entry_set_size(ae, SMALL);
	assertEqualIntA(w, ARCHIVE_OK, archive_write_header(w, ae));
	assertEqualIntA(w, SMALL, archive_write_data(w, small_data, SMALL));
	archive_entry_free(ae);
	assertEqualIntA(w, ARCHIVE_OK, archive_write_close(w));
	assertEqualInt(ARCHIVE_OK, archive_write_free(w));
	assertMakeBinFile("archive.cpio", 0644, (int)used, buff);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_cpio(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, "archive.cpio", 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualStringA(a, "symlink", archive_entry_pathname(ae));
	assertEqualString("target/elsewhere", archive_entry_symlink(ae));
	out = open("symlink_data", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(out >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, out));
	assertEqualInt(0, close(out));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualStringA(a, "after", archive_entry_pathname(ae));
	out = open("after", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(out >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, out));
	assertEqualInt(0, close(out));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assertFileSize("symlink_data", 0);
	assertFileContents(small_data, SMALL, "after");
}

DEFINE_TEST(test_read_data_into_fd_clone_all_hole)
{
	char buff[16384], after[SMALL];
	size_t used;
	struct archive *a;
	struct archive_entry *ae;
	int out;

	fill(after, sizeof(after), 'z');
	used = make_all_hole_tar(buff, sizeof(buff), after);
	assertMakeBinFile("sparse.tar", 0644, (int)used, buff);

	a = new_reader();
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, "sparse.tar", 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualStringA(a, "hole", archive_entry_pathname(ae));
	assertEqualInt(SPARSE_SIZE, archive_entry_size(ae));
	assertEqualInt(0, archive_entry_sparse_count(ae));
	out = open("hole", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(out >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, out));
	assertEqualInt(0, close(out));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualStringA(a, "after", archive_entry_pathname(ae));
	out = open("after", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(out >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, out));
	assertEqualInt(0, close(out));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assertFileSize("hole", 0);
	assertFileContents(after, sizeof(after), "after");
}

DEFINE_TEST(test_read_data_into_fd_clone_sun_hole)
{
	char buff[16384], after[SMALL];
	size_t used;
	struct archive *a;
	struct archive_entry *ae;
	int out;

	fill(after, sizeof(after), 'z');
	used = make_sun_hole_tar(buff, sizeof(buff), after);
	assertMakeBinFile("sparse.tar", 0644, (int)used, buff);
	a = new_reader();
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, "sparse.tar", 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(SUN_HOLE_SIZE, archive_entry_size(ae));
	assertEqualInt(0, archive_entry_sparse_count(ae));
	out = open("hole", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(out >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, out));
	assertEqualInt(0, close(out));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualStringA(a, "after", archive_entry_pathname(ae));
	out = open("after", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(out >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, out));
	assertEqualInt(0, close(out));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assertFileSize("hole", 0);
	assertFileContents(after, sizeof(after), "after");
}

DEFINE_TEST(test_read_data_into_fd_clone_short_sparse)
{
	char buff[16384], after[SMALL];
	size_t used;
	struct archive *a;
	struct archive_entry *ae;
	int out;

	fill(after, sizeof(after), 'z');
	used = make_short_sparse_tar(buff, sizeof(buff), after);
	assertMakeBinFile("sparse.tar", 0644, (int)used, buff);
	a = new_reader();
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, "sparse.tar", 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(SPARSE_SIZE, archive_entry_size(ae));
	assertEqualInt(0, archive_entry_sparse_count(ae));
	out = open("short", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(out >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, out));
	assertEqualInt(0, close(out));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualStringA(a, "after", archive_entry_pathname(ae));
	out = open("after", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(out >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, out));
	assertEqualInt(0, close(out));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assertFileSize("short", 0);
	assertFileContents(after, sizeof(after), "after");
}
