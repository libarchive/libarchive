/*-
 * Copyright (c) 2026 Dustin L. Howett
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

/*
 * This test relies on an archive with the ZIP end of central directory
 * that is just beyond the 16kb range we use to detect a seekable zip,
 * which would also erroneously trigger if seek was broken.
 * If seek is not working properly, this archive will either not be parsed
 * or be parsed skipping the first few files (depending on which bidders
 * run before zip and how much data they consume.)
 */
DEFINE_TEST(test_extract_pipe_windows)
{
#if !defined(_WIN32) || defined(__CYGWIN__)
	skipping("Windows specific test");
#else
	const char *refname = "test_extract_pipe_windows.zip";
	char *p;
	size_t s;
	HANDLE hRead, hWrite;
	STARTUPINFOA si = {0};
	PROCESS_INFORMATION pi = {0};
	SECURITY_ATTRIBUTES sa = {0};
	char *cmdline;
	size_t cmdlineSize;
	DWORD w = 0;
	DWORD exitCode = 0;

	extract_reference_file(refname);
	p = slurpfile(&s, "%s", refname);

	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	/* Non-seekable source bound to stdin */
	assert(TRUE == CreatePipe(&hRead, &hWrite, &sa, s));
	/* Make sure bsdtar does not inherit the write half so it can be closed to signal EOF */
	if(!assert(TRUE == SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT, 0))) {
		CloseHandle(hRead);
		CloseHandle(hWrite);
		free(p);
		return;
	}

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	si.hStdInput = hRead;
	cmdlineSize = strlen(testprog) + 16;
	cmdline = malloc(cmdlineSize);
	if(!assert(cmdline != NULL))
	{
		CloseHandle(hRead);
		CloseHandle(hWrite);
		free(p);
		return;
	}

	snprintf(cmdline, cmdlineSize, "%s -xf -", testprog);
	if(!assert(TRUE == CreateProcessA(testprogfile, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))) {
		/* Failed to create the bsdtar process */
		free(cmdline);
		CloseHandle(hRead);
		CloseHandle(hWrite);
		free(p);
	}

	/* Passed to the child; no longer necessary */
	CloseHandle(hRead);
	free(cmdline);

	assert(TRUE == WriteFile(hWrite, p, (DWORD)s, &w, NULL));
	assertEqualInt((size_t)w, s);
	assert(TRUE == FlushFileBuffers(hWrite));

	CloseHandle(pi.hThread);
	CloseHandle(hWrite);
	free(p);

	WaitForSingleObject(pi.hProcess, INFINITE);
	assert(TRUE == GetExitCodeProcess(pi.hProcess, &exitCode));
	assertEqualInt(0, exitCode);

	CloseHandle(pi.hProcess);
#endif
}
