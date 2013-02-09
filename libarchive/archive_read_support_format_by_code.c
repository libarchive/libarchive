/*-
 * Copyright (c) 2003-2011 Tim Kientzle
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

#include "archive.h"
#include "archive_private.h"

int
archive_read_support_format_by_code(struct archive *a, int format_code)
{
	archive_check_magic(a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_format_by_code");

	switch (format_code & ARCHIVE_FORMAT_BASE_MASK) {
#ifdef BUILD_7ZIP_FORMAT
	case ARCHIVE_FORMAT_7ZIP:
		return archive_read_support_format_7zip(a);
		break;
#endif
#ifdef BUILD_AR_FORMAT
	case ARCHIVE_FORMAT_AR:
		return archive_read_support_format_ar(a);
		break;
#endif
#ifdef BUILD_CAB_FORMAT
	case ARCHIVE_FORMAT_CAB:
		return archive_read_support_format_cab(a);
		break;
#endif
#ifdef BUILD_CPIO_FORMAT
	case ARCHIVE_FORMAT_CPIO:
		return archive_read_support_format_cpio(a);
		break;
#endif
#ifdef BUILD_ISO_FORMAT
	case ARCHIVE_FORMAT_ISO9660:
		return archive_read_support_format_iso9660(a);
		break;
#endif
#ifdef BUILD_LHA_FORMAT
	case ARCHIVE_FORMAT_LHA:
		return archive_read_support_format_lha(a);
		break;
#endif
#ifdef BUILD_MTREE_FORMAT
	case ARCHIVE_FORMAT_MTREE:
		return archive_read_support_format_mtree(a);
		break;
#endif
#ifdef BUILD_RAR_FORMAT
	case ARCHIVE_FORMAT_RAR:
		return archive_read_support_format_rar(a);
		break;
#endif
	case ARCHIVE_FORMAT_TAR:
		return archive_read_support_format_tar(a);
		break;
#ifdef BUILD_XAR_FORMAT
	case ARCHIVE_FORMAT_XAR:
		return archive_read_support_format_xar(a);
		break;
#endif
#ifdef BUILD_ZIP_FORMAT
	case ARCHIVE_FORMAT_ZIP:
		return archive_read_support_format_zip(a);
		break;
#endif
	}
	return (ARCHIVE_FATAL);
}
