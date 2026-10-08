/*-
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "test.h"

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

static int
run_tar(const wchar_t *program, const wchar_t *arguments)
{
	PROCESS_INFORMATION pi;
	STARTUPINFOW si;
	DWORD exit_code;
	wchar_t *command;
	size_t length;

	length = wcslen(program) + wcslen(arguments) + 4;
	command = malloc(length * sizeof(*command));
	assert(command != NULL);
	wcscpy(command, L"\"");
	wcscat(command, program);
	wcscat(command, L"\" ");
	wcscat(command, arguments);

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	memset(&pi, 0, sizeof(pi));
	assert(CreateProcessW(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL,
	    &si, &pi));
	assert(WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_OBJECT_0);
	assert(GetExitCodeProcess(pi.hProcess, &exit_code));
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	free(command);
	return ((int)exit_code);
}

static void
write_file(const wchar_t *path, const void *data, DWORD size)
{
	HANDLE file;
	DWORD written;

	file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	    FILE_ATTRIBUTE_NORMAL, NULL);
	assert(file != INVALID_HANDLE_VALUE);
	assert(WriteFile(file, data, size, &written, NULL));
	assertEqualInt(size, written);
	CloseHandle(file);
}

static void
assert_file_contents(const wchar_t *path, const void *data, DWORD size)
{
	HANDLE file;
	DWORD read;
	char *buffer;

	file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
	    FILE_ATTRIBUTE_NORMAL, NULL);
	assert(file != INVALID_HANDLE_VALUE);
	buffer = malloc(size);
	assert(buffer != NULL);
	assert(ReadFile(file, buffer, size, &read, NULL));
	assertEqualInt(size, read);
	assertEqualMem(data, buffer, size);
	free(buffer);
	CloseHandle(file);
}
#endif

DEFINE_TEST(test_windows_unicode_cli)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	wchar_t program[MAX_PATH];
	wchar_t cwd[MAX_PATH];
	wchar_t source[MAX_PATH];
	wchar_t archive[MAX_PATH];
	wchar_t extract[MAX_PATH];
	wchar_t extracted[MAX_PATH];
	wchar_t arguments[MAX_PATH * 3];
	const char payload[] = "unicode command-line path\n";
	DWORD length;

	length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, testprogfile,
	    -1, program, (int)ARRAY_SIZE(program));
	assert(length != 0);
	length = GetCurrentDirectoryW((DWORD)ARRAY_SIZE(cwd), cwd);
	assert(length != 0 && length < ARRAY_SIZE(cwd));
	assert(swprintf(source, ARRAY_SIZE(source), L"%ls\\source-\U0001f600.txt", cwd) > 0);
	assert(swprintf(archive, ARRAY_SIZE(archive), L"%ls\\archive-\U0001f600.tar", cwd) > 0);
	assert(swprintf(extract, ARRAY_SIZE(extract), L"%ls\\extract-\U0001f600", cwd) > 0);
	assert(swprintf(extracted, ARRAY_SIZE(extracted), L"%ls\\source-\U0001f600.txt", extract) > 0);

	write_file(source, payload, sizeof(payload) - 1);
	assert(swprintf(arguments, ARRAY_SIZE(arguments),
	    L"-cf \"archive-\U0001f600.tar\" \"source-\U0001f600.txt\"") > 0);
	assertEqualInt(0, run_tar(program, arguments));
	assert(GetFileAttributesW(archive) != INVALID_FILE_ATTRIBUTES);

	assert(CreateDirectoryW(extract, NULL));
	assert(swprintf(arguments, ARRAY_SIZE(arguments),
	    L"-xf \"archive-\U0001f600.tar\" -C \"extract-\U0001f600\"") > 0);
	assertEqualInt(0, run_tar(program, arguments));
	assert(GetFileAttributesW(extracted) != INVALID_FILE_ATTRIBUTES);
	assert_file_contents(extracted, payload, sizeof(payload) - 1);
#else
	skipping("Windows-specific command line encoding test");
#endif
}
