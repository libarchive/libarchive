/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2015 Okhotnikov Kirill
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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_set_format_by_name.c 201168 2009-12-29 06:15:32Z kientzle $");

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_private.h"

/* A table that maps names to functions. */
static
struct { const char *name; int (*format)(struct archive *); int (*filter)(struct archive *);  } names[] =
{
	{ ".7z",	archive_write_set_format_7zip,    archive_write_add_filter_none},
	{ ".zip",	archive_write_set_format_zip,     archive_write_add_filter_none},
	{ ".jar",	archive_write_set_format_zip,     archive_write_add_filter_none},
	{ ".cpio",	archive_write_set_format_cpio,    archive_write_add_filter_none},
	{ ".iso",	archive_write_set_format_iso9660, archive_write_add_filter_none},
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || defined(__OpenBSD__)
	{ ".a",	        archive_write_set_format_ar_bsd,  archive_write_add_filter_none},
	{ ".ar",	archive_write_set_format_ar_bsd,  archive_write_add_filter_none},
	{ ".tar",	archive_write_set_format_pax,     archive_write_add_filter_none},
	{ ".tgz",	archive_write_set_format_pax,     archive_write_add_filter_gzip},
	{ ".tar.gz",	archive_write_set_format_pax,     archive_write_add_filter_gzip},
	{ ".tar.bz2",	archive_write_set_format_pax,     archive_write_add_filter_bzip2},
	{ ".tar.xz",	archive_write_set_format_pax,     archive_write_add_filter_xz},
#else         
	{ ".a",	        archive_write_set_format_ar_svr4, archive_write_add_filter_none},
	{ ".ar",	archive_write_set_format_ar_svr4, archive_write_add_filter_none},
	{ ".tar",	archive_write_set_format_gnutar,  archive_write_add_filter_none},
	{ ".tgz",	archive_write_set_format_gnutar,  archive_write_add_filter_gzip},
	{ ".tar.gz",	archive_write_set_format_gnutar,  archive_write_add_filter_gzip},
	{ ".tar.bz2",	archive_write_set_format_gnutar,  archive_write_add_filter_bzip2},
	{ ".tar.xz",	archive_write_set_format_gnutar,  archive_write_add_filter_xz},
#endif        
	{ NULL,		NULL,                             NULL }
};

static 
int cmpsuff(const char *str, const char *suffix)
{
  size_t length_str     = strlen(str);
  size_t length_suffix  = strlen(suffix);
  
  return strcmp((str + length_str - length_suffix), suffix);
}


int
archive_write_set_format_filter_by_ext(struct archive *a, const char *filename)
{
	int i;

	for (i = 0; names[i].name != NULL; i++) 
        {
	  if (cmpsuff(filename, names[i].name) == 0)
          { 
	    int format_state = (names[i].format)(a);
            if( format_state == ARCHIVE_OK )
              return ((names[i].filter)(a));
            else
              return format_state;
          }       
	}

	archive_set_error(a, EINVAL, "No such format '%s'", filename);
	a->state = ARCHIVE_STATE_FATAL;
	return (ARCHIVE_FATAL);
}
