/*-
 * Copyright (c) 2010 Michihiro NAKAJIMA
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
__FBSDID("$FreeBSD");

/*
Execute the following command to rebuild the data for this program:
   tail -n +32 test_read_format_cab.c | /bin/sh

#/bin/sh
#
# How to make test data.
#
# Temporary directory.
base=/tmp/cab
# Owner id
owner=1001
# Group id
group=1001
#
# Make contents of a cabinet file.
#
rm -rf ${base}
mkdir ${base}
mkdir ${base}/dir1
mkdir ${base}/dir2
#
touch ${base}/empty
cat > ${base}/dir1/file1 << END
                          file 1 contents
hello
hello
hello
END
#
cat > ${base}/dir2/file2 << END
                          file 2 contents
hello
hello
hello
hello
hello
hello
END
#
dd if=/dev/zero of=${base}/zero bs=1 count=33000
#
# Set up a file time.
#
TZ=utc touch -afhm -t 198001020000.00 ${base}/dir1/file1 ${base}/dir2/file2
TZ=utc touch -afhm -t 198001020000.02 ${base}/dir1 ${base}/dir2
TZ=utc touch -afhm -t 198001020000.04 ${base}/empty ${base}/zero
#
#
cat > ${base}/mkcab1 << END
.Set Compress=OFF
.Set DiskDirectory1=.
.Set CabinetName1=test_read_format_cab_1.cab
empty
.Set DestinationDir=dir1
dir1/file1
.Set DestinationDir=dir2
dir2/file2
END
#
cat > ${base}/mkcab2 << END
.Set CompressionType=MSZIP
.Set DiskDirectory1=.
.Set CabinetName1=test_read_format_cab_2.cab
empty
zero
.Set DestinationDir=dir1
dir1/file1
.Set DestinationDir=dir2
dir2/file2
END
#
f=cab.tar.Z
(cd ${base}; bsdtar cfZ $f empty zero dir1/file1 dir2/file2 mkcab1 mkcab2)
#
cab1=test_read_format_cab_1.cab 
cab2=test_read_format_cab_2.cab 
echo "You need some more work to make sample files for cab reader test."
echo "1. Move ${base}/${f} to Windows PC"
echo "2. Extract ${base}/${f}"
echo "3. Open command prompt and change current directory where you extracted ${base}/${f}"

echo "4. Execute makecab.exe /F mkcab1"
echo "5. Execute makecab.exe /F mkcab2"
echo "6. Then you will see what there are two cabinet files, ${cab1} and ${cab2}"
echo "7. Move the cabinet files to posix platform"
echo "8. Execute uuencode ${cab1} ${cab1} > ${cab1}.uu"
echo "9. Execute uuencode ${cab2} ${cab2} > ${cab2}.uu"
exit 1
*/

static const char file1[] = {
"                          file 1 contents\n"
"hello\n"
"hello\n"
"hello\n"
};
#define file1_size (sizeof(file1)-1)
static const char file2[] = {
"                          file 2 contents\n"
"hello\n"
"hello\n"
"hello\n"
"hello\n"
"hello\n"
"hello\n"
};
#define file2_size (sizeof(file2)-1)

static void
verify(const char *refname, int comp)
{
	struct archive_entry *ae;
	struct archive *a;
	char buff[128];
	char zero[128];
	size_t s;

	memset(zero, 0, sizeof(zero));
	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_compression_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Verify regular empty. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0777), archive_entry_mode(ae));
	assertEqualString("empty", archive_entry_pathname(ae));
	assertEqualInt(1292586132, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_uid(ae));
	assertEqualInt(0, archive_entry_gid(ae));
	assertEqualInt(0, archive_entry_size(ae));

	if (comp) {
		/* Verify regular zero.
		 * Maximum CFDATA size is 32768, so we need over 32768 bytes
		 * file to check if we properly handle multiple CFDATA.
		 */
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualInt((AE_IFREG | 0777), archive_entry_mode(ae));
		assertEqualString("zero", archive_entry_pathname(ae));
		assertEqualInt(1292586132, archive_entry_mtime(ae));
		assertEqualInt(0, archive_entry_uid(ae));
		assertEqualInt(0, archive_entry_gid(ae));
		assertEqualInt(33000, archive_entry_size(ae));
		for (s = 0; s + sizeof(buff) < 33000; s+= sizeof(buff)) {
			assertEqualInt(sizeof(buff),
			    archive_read_data(a, buff, sizeof(buff)));
			assertEqualMem(buff, zero, sizeof(buff));
		}
		assertEqualInt(33000 - s, archive_read_data(a, buff, 33000 - s));
		assertEqualMem(buff, zero, 33000 - s);
	}

	/* Verify regular file1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0777), archive_entry_mode(ae));
	assertEqualString("dir1/file1", archive_entry_pathname(ae));
	assertEqualInt(1292586132, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_uid(ae));
	assertEqualInt(0, archive_entry_gid(ae));
	assertEqualInt(file1_size, archive_entry_size(ae));
	assertEqualInt(file1_size, archive_read_data(a, buff, file1_size));
	assertEqualMem(buff, file1, file1_size);

	/* Verify regular file2. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0777), archive_entry_mode(ae));
	assertEqualString("dir2/file2", archive_entry_pathname(ae));
	assertEqualInt(1292586132, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_uid(ae));
	assertEqualInt(0, archive_entry_gid(ae));
	assertEqualInt(file2_size, archive_entry_size(ae));
	assertEqualInt(file2_size, archive_read_data(a, buff, file2_size));
	assertEqualMem(buff, file2, file2_size);

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_COMPRESSION_NONE, archive_compression(a));
	assertEqualIntA(a, ARCHIVE_FORMAT_CAB, archive_format(a));

	/* Close the archive. */
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_cab)
{
	/* Verify Cabinet file in no compression. */
	verify("test_read_format_cab_1.cab", 0);
	/* Verify Cabinet file in MSZIP. */
	verify("test_read_format_cab_2.cab", 1);
}

