/*-
 * Copyright (c) 2007 Joerg Sonnenberger
 * Copyright (c) 2012 Michihiro NAKAJIMA
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

#include "archive_platform.h"
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_set_compression_program.c 201104 2009-12-28 03:14:30Z kientzle $");

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifdef HAVE_ERRNO_H
#  include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_string.h"
#include "archive_write_private.h"
#include "filter_fork.h"

#if ARCHIVE_VERSION_NUMBER < 4000000
int
archive_write_set_compression_program(struct archive *a, const char *cmd)
{
	__archive_write_filters_free(a);
	return (archive_write_add_filter_program(a, cmd));
}
#endif

struct private_data {
	char		*cmd;
	char		**argv;
	struct archive_string description;
#if defined(_WIN32) && !defined(__CYGWIN__)
	HANDLE		 child;
#else
	pid_t		 child;
#endif
	int		 child_stdin, child_stdout;

	char		*child_buf;
	size_t		 child_buf_len, child_buf_avail;
};

static int archive_compressor_program_open(struct archive_write_filter *);
static int archive_compressor_program_write(struct archive_write_filter *,
		    const void *, size_t);
static int archive_compressor_program_close(struct archive_write_filter *);
static int archive_compressor_program_free(struct archive_write_filter *);
static int write_add_filter_programv(struct archive *, const char *,
    char * const *);

/*
 * Add a filter to this write handle that passes all data through an
 * external program.
 */
int
archive_write_add_filter_program(struct archive *_a, const char *cmd)
{
	char *argv[2];
	int r;

	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_program");

	argv[0] = strdup(cmd);
	if (argv[0] == NULL) {
		archive_set_error(_a, ENOMEM,
		    "Can't allocate memory for filter program");
		return (ARCHIVE_FATAL);
	}
	argv[1] = NULL;
	r = write_add_filter_programv(_a, cmd, argv);
	free(argv[0]);
	return (r);
}

int
archive_write_add_filter_programl(struct archive *_a, const char *cmd,
    const char *arg, ...)
{
	va_list ap;
	char **argv;
	char *val;
	int i, r;

	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_programl");

	i = 2;
	if (arg != NULL) {
		va_start(ap, arg);
		while (va_arg(ap, char *) != NULL)
			i++;
		va_end(ap);
	}
	argv = malloc(i * sizeof(char *));
	if (argv == NULL)
		goto memerr;

	if (arg != NULL) {
		argv[0] = strdup(arg);
		if (argv[0] == NULL)
			goto memerr;
		i = 1;
		va_start(ap, arg);
		while ((val = va_arg(ap, char *)) != NULL) {
			argv[i] = strdup(val);
			if (argv[i] == NULL)
				goto memerr;
			i++;	
		}
		va_end(ap);
		argv[i] = NULL;
	} else {
		argv[0] = strdup(cmd);
		if (argv[0] == NULL)
			goto memerr;
		argv[1] = NULL;
	}

	r = write_add_filter_programv(_a, cmd, argv);
	for (i = 0; argv[i] != NULL; i++)
		free(argv[i]);
	free(argv);
	return (r);
memerr:
	if (argv) {
		for (i = 0; argv[i] != NULL; i++)
			free(argv[i]);
		free(argv);
	}
	archive_set_error(_a, ENOMEM,
	    "Can't allocate memory for filter program");
	return (ARCHIVE_FATAL);
}

int
archive_write_add_filter_programv(struct archive *_a, const char *cmd,
    char * const argv[])
{

	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_programv");

	return write_add_filter_programv(_a, cmd, argv);
}

static int
write_add_filter_programv(struct archive *_a, const char *cmd,
    char * const argv[])
{

	struct archive_write_filter *f = __archive_write_allocate_filter(_a);
	struct private_data *data;
	static const char *prefix = "Program: ";
	int i;
	size_t l;

	data = calloc(1, sizeof(*data));
	if (data == NULL)
		goto memerr;
	data->cmd = strdup(cmd);
	if (data->cmd == NULL)
		goto memerr;

	/* Reproduce argv. */
	for (i = 0; argv[i] != NULL; i++)
		;
	data->argv = calloc(i + 1, sizeof(char *));
	if (data->argv == NULL)
		goto memerr;
	l = strlen(prefix) + strlen(cmd) + 1;
	for (i = 0; argv[i] != NULL; i++) {
		data->argv[i] = strdup(argv[i]);
		if (data->argv[i] == NULL)
			goto memerr;
		l += strlen(data->argv[i]) + 1;
	}

	/* Make up a description string. */
	if (archive_string_ensure(&data->description, l) == NULL)
		goto memerr;
	archive_strcpy(&data->description, prefix);
	archive_strcat(&data->description, cmd);
	for (i = 0; argv[i] != NULL; i++) {
		archive_strappend_char(&data->description, ' ');
		archive_strcat(&data->description, data->argv[i]);
	}

	f->name = data->description.s;
	f->data = data;
	f->open = &archive_compressor_program_open;
	f->code = ARCHIVE_COMPRESSION_PROGRAM;
	f->free = archive_compressor_program_free;
	return (ARCHIVE_OK);
memerr:
	free(data);
	archive_compressor_program_free(f);
	archive_set_error(_a, ENOMEM,
	    "Can't allocate memory for filter program");
	return (ARCHIVE_FATAL);
}

/*
 * Setup callback.
 */
static int
archive_compressor_program_open(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
	pid_t child;
	int ret;

	ret = __archive_write_open_filter(f->next_filter);
	if (ret != ARCHIVE_OK)
		return (ret);

	if (data->child_buf == NULL) {
		data->child_buf_len = 65536;
		data->child_buf_avail = 0;
		data->child_buf = malloc(data->child_buf_len);

		if (data->child_buf == NULL) {
			archive_set_error(f->archive, ENOMEM,
			    "Can't allocate compression buffer");
			return (ARCHIVE_FATAL);
		}
	}

	child = __archive_create_child(data->cmd, (char * const *)data->argv,
		 &data->child_stdin, &data->child_stdout);
	if (child == -1) {
		archive_set_error(f->archive, EINVAL,
		    "Can't initialise filter");
		return (ARCHIVE_FATAL);
	}
#if defined(_WIN32) && !defined(__CYGWIN__)
	data->child = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, child);
	if (data->child == NULL) {
		close(data->child_stdin);
		data->child_stdin = -1;
		close(data->child_stdout);
		data->child_stdout = -1;
		archive_set_error(f->archive, EINVAL,
		    "Can't initialise filter");
		return (ARCHIVE_FATAL);
	}
#else
	data->child = child;
#endif

	f->write = archive_compressor_program_write;
	f->close = archive_compressor_program_close;
	return (0);
}

static ssize_t
child_write(struct archive_write_filter *f, const char *buf, size_t buf_len)
{
	struct private_data *data = f->data;
	ssize_t ret;

	if (data->child_stdin == -1)
		return (-1);

	if (buf_len == 0)
		return (-1);

	for (;;) {
		do {
			ret = write(data->child_stdin, buf, buf_len);
		} while (ret == -1 && errno == EINTR);

		if (ret > 0)
			return (ret);
		if (ret == 0) {
			close(data->child_stdin);
			data->child_stdin = -1;
			fcntl(data->child_stdout, F_SETFL, 0);
			return (0);
		}
		if (ret == -1 && errno != EAGAIN)
			return (-1);

		if (data->child_stdout == -1) {
			fcntl(data->child_stdin, F_SETFL, 0);
			__archive_check_child(data->child_stdin,
				data->child_stdout);
			continue;
		}

		do {
			ret = read(data->child_stdout,
			    data->child_buf + data->child_buf_avail,
			    data->child_buf_len - data->child_buf_avail);
		} while (ret == -1 && errno == EINTR);

		if (ret == 0 || (ret == -1 && errno == EPIPE)) {
			close(data->child_stdout);
			data->child_stdout = -1;
			fcntl(data->child_stdin, F_SETFL, 0);
			continue;
		}
		if (ret == -1 && errno == EAGAIN) {
			__archive_check_child(data->child_stdin,
				data->child_stdout);
			continue;
		}
		if (ret == -1)
			return (-1);

		data->child_buf_avail += ret;

		ret = __archive_write_filter(f->next_filter,
		    data->child_buf, data->child_buf_avail);
		if (ret != ARCHIVE_OK)
			return (-1);
		data->child_buf_avail = 0;
	}
}

/*
 * Write data to the compressed stream.
 */
static int
archive_compressor_program_write(struct archive_write_filter *f,
    const void *buff, size_t length)
{
	ssize_t ret;
	const char *buf;

	buf = buff;
	while (length > 0) {
		ret = child_write(f, buf, length);
		if (ret == -1 || ret == 0) {
			archive_set_error(f->archive, EIO,
			    "Can't write to filter");
			return (ARCHIVE_FATAL);
		}
		length -= ret;
		buf += ret;
	}
	return (ARCHIVE_OK);
}


/*
 * Finish the compression...
 */
static int
archive_compressor_program_close(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
	int ret, r1, status;
	ssize_t bytes_read;

	ret = 0;
	close(data->child_stdin);
	data->child_stdin = -1;
	fcntl(data->child_stdout, F_SETFL, 0);

	for (;;) {
		do {
			bytes_read = read(data->child_stdout,
			    data->child_buf + data->child_buf_avail,
			    data->child_buf_len - data->child_buf_avail);
		} while (bytes_read == -1 && errno == EINTR);

		if (bytes_read == 0 || (bytes_read == -1 && errno == EPIPE))
			break;

		if (bytes_read == -1) {
			archive_set_error(f->archive, errno,
			    "Read from filter failed unexpectedly.");
			ret = ARCHIVE_FATAL;
			goto cleanup;
		}
		data->child_buf_avail += bytes_read;

		ret = __archive_write_filter(f->next_filter,
		    data->child_buf, data->child_buf_avail);
		if (ret != ARCHIVE_OK) {
			ret = ARCHIVE_FATAL;
			goto cleanup;
		}
		data->child_buf_avail = 0;
	}

cleanup:
	/* Shut down the child. */
	if (data->child_stdin != -1)
		close(data->child_stdin);
	if (data->child_stdout != -1)
		close(data->child_stdout);
	while (waitpid(data->child, &status, 0) == -1 && errno == EINTR)
		continue;
#if defined(_WIN32) && !defined(__CYGWIN__)
	CloseHandle(data->child);
#endif
	data->child = 0;

	if (status != 0) {
		archive_set_error(f->archive, EIO,
		    "Filter exited with failure.");
		ret = ARCHIVE_FATAL;
	}
	r1 = __archive_write_close_filter(f->next_filter);
	return (r1 < ret ? r1 : ret);
}

static int
archive_compressor_program_free(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;

	if (data) {
#if defined(_WIN32) && !defined(__CYGWIN__)
		if (data->child)
			CloseHandle(data->child);
#endif
		if (data->argv != NULL) {
			int i;
			for (i = 0; data->argv[i] != NULL; i++)
				free(data->argv[i]);
			free(data->argv);
		}
		free(data->cmd);
		archive_string_free(&data->description);
		free(data->child_buf);
		free(data);
		f->data = NULL;
	}
	return (ARCHIVE_OK);
}
