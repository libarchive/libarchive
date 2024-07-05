#!/bin/sh
if [ "$1" = "prepare" ]
then
	set -x
	brew uninstall openssl@1.1 > /dev/null
	brew uninstall python@3.7 > /dev/null
	brew untap local/openssl > /dev/null
	brew update > /dev/null
	brew upgrade > /dev/null
	set -x -e
	for pkg in \
		autoconf \
		automake \
		libtool \
		pkg-config \
		cmake \
		xz \
		lz4 \
		zstd \
		libxml2 \
		openssl
	do
		brew list $pkg > /dev/null && brew upgrade $pkg || brew install $pkg
	done
fi
