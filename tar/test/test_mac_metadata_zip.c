/*-
 * Copyright (c) 2017 Martin Matuska
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

/*
Notes on reffile creation:

- I chose an existing small text file on my machine that had extended attributes.
An easy way to generate one is to download a file from github in Google Chrome,
as mac will automatically set a "com.apple.metadata:kMDItemWhereFroms" xattr on the file.
It also would have worked to set an xattr manually using xattr -w [-rsx] attr_name attr_value file ...

- to verify the names/values of xattrs, run: xattr -l filename

- I then zipped the file using right-click > Compress in Finder
(Apple's officially recommended method for zip file creation)

- I then uuencoded the zip file by running: uuencode version.txt.zip test_mac_metadata_zip.zip

Zip created 12-31-2023 on a 2022 Mac Studio running macOS Ventura 13.6.3 (22G436) (Apple M1 Ultra - arm64)

Contents:
 Length   Method    Size  Cmpr    Date    Time   CRC-32   Name
--------  ------  ------- ---- ---------- ----- --------  ----
      15  Defl:N       17 -13% 09-21-2023 01:05 f542663d  version.txt
    1038  Defl:N      668  36% 09-21-2023 01:05 85bdc448  __MACOSX/._version.txt
--------          -------  ---                            -------
    1053              685  35%                            2 files

For additional notes, see: https://github.com/libarchive/libarchive/issues/2041#issuecomment-1873018929
*/

#include "test.h"

DEFINE_TEST(test_mac_metadata_zip)
{
#if !defined(__APPLE__) || !defined(UF_COMPRESSED) || !defined(HAVE_SYS_XATTR_H)\
	|| !defined(HAVE_ZLIB_H)
	skipping("MacOS-specific Mac Metadata test");
#else
	const char *reffile = "test_mac_metadata_zip.zip";

	const char *testattr = "com.apple.metadata:kMDItemWhereFroms";
	void *readval_orig;
	void *readval;
	size_t size;
	int r;

	extract_reference_file(reffile);

	/* Extract an archive to disk with mac metadata. */
	assertMakeDir("mac_metadata", 0755);
	r = systemf("%s -x -C mac_metadata --mac-metadata -f %s >mac_metadata.out 2>mac_metadata.err", testprog, reffile);
	assertEqualInt(r, 0);
	assertFileSize("mac_metadata/version.txt", 15);
	readval_orig = getXattr("mac_metadata/version.txt", testattr, &size);
	assertEqualInt(size, 658);

	/* Re-Archive with --mac-metadata --format zip */
	assertMakeDir("mac_metadata/subdir", 0755);
	r = systemf("%s --mac-metadata --format zip -cvf mac_metadata/subdir/archive.zip -C mac_metadata version.txt", testprog);
	assertEqualInt(r, 0);
	assertFileSize("mac_metadata/subdir/archive.zip", 1088);

	/* Re-Extract with --mac-metadata, assert that xattr was preserved */
	r = systemf("%s -x -C mac_metadata/subdir --mac-metadata -f mac_metadata/subdir/archive.zip >mac_metadata.out 2>mac_metadata.err", testprog);
	assertEqualInt(r, 0);
	assertFileSize("mac_metadata/subdir/version.txt", 15);
	readval = getXattr("mac_metadata/subdir/version.txt", testattr, &size);
	assertEqualInt(size, 658);

	/* Assert that it is identical to original */
	assertEqualMem(readval_orig, readval, size);
	assertEqualFile("mac_metadata/version.txt", "mac_metadata/subdir/version.txt");
#endif
}
