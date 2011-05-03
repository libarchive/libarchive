/*-
 * Copyright (c) 2011 Michihiro NAKAJIMA
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
__FBSDID("$FreeBSD$");

#include <locale.h>

#define __LIBARCHIVE_TEST
#include "archive_string.h"

/*
Execute the following to rebuild the data for this program:
   tail -n +36 test_archive_string_conversion.c | /bin/sh
#
# This requires http://unicode.org/Public/UNIDATA/NormalizationTest.txt
#
if="NormalizationTest.txt"
if [ ! -f ${if} ]; then
  echo "Not found: \"${if}\""
  exit 0
fi
of=test_archive_string_conversion.txt.gz
echo "\$FreeBSD\$" > ${of}.uu
awk -F ';'  '$0 ~/^[0-9A-F]+/ {printf "%s;%s\n", $2, $3}' ${if} | gzip | uuencode ${of} >> ${of}.uu
exit 1
*/

static int
unicode_to_utf8(char *p, uint32_t uc)
{        
        char *_p = p;

        /* Translate code point to UTF8 */
        if (uc <= 0x7f) {
                *p++ = (char)uc;
        } else if (uc <= 0x7ff) {
                *p++ = 0xc0 | ((uc >> 6) & 0x1f);
                *p++ = 0x80 | (uc & 0x3f);
        } else if (uc <= 0xffff) {
                *p++ = 0xe0 | ((uc >> 12) & 0x0f);
                *p++ = 0x80 | ((uc >> 6) & 0x3f);
                *p++ = 0x80 | (uc & 0x3f);
        } else {
                *p++ = 0xf0 | ((uc >> 18) & 0x07);
                *p++ = 0x80 | ((uc >> 12) & 0x3f);
                *p++ = 0x80 | ((uc >> 6) & 0x3f);
                *p++ = 0x80 | (uc & 0x3f);
        }
        return ((int)(p - _p));
}

static int
scan_unicode_pattern(char *out, const char *pattern, int exclude_mac_nfd)
{
	unsigned uc = 0;
	const char *p = pattern;
	char *op = out;

	for (;;) {
		if (*p >= '0' && *p <= '9')
			uc = (uc << 4) + (*p - '0');
		else if (*p >= 'A' && *p <= 'F')
			uc = (uc << 4) + (*p - 'A' + 0x0a);
		else {
			if (exclude_mac_nfd) {
				/* These are not converted to NFD on Mac OS. */
				if ((uc >= 0x2000 && uc <= 0x2FFF) ||
				    (uc >= 0xF900 && uc <= 0xFAFF) ||
				    (uc >= 0x2F800 && uc <= 0x2FAFF))
					return (-1);
			}
			op += unicode_to_utf8(op, uc);
			if (!*p) {
				*op = '\0';
				break;
			}
			uc = 0;
		}
		p++;
	}
	return (0);
}

/*
 * A conversion test that we correctly normalize UTF-8 characters.
 * On Mac OS, the characters to be Form D.
 * On other platforms, the characters to be Form C.
 */
static void
test_archive_string_normalization()
{
	struct archive *a;
	struct archive_entry *ae;
	struct archive_string s;
	struct archive_string_conv *sconv;
	FILE *fp;
	char buff[512];
	static const char reffile[] = "test_archive_string_conversion.txt.gz";
	ssize_t size;
	int line = 0;

	/* If it doesn't exist, just warn and return. */
	if (NULL == setlocale(LC_ALL, "en_US.UTF-8")) {
		skipping("invalid encoding tests require a suitable locale;"
		    " en_US.UTF-8 not available on this system");
		return;
	}

	archive_string_init(&s);
	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
        assertEqualIntA(a, ARCHIVE_OK,
            archive_read_open_filename(a, reffile, 512));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assert((fp = fopen("testdata.txt", "w")) != NULL);
	while ((size = archive_read_data(a, buff, 512)) > 0)
		fwrite(buff, 1, size, fp);
	fclose(fp);

	assert((fp = fopen("testdata.txt", "r")) != NULL);
	assert(NULL != (sconv =
	    archive_string_conversion_from_charset(a, "UTF-8", 0)));
	/*
	 * Read test data.
	 *  Test data format:
	 *     <NFC Unicode pattern> ';' <NFD Unicode pattern> '\n'
	 *  Unicode pattern format:
	 *     [0-9A-F]{4,5}([ ][0-9A-F]{4,5}){0,}
	 */
	while (fgets(buff, sizeof(buff), fp) != NULL) {
		char nfc[80], nfd[80];
		char utf8_nfc[80], utf8_nfd[80];
		char *e, *p;

		line++;
		if (buff[0] == '#')
			continue;
		p = strchr(buff, ';');
		if (p == NULL)
			continue;
		*p++ = '\0';
		/* Copy an NFC pattern */
		strncpy(nfc, buff, sizeof(nfc)-1);
		nfc[sizeof(nfc)-1] = '\0';
		e = p;
		p = strchr(p, '\n');
		if (p == NULL)
			continue;
		*p = '\0';
		/* Copy an NFD pattern */
		strncpy(nfd, e, sizeof(nfd)-1);
		nfd[sizeof(nfd)-1] = '\0';

		/*
		 * Convert an NFC pattern to UTF-8 bytes.
		 */
#if defined(__APPLE__)
		if (scan_unicode_pattern(utf8_nfc, nfc, 1) != 0)
			continue;
#else
		scan_unicode_pattern(utf8_nfc, nfc, 0);
#endif

		/*
		 * Convert an NFD pattern to UTF-8 bytes.
		 */
		scan_unicode_pattern(utf8_nfd, nfd, 0);

#if defined(__APPLE__)
		/*
		 * Normalize an NFC string.
		 */
		assertEqualInt(0,
		    archive_strcpy_in_locale(&s, utf8_nfc, sconv));
		failure("NFC(%s) should be converted to NFD(%s):%d",
		    nfc, nfd, line);
		assertEqualString(utf8_nfd, s.s);

		/*
		 * Normalize an NFD string.
		 */
		assertEqualInt(0,
		    archive_strcpy_in_locale(&s, utf8_nfd, sconv));
		failure("NFD(%s) should not be any changed:%d",
		    nfd, line);
		assertEqualString(utf8_nfd, s.s);
#else
		/*
		 * Normalize an NFD string.
		 */
		assertEqualInt(0,
		    archive_strcpy_in_locale(&s, utf8_nfd, sconv));
		failure("NFD(%s) should be converted to NFC(%s):%d",
		    nfd, nfc, line);
		assertEqualString(utf8_nfc, s.s);

		/*
		 * Normalize an NFC string.
		 */
		assertEqualInt(0,
		    archive_strcpy_in_locale(&s, utf8_nfc, sconv));
		failure("NFC(%s) should not be any changed:%d",
		    nfc, line);
		assertEqualString(utf8_nfc, s.s);
#endif
	}

	archive_string_free(&s);
	fclose(fp);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}


DEFINE_TEST(test_archive_string_conversion)
{
	test_archive_string_normalization();
}
