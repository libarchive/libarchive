/*-
 * Copyright (c) 2009 Andreas Henriksson
 * All rights reserved.
 *
 * Just reuse libarchive/test/test_read_format_isojoliet_bz2.c
 * and extend it by defining ONLY_IN_JOLIET_WITH_RR_TEST to include
 * more asserts which only works with RockRidge extension available.
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
Execute the following to rebuild the data for this program:
   tail -n +35 test_read_format_isojolietrr_bz2.c | /bin/sh

rm -rf /tmp/iso
mkdir /tmp/iso
mkdir /tmp/iso/dir
echo "hello" >/tmp/iso/long-joliet-file-name.textfile
ln /tmp/iso/long-joliet-file-name.textfile /tmp/iso/hardlink
(cd /tmp/iso; ln -s long-joliet-file-name.textfile symlink)
if [ "$(uname -s)" = "Linux" ]; then # gnu coreutils touch doesn't have -h
TZ=utc touch -afm -t 197001020000.01 /tmp/iso /tmp/iso/long-joliet-file-name.textfile /tmp/iso/dir
TZ=utc touch -afm -t 197001030000.02 /tmp/iso/symlink
else
TZ=utc touch -afhm -t 197001020000.01 /tmp/iso /tmp/iso/long-joliet-file-name.textfile /tmp/iso/dir
TZ=utc touch -afhm -t 197001030000.02 /tmp/iso/symlink
fi
mkhybrid -J -R -uid 1 -gid 2 /tmp/iso | bzip2 > test_read_format_isojolietrr_bz2.iso.bz2
F=test_read_format_isojolietrr_bz2.iso.bz2
uuencode $F $F > $F.uu
exit 1
 */

#if 0 /* iso9660 with *BOTH* joliet and RR not implemented yet. :( */

#define ONLY_IN_JOLIET_WITH_RR_TEST
#include "test_read_format_isojoliet_bz2.c"
#undef ONLY_IN_JOLIET_WITH_RR_TEST

#else /* dummy */

#include "test.h"

DEFINE_TEST(test_read_format_isojolietrr_bz2)
{
}

#endif

