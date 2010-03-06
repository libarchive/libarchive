/*-
 * Copyright (c) 2003-2010 Tim Kientzle
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
__FBSDID("$FreeBSD$");

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive_string.h"
#include "archive_write_private.h"

/*
 * Like strdup, but takes start and end pointers.
 */
static char *
strsave(const char *start, const char *end)
{
	char *s = malloc(end - start + 1);
	if (s == NULL)
		return (NULL);
	memcpy(s, start, end - start);
	s[end - start] = '\0';
	return (s);
}

/*
 * Parse the next option from the string.  Returns the length of
 * the recognized option string.  Sets name, key, value to malloc-ed
 * strings holding the components of this option.  Caller is
 * responsible for freeing the name, key, and value.
 */
static int
next_option(const char *_p, char **name, char **key, char **value)
{
	static const char *set_value = "1";
	const char *p, *start;
	char sep;

	*name = *key = NULL;
	*value = strsave(set_value, set_value + 1);

	/* The first separator determines whether the first word is a
	 * name (':'), bare key (\0' or ','), or key followed by value
	 * ('=').
	 */
	p = _p;
	while (*p == ' ' || *p == '\t')
		++p;
	if (*p == '\0')
		return (0);
	if (*p == '!') {
		free(*value);
		*value = NULL;
		++p;
	}
	start = p;
	if (*p < 'a' || *p > 'z')
		return (-1);
	while (*p != ':' && *p != ',' && *p != '=' && *p != '\0') {
		if ((*p < 'a' || *p > 'z')
		    && (*p < '0' || *p > '9')
		    && (*p != '-'))
			return (-1);
		++p;
	}
	sep = *p;
	if (sep == ':')
		*name = strsave(start, p);
	else
		*key = strsave(start, p);
	if (sep != '\0')
		++p;
	switch (sep) {
	case '\0': case ',':
		return p - _p;
	case ':':
		goto key;
	case '=':
		goto value;
	}

key:
	if (*p == '!') {
		free(*value);
		*value = NULL;
		++p;
	}
	start = p;
	while (*p != ',' && *p != '=' && *p != '\0') {
		if ((*p < 'a' || *p > 'z')
		    && (*p < '0' || *p > '9')
		    && (*p != '-'))
			return (-1);
		++p;
	}
	*key = strsave(start, p);
	sep = *p;
	if (*p != '\0')
		++p;
	if (sep != '=')
		return p - _p;

value:
	start = p;
	while (*p != ',' && *p != '\0')
		++p;
	free(*value);
	*value = strsave(start, p);
	if (*p == ',')
		++p;
	return p - _p;
}


/*
 * See if the format can handle this option.
 */
static int
apply_format_option(struct archive_write *a, const char *name,
    const char *key, const char *value)
{
	int r;
	if (name != NULL && strcmp(name, a->format_name) != 0)
		return (ARCHIVE_WARN);
	if (a->format_options == NULL)
		return (ARCHIVE_WARN);
	r = a->format_options(a, key, value);
	if (r == ARCHIVE_FATAL)
		return (r);
	return (r < ARCHIVE_OK ? ARCHIVE_WARN : ARCHIVE_OK);
}

/*
 * See if a filter can handle this option.
 */
static int
apply_filter_option(struct archive_write *a, const char *name,
    const char *key, const char *value)
{
	struct archive_write_filter *filter;
	int r, handled = 0;

	for (filter = a->filter_first; filter != NULL; filter = filter->next_filter) {
		if (filter->options == NULL)
			continue;
		if (name != NULL && strcmp(name, filter->name) != 0)
			continue;
		r = filter->options(filter, key, value);
		if (r == ARCHIVE_FATAL)
			return (r);
		if (r == ARCHIVE_OK)
			++handled;
	}
	return (handled > 0 ? ARCHIVE_OK : ARCHIVE_WARN);
}


/*
 * See if a format or filter can handle this option.
 * Returns ARCHIVE_OK if it is handled by anyone.
 */
static int
apply_option(struct archive_write *a, const char *name,
    const char *key, const char *value)
{
	int r1, r2;

	r1 = apply_format_option(a, name, key, value);
	if (r1 == ARCHIVE_FATAL)
		return (r1);
	r2 = apply_filter_option(a, name, key, value);
	if (r2 == ARCHIVE_FATAL)
		return (r2);
	if (r1 < ARCHIVE_OK && r2 < ARCHIVE_OK)
		return (ARCHIVE_WARN);
	return (ARCHIVE_OK);
}

/*
 * Walk through all of the options and try applying each one.
 * Construct an error message listing all of the unhandled options.
 */
static int
process_options(struct archive *_a, const char *s,
    int(*apply)(struct archive_write *, const char *, const char *, const char *))
{
	struct archive_write *a = (struct archive_write *)_a;
	struct archive_string unhandled;
	char *name, *key, *value;
	int len, r;

	archive_clear_error(&a->archive);
	archive_string_init(&unhandled);

	if (s == NULL || *s == '\0')
		return (ARCHIVE_OK);

	while ((len = next_option(s, &name, &key, &value)) > 0) {
		r = (*apply)(a, name, key, value);
		if (r == ARCHIVE_FATAL) {
			free(name);
			free(key);
			free(value);
			return (r);
		}
		if (r < ARCHIVE_OK) { /* This key was not handled. */
			if (archive_strlen(&unhandled) > 0)
				archive_strcat(&unhandled, ", ");
			archive_strcat(&unhandled, key);
		}
		free(name);
		free(key);
		free(value);
		s += len;
	}
	if (len < 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Malformed options string: %s", s);
		archive_string_free(&unhandled);
		return (ARCHIVE_WARN);
	}
	if (archive_strlen(&unhandled) > 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Unsupported options: %s", unhandled.s);
		archive_string_free(&unhandled);
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
}

/*
 * Set write options for the format. Returns 0 if successful.
 */
int
archive_write_set_format_options(struct archive *a, const char *s)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_format_options");
	return (process_options(a, s, apply_format_option));
}

#if ARCHIVE_VERSION_NUMBER < 4000000
/*
 * Set write options for the filters. Returns 0 if successful.
 */
int
archive_write_set_compressor_options(struct archive *a, const char *s)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_compressor_options");
	return (process_options(a, s, apply_filter_option));
}
#endif

/*
 * Set write options for the filters. Returns 0 if successful.
 */
int
archive_write_set_filter_options(struct archive *a, const char *s)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_compressor_options");
	return (process_options(a, s, apply_filter_option));
}

/*
 * Set write options. Returns 0 if successful.
 */
int
archive_write_set_options(struct archive *a, const char *s)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_compressor_options");
	return (process_options(a, s, apply_option));
}
