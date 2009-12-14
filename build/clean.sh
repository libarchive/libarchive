#!/bin/sh

if [ \! -f build/version ]; then
    echo 'Must run the clean script from the top-level dir of the libarchive distribution' 1>&2
    exit 1
fi

#
# The automake-generated 'maintainer-clean' target does clean up a
# lot.  If that fails, try plain 'clean' in case we're using the cmake
# or other makefile.  But don't worry if we can't...
#
make maintainer-clean || make clean || true

# If we're on BSD, blow away the build dir under /usr/obj
rm -rf /usr/obj`pwd`

#
# Try to clean up a bit more...
#
find . -name '*~' | xargs rm
find . -name '*.rej' | xargs rm
find . -name '*.orig' | xargs rm
find . -name '*.o' | xargs rm
find . -name '*.po' | xargs rm
find . -name '*.lo' | xargs rm
find . -name '*.So' | xargs rm
find . -name '*.a' | xargs rm
find . -name '*.la' | xargs rm
find . -name '.depend' | xargs rm
find . -name '.dirstamp' | xargs rm
find . -name '.deps' | xargs rm -rf
find . -name '.libs' | xargs rm -rf
rm -rf autom4te.cache
rm -f bsdcpio bsdcpio_test bsdtar bsdtar_test libarchive_test
rm -f libtool aclocal.m4 config.h config.h.in
rm -f doc/man/* doc/pdf/* doc/wiki/* doc/html/* doc/text/*
rm -f config.log Makefile.in configure config.status stamp-h1
rm -f libarchive/*.[35].gz libarchive/libarchive.so*
rm -f libarchive/test/libarchive_test libarchive/test/list.h
rm -f tar/*.1.gz tar/bsdtar tar/test/bsdtar_test tar/test/list.h
rm -f cpio/*.1.gz cpio/bsdcpio cpio/test/bsdcpio_test cpio/test/list.h
rm -f build/autoconf/compile build/autoconf/config.* build/autoconf/ltmain.sh
rm -f build/autoconf/install-sh build/autoconf/depcomp build/autoconf/missing
rm -f build/pkgconfig/libarchive.pc
