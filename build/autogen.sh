#!/bin/sh


# Start from one level above the build directory
if [ -f version ]; then
    cd ..
fi

if [ \! -f build/version ]; then
    echo "Can't find source directory"
    exit 1
fi

set -xe
aclocal
autoheader
autoconf
case `uname` in
Darwin) glibtoolize --automake -c;;
*) libtoolize --automake -c;;
esac
automake -a -c
