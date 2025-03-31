/*-
 * Copyright © 2025 ARJANEN Loïc Jean David
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
#include "archive_private.h"
#include "archive_time_private.h"
#include <stdlib.h>

#define NTFS_EPOC_TIME ARCHIVE_LITERAL_ULL(11644473600)
#define NTFS_TICKS ARCHIVE_LITERAL_ULL(10000000)
#define NTFS_EPOC_TICKS (NTFS_EPOC_TIME * NTFS_TICKS)
#define DOS_MIN_TIME 0x00210000U
#define DOS_MAX_TIME 0xff9fbf7dU
/* The min/max DOS Unix time are locale-dependant, so they're static variables,
 * initialised on first use. */
static char dos_initialised = 0;
static int64_t dos_max_unix;
static int64_t dos_min_unix;

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <winnt.h>
/* Windows FILETIME to NTFS time. */
uint64_t
FILETIME_to_ntfs(const FILETIME* filetime)
{
	ULARGE_INTEGER utc;
	utc.HighPart = filetime->dwHighDateTime;
	utc.LowPart  = filetime->dwLowDateTime;
	return utc.QuadPart;
}
#endif

/* Convert an MSDOS-style date/time into Unix-style time. */
int64_t
dos_to_unix(uint32_t dos_time)
{
	uint16_t msTime, msDate;
	struct tm ts;

	msTime = (0xFFFF & dos_time);
	msDate = (dos_time >> 16);
	ts.tm_year = ((msDate >> 9) & 0x7f) + 80; /* Years since 1900. */
	ts.tm_mon = ((msDate >> 5) & 0x0f) - 1; /* Month number. */
	ts.tm_mday = msDate & 0x1f; /* Day of month. */
	ts.tm_hour = (msTime >> 11) & 0x1f;
	ts.tm_min = (msTime >> 5) & 0x3f;
	ts.tm_sec = (msTime << 1) & 0x3e;
	ts.tm_isdst = -1;
	return mktime(&ts);
}

/* Convert into MSDOS-style date/time. */
uint32_t
unix_to_dos(int64_t unix_time)
{
	struct tm *t;
	uint32_t dt;
	time_t ut = unix_time;
#if defined(HAVE_LOCALTIME_R) || defined(HAVE_LOCALTIME_S)
	struct tm tmbuf;
#endif

	if (!dos_initialised) {
		dos_max_unix = dos_to_unix(DOS_MAX_TIME);
		dos_min_unix = dos_to_unix(DOS_MIN_TIME);
		dos_initialised = 1;
	}
	if (unix_time >= dos_max_unix) {
		return DOS_MAX_TIME;
	}
	else if(unix_time <= dos_min_unix) {
		return DOS_MIN_TIME;
	}
	else {
#if defined(HAVE_LOCALTIME_S)
		t = localtime_s(&tmbuf, &ut) ? NULL : &tmbuf;
#elif defined(HAVE_LOCALTIME_R)
		t = localtime_r(&ut, &tmbuf);
#else
		t = localtime(&ut);
#endif
		dt = 0;
		dt += ((t->tm_year - 80) & 0x7f) << 9;
		dt += ((t->tm_mon + 1) & 0x0f) << 5;
		dt += (t->tm_mday & 0x1f);
		dt <<= 16;
		dt += (t->tm_hour & 0x1f) << 11;
		dt += (t->tm_min & 0x3f) << 5;
		dt += (t->tm_sec & 0x3e) >> 1; /* Only counting every 2 seconds. */
		return dt;
	}
}

/* Convert NTFS time to Unix sec/ncse */
void
ntfs_to_unix(uint64_t ntfs, int64_t* secs, uint32_t* nsecs)
{
	ntfs -= NTFS_EPOC_TICKS;
	lldiv_t tdiv = lldiv(ntfs, NTFS_TICKS);
	*secs = tdiv.quot;
	*nsecs = tdiv.rem * 100;
}

/* Convert Unix sec/nsec to NTFS time */
uint64_t
unix_to_ntfs(int64_t secs, uint32_t nsecs)
{
	uint64_t ntfs = secs + NTFS_EPOC_TIME;
	ntfs *= NTFS_TICKS;
	return ntfs + nsecs/100;
}

/* Check if time fits in 32-bits Unix time */
char
fits_in_unix(int64_t secs) {
	return secs >= INT32_MIN && secs <= INT32_MAX;
}

/* Check if time fits in DOS time */
char
fits_in_dos(int64_t secs) {
	if (!dos_initialised) {
		dos_max_unix = dos_to_unix(DOS_MAX_TIME);
		dos_min_unix = dos_to_unix(DOS_MIN_TIME);
		dos_initialised = 1;
	}
	return secs >= dos_min_unix && secs <= dos_max_unix;
}