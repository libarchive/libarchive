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

#include "archive_platform.h"
#include "archive_time_private.h"
#include "archive_private.h"
#include "archive_entry.h"

#if defined(_WIN32) && !defined(__CYGWIN__)

void
archive_entry_copy_bhfi(struct archive_entry *entry,
			BY_HANDLE_FILE_INFORMATION *bhfi)
{
	int64_t secs;
	uint32_t nsecs;
	uint64_t file_index, file_size;

	ntfs_to_unix(FILETIME_to_ntfs(&bhfi->ftLastAccessTime), &secs, &nsecs);
	archive_entry_set_atime(entry, secs, nsecs);
	ntfs_to_unix(FILETIME_to_ntfs(&bhfi->ftLastWriteTime), &secs, &nsecs);
	archive_entry_set_mtime(entry, secs, nsecs);
	ntfs_to_unix(FILETIME_to_ntfs(&bhfi->ftCreationTime), &secs, &nsecs);
	archive_entry_set_birthtime(entry, secs, nsecs);
	archive_entry_set_ctime(entry, secs, nsecs);
	archive_entry_set_dev(entry, bhfi->dwVolumeSerialNumber);
	file_index = bhfi_ino(bhfi);
#if ARCHIVE_VERSION_NUMBER < 4000000
	if (file_index > (uint64_t)INT64_MAX)
		archive_entry_set_ino64(entry, -1);
	else
#endif
		archive_entry_set_ino64(entry, (__LA_INO_T)file_index);
	archive_entry_set_nlink(entry, bhfi->nNumberOfLinks);
	file_size = ((uint64_t)bhfi->nFileSizeHigh << 32) |
	    bhfi->nFileSizeLow;
	archive_entry_set_size(entry, file_size > INT64_MAX ? INT64_MAX :
	    (int64_t)file_size);
	/* archive_entry_set_mode(entry, st->st_mode); */
}
#endif
