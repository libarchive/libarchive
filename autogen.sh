#!/bin/sh

set -xe
mkdir -p config.aux
aclocal
autoheader
autoconf
libtoolize --automake -c
automake -a -c
