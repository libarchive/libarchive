/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
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
 *
 * $FreeBSD$
 */

#if defined(__CYGWIN__)
#define _WIN32_WINNT	0x500
#define WINVER		0x500

#include "cpio_platform.h"
#include <errno.h>
#include <stddef.h>
#include <sys/utime.h>
#include <sys/stat.h>
#include <process.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <sddl.h>

#include "cpio.h"

#ifndef LIST_H
static int
_is_privileged(HANDLE thandle, const char *sidlist[])
{
	TOKEN_USER *tuser;
	TOKEN_GROUPS  *tgrp;
	DWORD bytes;
	PSID psid;
	DWORD i, g;
	int member;

	psid = NULL;
	tuser = NULL;
	tgrp = NULL;
	member = 0;
	for (i = 0; sidlist[i] != NULL && member == 0; i++) {
		if (psid != NULL)
			LocalFree(psid);
		/* mingw/cygwin: incorrectly prototypes arg 1 as LPSTR
		 * instead of LPCSTR. Work around it here
		 */
		if (ConvertStringSidToSidA((char *)sidlist[i], &psid) == 0) {
			errno = EPERM;
			return (-1);
		}
		if (tuser == NULL) {
			GetTokenInformation(thandle, TokenUser, NULL, 0, &bytes);
			tuser = malloc(bytes);
			if (tuser == NULL) {
				errno = ENOMEM;
				member = -1;
				break;
			}
			if (GetTokenInformation(thandle, TokenUser, tuser, bytes, &bytes) == 0) {
				errno = EPERM;
				member = -1;
				break;
			}
		}
		member = EqualSid(tuser->User.Sid, psid);
		if (member)
			break;
		if (tgrp == NULL) {
			GetTokenInformation(thandle, TokenGroups, NULL, 0, &bytes);
			tgrp = malloc(bytes);
			if (tgrp == NULL) {
				errno = ENOMEM;
				member = -1;
				break;
			}
			if (GetTokenInformation(thandle, TokenGroups, tgrp, bytes, &bytes) == 0) {
				errno = EPERM;
				member = -1;
				break;
			}
		}
		for (g = 0; g < tgrp->GroupCount; g++) {
			member = EqualSid(tgrp->Groups[g].Sid, psid);
			if (member)
				break;
		}
	}
	LocalFree(psid);
	free(tuser);
	free(tgrp);

	return (member);
}

int
bsdcpio_is_privileged()
{
	HANDLE thandle;
	int ret;
	const char *sidlist[] = {
		"S-1-5-32-544",	/* Administrators */
		"S-1-5-32-551", /* Backup Operators */
		NULL
	};

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &thandle) == 0) {
		cpio_warnc(EPERM, "Failed to check privilege");
		return (0);
	}
	ret = _is_privileged(thandle, sidlist);
	if (ret < 0) {
		cpio_warnc(errno, "Failed to check privilege");
		return (0);
	}
	return (ret);
}

#endif /* LIST_H */

#endif
