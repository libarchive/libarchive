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

#include "archive_platform.h"

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_private.h"

/* The longest chain of filter extensions that is taken into account. */
#define	MAX_FILTERS	8

/*
 * The longest filename extension, without its dot. It must be at least as
 * long as the longest extension in the tables below, or that entry never
 * matches.
 */
#define	MAX_EXTENSION	10

/* A filter code that stands for "lzma or lzip", as in ".tlz". */
#define	FILTER_LZMA_OR_LZIP	(-1)

enum format {
	FORMAT_7ZIP,
	FORMAT_AR,
	FORMAT_CAB,
	FORMAT_CPIO,
	FORMAT_ISO9660,
	FORMAT_LHA,
	FORMAT_MTREE,
	FORMAT_RAR,
	FORMAT_RAW,
	FORMAT_RPM,
	FORMAT_TAR,
	FORMAT_WARC,
	FORMAT_XAR,
	FORMAT_ZIP
};

struct extension {
	const char	*name;
	int		 value;
};

/*
 * Extensions of a compression or an encoding applied to a single file.
 */
static const struct extension filter_extensions[] = {
	{"bz", ARCHIVE_FILTER_BZIP2},
	{"bz2", ARCHIVE_FILTER_BZIP2},
	{"bzip2", ARCHIVE_FILTER_BZIP2},
	{"grz", ARCHIVE_FILTER_GRZIP},
	{"grzip", ARCHIVE_FILTER_GRZIP},
	{"gz", ARCHIVE_FILTER_GZIP},
	{"gzip", ARCHIVE_FILTER_GZIP},
	{"lrz", ARCHIVE_FILTER_LRZIP},
	{"lrzip", ARCHIVE_FILTER_LRZIP},
	{"lz", ARCHIVE_FILTER_LZIP},
	{"lz4", ARCHIVE_FILTER_LZ4},
	{"lzip", ARCHIVE_FILTER_LZIP},
	{"lzma", ARCHIVE_FILTER_LZMA},
	{"lzo", ARCHIVE_FILTER_LZOP},
	{"lzop", ARCHIVE_FILTER_LZOP},
	{"uu", ARCHIVE_FILTER_UU},
	{"uue", ARCHIVE_FILTER_UU},
	{"xz", ARCHIVE_FILTER_XZ},
	{"z", ARCHIVE_FILTER_COMPRESS},
	{"zst", ARCHIVE_FILTER_ZSTD},
	{"zstd", ARCHIVE_FILTER_ZSTD},
	{NULL, 0}
};

/* Extensions that stand for a compressed tar archive, such as ".tgz". */
static const struct extension tar_extensions[] = {
	{"taz", ARCHIVE_FILTER_COMPRESS},
	{"tb2", ARCHIVE_FILTER_BZIP2},
	{"tbz", ARCHIVE_FILTER_BZIP2},
	{"tbz2", ARCHIVE_FILTER_BZIP2},
	{"tgz", ARCHIVE_FILTER_GZIP},
	{"tlrz", ARCHIVE_FILTER_LRZIP},
	{"tlz", FILTER_LZMA_OR_LZIP},
	{"tlz4", ARCHIVE_FILTER_LZ4},
	{"tlzip", ARCHIVE_FILTER_LZIP},
	{"tlzma", ARCHIVE_FILTER_LZMA},
	{"tlzo", ARCHIVE_FILTER_LZOP},
	{"tlzop", ARCHIVE_FILTER_LZOP},
	{"txz", ARCHIVE_FILTER_XZ},
	{"tz", ARCHIVE_FILTER_COMPRESS},
	{"tz2", ARCHIVE_FILTER_BZIP2},
	{"tzs", ARCHIVE_FILTER_ZSTD},
	{"tzst", ARCHIVE_FILTER_ZSTD},
	{"tzstd", ARCHIVE_FILTER_ZSTD},
	{NULL, 0}
};

/* Extensions that stand for a compressed cpio archive. */
static const struct extension cpio_extensions[] = {
	{"cpgz", ARCHIVE_FILTER_GZIP},
	{NULL, 0}
};

/* Extensions of archive formats that can be read straight from the file. */
static const struct extension format_extensions[] = {
	{"3mf", FORMAT_ZIP},
	{"7z", FORMAT_7ZIP},
	{"7zip", FORMAT_7ZIP},
	{"a", FORMAT_AR},
	{"aab", FORMAT_ZIP},
	{"aar", FORMAT_ZIP},
	{"apk", FORMAT_ZIP},
	{"apkm", FORMAT_ZIP},
	{"apks", FORMAT_ZIP},
	{"appx", FORMAT_ZIP},
	{"appxbundle", FORMAT_ZIP},
	{"ar", FORMAT_AR},
	{"cab", FORMAT_CAB},
	{"cb7", FORMAT_7ZIP},
	{"cbr", FORMAT_RAR},
	{"cbt", FORMAT_TAR},
	{"cbz", FORMAT_ZIP},
	{"conda", FORMAT_ZIP},
	{"cpio", FORMAT_CPIO},
	{"crx", FORMAT_ZIP},
	{"ddeb", FORMAT_AR},
	{"deb", FORMAT_AR},
	{"docm", FORMAT_ZIP},
	{"docx", FORMAT_ZIP},
	{"dotm", FORMAT_ZIP},
	{"dotx", FORMAT_ZIP},
	{"ear", FORMAT_ZIP},
	{"egg", FORMAT_ZIP},
	{"epub", FORMAT_ZIP},
	{"gtar", FORMAT_TAR},
	{"ipa", FORMAT_ZIP},
	{"ipsw", FORMAT_ZIP},
	{"iso", FORMAT_ISO9660},
	{"iso9660", FORMAT_ISO9660},
	{"jar", FORMAT_ZIP},
	{"kmz", FORMAT_ZIP},
	{"kra", FORMAT_ZIP},
	{"lha", FORMAT_LHA},
	{"lzh", FORMAT_LHA},
	{"mcaddon", FORMAT_ZIP},
	{"mcpack", FORMAT_ZIP},
	{"mctemplate", FORMAT_ZIP},
	{"mcworld", FORMAT_ZIP},
	{"msix", FORMAT_ZIP},
	{"msixbundle", FORMAT_ZIP},
	{"msu", FORMAT_CAB},
	{"mtree", FORMAT_MTREE},
	{"nupkg", FORMAT_ZIP},
	{"odb", FORMAT_ZIP},
	{"odc", FORMAT_ZIP},
	{"odf", FORMAT_ZIP},
	{"odg", FORMAT_ZIP},
	{"odi", FORMAT_ZIP},
	{"odm", FORMAT_ZIP},
	{"odp", FORMAT_ZIP},
	{"ods", FORMAT_ZIP},
	{"odt", FORMAT_ZIP},
	{"ora", FORMAT_ZIP},
	{"otc", FORMAT_ZIP},
	{"otf", FORMAT_ZIP},
	{"otg", FORMAT_ZIP},
	{"oti", FORMAT_ZIP},
	{"otp", FORMAT_ZIP},
	{"ots", FORMAT_ZIP},
	{"ott", FORMAT_ZIP},
	{"oxps", FORMAT_ZIP},
	{"pk3", FORMAT_ZIP},
	{"pk4", FORMAT_ZIP},
	{"potm", FORMAT_ZIP},
	{"potx", FORMAT_ZIP},
	{"ppam", FORMAT_ZIP},
	{"ppsm", FORMAT_ZIP},
	{"ppsx", FORMAT_ZIP},
	{"pptx", FORMAT_ZIP},
	{"rar", FORMAT_RAR},
	{"rpm", FORMAT_RPM},
	{"sb3", FORMAT_ZIP},
	{"sldm", FORMAT_ZIP},
	{"sldx", FORMAT_ZIP},
	{"snupkg", FORMAT_ZIP},
	{"spm", FORMAT_RPM},
	{"sxc", FORMAT_ZIP},
	{"sxd", FORMAT_ZIP},
	{"sxg", FORMAT_ZIP},
	{"sxi", FORMAT_ZIP},
	{"sxm", FORMAT_ZIP},
	{"sxw", FORMAT_ZIP},
	{"tar", FORMAT_TAR},
	{"thmx", FORMAT_ZIP},
	{"udeb", FORMAT_AR},
	{"usdz", FORMAT_ZIP},
	{"vsdm", FORMAT_ZIP},
	{"vsdx", FORMAT_ZIP},
	{"vsix", FORMAT_ZIP},
	{"vssm", FORMAT_ZIP},
	{"vssx", FORMAT_ZIP},
	{"vstm", FORMAT_ZIP},
	{"vstx", FORMAT_ZIP},
	{"war", FORMAT_ZIP},
	{"warc", FORMAT_WARC},
	{"whl", FORMAT_ZIP},
	{"xapk", FORMAT_ZIP},
	{"xar", FORMAT_XAR},
	{"xlam", FORMAT_ZIP},
	{"xlsb", FORMAT_ZIP},
	{"xlsm", FORMAT_ZIP},
	{"xlsx", FORMAT_ZIP},
	{"xltm", FORMAT_ZIP},
	{"xltx", FORMAT_ZIP},
	{"xpi", FORMAT_ZIP},
	{"xps", FORMAT_ZIP},
	{"zip", FORMAT_ZIP},
	{"zipx", FORMAT_ZIP},
	{NULL, 0}
};

/* What the filename tells about how to read the file. */
struct plan {
	int		filters[MAX_FILTERS];
	int		filter_count;
	enum format	format;
	size_t		stem_length;
};

static int
is_separator(char c)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	if (c == '\\')
		return (1);
#endif
	return (c == '/');
}

static const struct extension *
find_extension(const struct extension *table, const char *name)
{
	for (; table->name != NULL; table++) {
		if (strcmp(table->name, name) == 0)
			return (table);
	}
	return (NULL);
}

/*
 * Finds the final filename extension in the first `length` bytes of `name`.
 * Stores the position of its dot in `*dot`, and the extension in lower case
 * and without its dot in `ext`. Returns 0 if there is no extension.
 *
 * A name that starts with a dot has no extension: ".gz" is a hidden file,
 * not a compressed file without a name. An extension has no space, is not
 * empty and is at most MAX_EXTENSION bytes long.
 */
static int
final_extension(const char *name, size_t length, size_t *dot,
    char ext[MAX_EXTENSION + 1])
{
	size_t start = 0, i, n;

	for (i = 0; i < length; i++) {
		if (is_separator(name[i]))
			start = i + 1;
	}

	i = length;
	while (i > start && name[i - 1] != '.' && name[i - 1] != ' ')
		i--;
	if (i <= start + 1 || name[i - 1] != '.')
		return (0);
	*dot = i - 1;

	n = length - i;
	if (n == 0 || n > MAX_EXTENSION)
		return (0);

	/* There must be something other than dots before the extension. */
	for (i = *dot; i > start && name[i - 1] == '.'; i--)
		;
	if (i == start)
		return (0);

	for (i = 0; i < n; i++) {
		char c = name[*dot + 1 + i];
		ext[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
	}
	ext[n] = '\0';
	return (1);
}

/* Finds the filter and the format of a compressed archive such as ".tgz". */
static int
find_compressed_archive(const char *ext, int *filter, enum format *format)
{
	const struct extension *e;

	if ((e = find_extension(tar_extensions, ext)) != NULL)
		*format = FORMAT_TAR;
	else if ((e = find_extension(cpio_extensions, ext)) != NULL)
		*format = FORMAT_CPIO;
	else
		return (0);
	*filter = e->value;
	return (1);
}

static int
make_plan(const char *name, size_t length, struct plan *plan)
{
	const struct extension *e;
	char ext[MAX_EXTENSION + 1];
	size_t dot;
	int filter;

	memset(plan, 0, sizeof(*plan));

	if (!final_extension(name, length, &dot, ext))
		return (0);

	if (find_compressed_archive(ext, &filter, &plan->format)) {
		plan->filters[plan->filter_count++] = filter;
		plan->stem_length = dot;
		return (1);
	}

	if ((e = find_extension(format_extensions, ext)) != NULL) {
		plan->format = (enum format)e->value;
		plan->stem_length = dot;
		return (1);
	}

	if ((e = find_extension(filter_extensions, ext)) == NULL)
		return (0);

	/* One or more filters, then maybe an archive format. */
	do {
		plan->filters[plan->filter_count++] = e->value;
		length = dot;
		e = NULL;
		if (plan->filter_count < MAX_FILTERS &&
		    final_extension(name, length, &dot, ext))
			e = find_extension(filter_extensions, ext);
	} while (e != NULL);

	/* A compressed archive can follow the filters: "backup.tgz.uu". */
	if (plan->filter_count < MAX_FILTERS &&
	    final_extension(name, length, &dot, ext) &&
	    find_compressed_archive(ext, &filter, &plan->format)) {
		plan->filters[plan->filter_count++] = filter;
		plan->stem_length = dot;
		return (1);
	}

	/*
	 * Any archive format can follow the filters, except:
	 * - ZIP and 7-Zip: their readers need to seek, which a decompression
	 *   filter cannot do;
	 * - RPM: the filters of its payload bid for it, but libarchive stops
	 *   bidding for filters once one has been appended.
	 * A file such as "foo.7z.gz" is read as a plain compressed file.
	 */
	plan->format = FORMAT_RAW;
	if (final_extension(name, length, &dot, ext) &&
	    (e = find_extension(format_extensions, ext)) != NULL &&
	    e->value != FORMAT_ZIP && e->value != FORMAT_7ZIP &&
	    e->value != FORMAT_RPM) {
		plan->format = (enum format)e->value;
		length = dot;
	}
	plan->stem_length = length;
	return (1);
}

static int
support_format(struct archive *a, enum format format)
{
	int r;

	switch (format) {
	case FORMAT_7ZIP:
		return (archive_read_support_format_7zip(a));
	case FORMAT_AR:
		return (archive_read_support_format_ar(a));
	case FORMAT_CAB:
		return (archive_read_support_format_cab(a));
	case FORMAT_CPIO:
		return (archive_read_support_format_cpio(a));
	case FORMAT_ISO9660:
		return (archive_read_support_format_iso9660(a));
	case FORMAT_LHA:
		return (archive_read_support_format_lha(a));
	case FORMAT_MTREE:
		return (archive_read_support_format_mtree(a));
	case FORMAT_RAR:
		if ((r = archive_read_support_format_rar(a)) < ARCHIVE_WARN)
			return (r);
		return (archive_read_support_format_rar5(a));
	case FORMAT_RAW:
		/*
		 * The raw reader needs at least a byte, so an empty file
		 * is left to the empty reader, as bsdcat does.
		 */
		if ((r = archive_read_support_format_empty(a)) < ARCHIVE_WARN)
			return (r);
		return (archive_read_support_format_raw(a));
	case FORMAT_RPM:
		/*
		 * The RPM filter skips the headers. The payload is a cpio
		 * archive, compressed in a way that only its contents tell,
		 * so these filters must bid for it, not be appended.
		 */
		if ((r = archive_read_support_filter_rpm(a)) < ARCHIVE_WARN)
			return (r);
		if ((r = archive_read_support_filter_bzip2(a)) < ARCHIVE_WARN)
			return (r);
		if ((r = archive_read_support_filter_gzip(a)) < ARCHIVE_WARN)
			return (r);
		if ((r = archive_read_support_filter_lzip(a)) < ARCHIVE_WARN)
			return (r);
		if ((r = archive_read_support_filter_lzma(a)) < ARCHIVE_WARN)
			return (r);
		if ((r = archive_read_support_filter_xz(a)) < ARCHIVE_WARN)
			return (r);
		if ((r = archive_read_support_filter_zstd(a)) < ARCHIVE_WARN)
			return (r);
		return (archive_read_support_format_cpio(a));
	case FORMAT_TAR:
		if ((r = archive_read_support_format_empty(a)) < ARCHIVE_WARN)
			return (r);
		return (archive_read_support_format_tar(a));
	case FORMAT_WARC:
		return (archive_read_support_format_warc(a));
	case FORMAT_XAR:
		return (archive_read_support_format_xar(a));
	case FORMAT_ZIP:
		/*
		 * The streaming ZIP reader only sees the local headers, so it
		 * doesn't know the Unix modes or the symlinks, and it can't
		 * know the size of an entry that has a data descriptor.
		 */
		return (archive_read_support_format_zip_seekable(a));
	}
	return (ARCHIVE_FATAL);
}

static int
append_filter(struct archive *a, int code)
{
	int r;

	switch (code) {
	case FILTER_LZMA_OR_LZIP:
		if ((r = archive_read_support_filter_lzma(a)) < ARCHIVE_WARN)
			return (r);
		return (archive_read_support_filter_lzip(a));
	default:
		return (archive_read_append_filter(a, code));
	}
}

/*
 * Names the only entry of a compressed file after the filename without its
 * extensions, for the data that don't give a name. Only the last component is
 * used, so that the name can't lead out of the directory that is extracted to.
 */
static int
set_raw_pathname(struct archive *a, const char *filename, size_t stem_length)
{
	char *pathname;
	int r;
	size_t i = stem_length;

	while (i > 0 && !is_separator(filename[i - 1]))
		i--;

	pathname = malloc(stem_length - i + 1);
	if (pathname == NULL) {
		archive_set_error(a, ENOMEM, "Can't allocate the pathname");
		return (ARCHIVE_FATAL);
	}
	memcpy(pathname, filename + i, stem_length - i);
	pathname[stem_length - i] = '\0';
	r = archive_read_set_format_option(a, "raw", "pathname", pathname);
	free(pathname);
	return (r);
}

la_ssize_t
archive_read_set_format_from_filename(struct archive *_a,
    const char *filename, la_ssize_t filename_length)
{
	struct plan plan;
	int i, r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
	    "archive_read_set_format_from_filename");

	/*
	 * A missing name is not an error: applications pass the same name to
	 * archive_read_open_filename(), where NULL stands for standard input.
	 */
	if (filename == NULL)
		filename_length = 0;
	else if (filename_length < 0)
		filename_length = (la_ssize_t)strlen(filename);

	/*
	 * Nothing is registered unless the filename is recognized. A name
	 * with a NUL byte has no reliable extension, and the explicit length
	 * is the only way to pass one.
	 */
	if ((filename_length > 0 &&
	    memchr(filename, '\0', (size_t)filename_length) != NULL) ||
	    !make_plan(filename, (size_t)filename_length, &plan)) {
		archive_set_error(_a, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Unrecognized filename extension");
		return (ARCHIVE_FAILED);
	}

	for (i = 0; i < plan.filter_count; i++) {
		r = append_filter(_a, plan.filters[i]);
		if (r < ARCHIVE_WARN)
			return (r);
	}

	r = support_format(_a, plan.format);
	if (r < ARCHIVE_WARN)
		return (r);

	if (plan.format == FORMAT_RAW) {
		r = set_raw_pathname(_a, filename, plan.stem_length);
		if (r < ARCHIVE_WARN)
			return (r);
	}

	return ((la_ssize_t)plan.stem_length);
}
