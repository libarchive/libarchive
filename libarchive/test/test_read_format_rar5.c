/*-
 * Copyright (c) 2018 Grzegorz Antoniak
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

/* Some tests will want to calculate some CRC32's, and this header can
 * help. */
#define __LIBARCHIVE_BUILD
#include <archive_crc32.h>

#define PROLOGUE(reffile) \
    struct archive_entry *ae; \
    struct archive *a; \
    \
    (void) a;  /* Make the compiler happy if we won't use this variables */ \
    (void) ae; /* in the test cases. */ \
    \
    extract_reference_file(reffile); \
    assert((a = archive_read_new()) != NULL); \
    assertA(0 == archive_read_support_filter_all(a)); \
    assertA(0 == archive_read_support_format_all(a)); \
    assertA(0 == archive_read_open_filename(a, reffile, 10240))

#define PROLOGUE_MULTI(reffile) \
    struct archive_entry *ae; \
    struct archive *a; \
    \
    (void) a; \
    (void) ae; \
    \
    extract_reference_files(reffile); \
    assert((a = archive_read_new()) != NULL); \
    assertA(0 == archive_read_support_filter_all(a)); \
    assertA(0 == archive_read_support_format_all(a)); \
    assertA(0 == archive_read_open_filenames(a, reffile, 10240))


#define EPILOGUE() \
    assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a)); \
    assertEqualInt(ARCHIVE_OK, archive_read_free(a))

static
int verify_data(const uint8_t* data_ptr, int magic, int size) {
    int i = 0;

    /* This is how the test data inside test files was generated;
     * we are re-generating it here and we check if our re-generated
     * test data is the same as in the test file. If this test is
     * failing it's either because there's a bug in the test case,
     * or the unpacked data is corrupted. */

    for(i = 0; i < size / 4; ++i) {
        const int k = i + 1;
        const signed int* lptr = (const signed int*) &data_ptr[i * 4];
        signed int val = k * k - 3 * k + (1 + magic);

        if(val < 0)
            val = 0;

        /* *lptr is a value inside unpacked test file, val is the
         * value that should be in the unpacked test file. */

        if(*lptr != val)
            return 0;
    }

    return 1;
}

DEFINE_TEST(test_read_format_rar5_stored) 
{
    const char helloworld_txt[] = "hello libarchive test suite!\n";
    la_ssize_t file_size = sizeof(helloworld_txt) - 1;
    char buff[64];

    PROLOGUE("test_read_format_rar5_stored.rar");

    assertA(0 == archive_read_next_header(a, &ae));
    assertEqualString("helloworld.txt", archive_entry_pathname(ae));
    assertA((int) archive_entry_mtime(ae) > 0);
    assertA((int) archive_entry_ctime(ae) == 0);
    assertA((int) archive_entry_atime(ae) == 0);
    assertEqualInt(file_size, archive_entry_size(ae));
    assertEqualInt(33188, archive_entry_mode(ae));
    assertA(file_size == archive_read_data(a, buff, file_size));
    assertEqualMem(buff, helloworld_txt, file_size);
    assertEqualInt(archive_entry_is_encrypted(ae), 0);

    assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));

    EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_compressed)
{
    const int DATA_SIZE = 1200;
    uint8_t buff[DATA_SIZE];

    PROLOGUE("test_read_format_rar5_compressed.rar");

    assertA(0 == archive_read_next_header(a, &ae));
    assertEqualString("test.bin", archive_entry_pathname(ae));
    assertA((int) archive_entry_mtime(ae) > 0);
    assertEqualInt(DATA_SIZE, archive_entry_size(ae));
    assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
    assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
    verify_data(buff, 0, DATA_SIZE);

    EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_multiple_files)
{
    const int DATA_SIZE = 4096;
    uint8_t buff[DATA_SIZE];

    PROLOGUE("test_read_format_rar5_multiple_files.rar");

    /* There should be 4 files inside this test file. Check for their
     * existence, and also check the contents of those test files. */

    assertA(0 == archive_read_next_header(a, &ae));
    assertEqualString("test1.bin", archive_entry_pathname(ae));
    assertEqualInt(DATA_SIZE, archive_entry_size(ae));
    assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
    assertA(verify_data(buff, 1, DATA_SIZE));
    
    assertA(0 == archive_read_next_header(a, &ae));
    assertEqualString("test2.bin", archive_entry_pathname(ae));
    assertEqualInt(DATA_SIZE, archive_entry_size(ae));
    assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
    assertA(verify_data(buff, 2, DATA_SIZE));

    assertA(0 == archive_read_next_header(a, &ae));
    assertEqualString("test3.bin", archive_entry_pathname(ae));
    assertEqualInt(DATA_SIZE, archive_entry_size(ae));
    assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
    assertA(verify_data(buff, 3, DATA_SIZE));

    assertA(0 == archive_read_next_header(a, &ae));
    assertEqualString("test4.bin", archive_entry_pathname(ae));
    assertEqualInt(DATA_SIZE, archive_entry_size(ae));
    assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
    assertA(verify_data(buff, 4, DATA_SIZE));

    /* There should be no more files in this archive. */

    assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
    EPILOGUE();
}

/* This test is really the same as the test above, but it deals with a solid
 * archive instead of a regular archive. The test solid archive contains the
 * same set of files as regular test archive, but it's size is 2x smaller,
 * because solid archives reuse the window buffer from previous compressed
 * files, so it's able to compress lots of small files more effectively. */

DEFINE_TEST(test_read_format_rar5_multiple_files_solid)
{
    const int DATA_SIZE = 4096;
    uint8_t buff[DATA_SIZE];

    PROLOGUE("test_read_format_rar5_multiple_files_solid.rar");

    assertA(0 == archive_read_next_header(a, &ae));
    assertEqualString("test1.bin", archive_entry_pathname(ae));
    assertEqualInt(DATA_SIZE, archive_entry_size(ae));
    assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
    assertA(verify_data(buff, 1, DATA_SIZE));
    
    assertA(0 == archive_read_next_header(a, &ae));
    assertEqualString("test2.bin", archive_entry_pathname(ae));
    assertEqualInt(DATA_SIZE, archive_entry_size(ae));
    assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
    assertA(verify_data(buff, 2, DATA_SIZE));

    assertA(0 == archive_read_next_header(a, &ae));
    assertEqualString("test3.bin", archive_entry_pathname(ae));
    assertEqualInt(DATA_SIZE, archive_entry_size(ae));
    assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
    assertA(verify_data(buff, 3, DATA_SIZE));

    assertA(0 == archive_read_next_header(a, &ae));
    assertEqualString("test4.bin", archive_entry_pathname(ae));
    assertEqualInt(DATA_SIZE, archive_entry_size(ae));
    assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
    assertA(verify_data(buff, 4, DATA_SIZE));

    assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
    EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_multiarchive_skip)
{
    const char* reffiles[] = {
        "test_read_format_rar5_multiarchive.part01.rar",
        "test_read_format_rar5_multiarchive.part02.rar",
        "test_read_format_rar5_multiarchive.part03.rar",
        "test_read_format_rar5_multiarchive.part04.rar",
        "test_read_format_rar5_multiarchive.part05.rar",
        "test_read_format_rar5_multiarchive.part06.rar",
        "test_read_format_rar5_multiarchive.part07.rar",
        "test_read_format_rar5_multiarchive.part08.rar",
        NULL
    };

    /* Just skip */
    {
        PROLOGUE_MULTI(reffiles);
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("home/antek/temp/build/unrar5/libarchive/bin/bsdcat_test", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("home/antek/temp/build/unrar5/libarchive/bin/bsdtar_test", archive_entry_pathname(ae));

        assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
        EPILOGUE();
    }

    /* Extract first and skip the second */
    {
        uint8_t* mem;
        la_ssize_t fsize;
        
        PROLOGUE_MULTI(reffiles);
        assertA(0 == archive_read_next_header(a, &ae));

        /* Read the whole file into memory. */
        fsize = archive_entry_size(ae);
        mem = malloc(fsize);
        assertA(mem != NULL);
        assertA(fsize == archive_read_data(a, mem, fsize));
        assertEqualInt(crc32(0, mem, fsize), 0x35277473);
        free(mem);

        /* Skip another file. */
        assertA(0 == archive_read_next_header(a, &ae));
        
        /* There's no third file, should be EOF. */
        assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
        EPILOGUE();
    }

    /* Skip first and extract the second */
    {
        uint8_t* mem;
        la_ssize_t fsize;

        PROLOGUE_MULTI(reffiles);
        assertA(0 == archive_read_next_header(a, &ae));

        /* Skip the first file, extract the second file. */
        assertA(0 == archive_read_next_header(a, &ae));
        fsize = archive_entry_size(ae);
        mem = malloc(fsize);
        assertA(mem != NULL);
        assertA(fsize == archive_read_data(a, mem, fsize));
        assertEqualInt(crc32(0, mem, fsize), 0xE59665F8);
        free(mem);

        /* Third file doesn't exist, and should be EOF. */
        assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
        EPILOGUE();
    }
}

DEFINE_TEST(test_read_format_rar5_blake2)
{
    const la_ssize_t proper_size = 814;
    uint8_t buf[proper_size];

    PROLOGUE("test_read_format_rar5_blake2.rar");
    assertA(0 == archive_read_next_header(a, &ae));
    assertEqualInt(proper_size, archive_entry_size(ae));

    /* Should blake2 calculation fail, we'll get a failure return
     * value from archive_read_data(). */

    assertA(proper_size == archive_read_data(a, buf, proper_size));

    /* To be extra pedantic, let's also check crc32 of the poem. */
    assertEqualInt(crc32(0, buf, proper_size), 0x7E5EC49E);

    assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
    EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_arm_filter)
{
    /* This test unpacks a file that uses an ARM filter. The DELTA
     * and X86 filters are tested implicitly in the "multiarchive_skip"
     * test. */

    const la_ssize_t proper_size = 90808;
    uint8_t buf[proper_size];

    PROLOGUE("test_read_format_rar5_arm.rar");
    assertA(0 == archive_read_next_header(a, &ae));
    assertEqualInt(proper_size, archive_entry_size(ae));
    assertA(proper_size == archive_read_data(a, buf, proper_size));

    /* Yes, RARv5 unpacker itself should calculate the CRC, but in case
     * the DONT_FAIL_ON_CRC_ERROR define option is enabled during compilation,
     * let's still fail the test if the unpacked data is wrong. */
    assertEqualInt(crc32(0, buf, proper_size), 0x886F91EB);

    assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
    EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_skip_stored)
{
    const char* fname = "test_read_format_rar5_stored_manyfiles.rar";

    /* Skip all 3 files first. */
    {
        PROLOGUE(fname);
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("make_uue.tcl", archive_entry_pathname(ae));
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("cebula.txt", archive_entry_pathname(ae));
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test.bin", archive_entry_pathname(ae));
        assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
        EPILOGUE();
    }

    /* Skip first, extract in part rest. */
    {
        char buf[6];
        PROLOGUE(fname);
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("make_uue.tcl", archive_entry_pathname(ae));
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("cebula.txt", archive_entry_pathname(ae));
        assertA(6 == archive_read_data(a, buf, 6));
        assertEqualInt(0, memcmp(buf, "Cebula", 6));
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test.bin", archive_entry_pathname(ae));
        assertA(4 == archive_read_data(a, buf, 4));
        assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
        EPILOGUE();
    }

    /* Extract in part first, skip rest. */
    {
        char buf[405];
        PROLOGUE(fname);
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("make_uue.tcl", archive_entry_pathname(ae));
        assertA(405 == archive_read_data(a, buf, sizeof(buf)));
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("cebula.txt", archive_entry_pathname(ae));
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test.bin", archive_entry_pathname(ae));
        assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
        EPILOGUE();
    }

    /* Extract in part all */
    {
        char buf[4];
        PROLOGUE(fname);
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("make_uue.tcl", archive_entry_pathname(ae));
        assertA(4 == archive_read_data(a, buf, 4));
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("cebula.txt", archive_entry_pathname(ae));
        assertA(4 == archive_read_data(a, buf, 4));
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test.bin", archive_entry_pathname(ae));
        assertA(4 == archive_read_data(a, buf, 4));
        assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
        EPILOGUE();
    }

}

DEFINE_TEST(test_read_format_rar5_multiarchive_solid_skip)
{
    const char* reffiles[] = {
        "test_read_format_rar5_multiarchive_solid.part01.rar",
        "test_read_format_rar5_multiarchive_solid.part02.rar",
        "test_read_format_rar5_multiarchive_solid.part03.rar",
        "test_read_format_rar5_multiarchive_solid.part04.rar",
        NULL
    };

    /* Just skip */
    {
        PROLOGUE_MULTI(reffiles);
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("cebula.txt", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test1.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test2.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test3.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test4.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test5.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test6.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("elf-Linux-ARMv7-ls", archive_entry_pathname(ae));

        assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
        EPILOGUE();
    }

    /* Extract first, skip rest */
    {
        uint8_t buf[814];

        PROLOGUE_MULTI(reffiles);
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("cebula.txt", archive_entry_pathname(ae));
        assertEqualInt(814, archive_read_data(a, buf, 814));
        assertEqualInt(crc32(0, buf, 814), 0x7E5EC49E);

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test1.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test2.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test3.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test4.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test5.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test6.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("elf-Linux-ARMv7-ls", archive_entry_pathname(ae));

        assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
        EPILOGUE();
    }

    /* Skip first, extract second, skip rest. */
    {
        uint8_t buf[1200];

        PROLOGUE_MULTI(reffiles);
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("cebula.txt", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test.bin", archive_entry_pathname(ae));
        assertEqualInt(1200, archive_entry_size(ae));
        assertEqualInt(1200, archive_read_data(a, buf, 1200));
        assertEqualInt(crc32(0, buf, 1200), 0x7CCA70CD);

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test1.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test2.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test3.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test4.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test5.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test6.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("elf-Linux-ARMv7-ls", archive_entry_pathname(ae));

        assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
        EPILOGUE();
    }

    /* Skip first two, extract third, skip rest. */
    {
        uint8_t buf[4096];

        PROLOGUE_MULTI(reffiles);
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("cebula.txt", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test1.bin", archive_entry_pathname(ae));
        assertEqualInt(4096, archive_entry_size(ae));
        assertEqualInt(4096, archive_read_data(a, buf, 4096));
        assertEqualInt(crc32(0, buf, 4096), 0x7E13B2C6);

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test2.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test3.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test4.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test5.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test6.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("elf-Linux-ARMv7-ls", archive_entry_pathname(ae));

        assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
        EPILOGUE();
    }

    /* Skip all but last, extract last. */
    {
        la_ssize_t fsize = 90808;
        uint8_t* buf = malloc(fsize);

        PROLOGUE_MULTI(reffiles);
        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("cebula.txt", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test1.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test2.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test3.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test4.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test5.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("test6.bin", archive_entry_pathname(ae));

        assertA(0 == archive_read_next_header(a, &ae));
        assertEqualString("elf-Linux-ARMv7-ls", archive_entry_pathname(ae));
        assertEqualInt(fsize, archive_entry_size(ae));
        assertEqualInt(fsize, archive_read_data(a, buf, fsize));
        assertEqualInt(crc32(0, buf, fsize), 0x886F91EB);

        assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
        EPILOGUE();
        free(buf);
    }
}

// TODO: skip in solid stream
