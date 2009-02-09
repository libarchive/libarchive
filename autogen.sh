#!/bin/sh

set -xe
mkdir -p config.aux
aclocal
autoheader
autoconf
case `uname` in
Darwin) glibtoolize --automake -c;;
*) libtoolize --automake -c;;
esac
automake -a -c
