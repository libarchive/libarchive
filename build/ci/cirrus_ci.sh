#!/bin/sh
UNAME=`uname`
if [ "$1" = "install" ]
then
	if [ "$UNAME" = "FreeBSD" ]
	then
		set -x -e
		sed -i.bak -e 's,pkg+http://pkg.FreeBSD.org/\${ABI}/quarterly,pkg+http://pkg.FreeBSD.org/\${ABI}/latest,' /etc/pkg/FreeBSD.conf
		mount -u -o acls /
		mkdir /tmp_acl_nfsv4
		MD=`mdconfig -a -t swap -s 128M`
		newfs /dev/$MD
		tunefs -N enable /dev/$MD
		mount /dev/$MD /tmp_acl_nfsv4
		chmod 1777 /tmp_acl_nfsv4
		pkg install -y autoconf automake libiconv libtool pkgconf expat libxml2 liblz4 zstd
	elif [ "$UNAME" = "Darwin" ]
	then
		set -x -e
		brew update
		brew install xz lz4 zstd
	fi
elif [ "$1" = "test" ]
then
	if [ "$UNAME" = "FreeBSD" ]
	then
		set -e
		echo "Additional NFSv4 ACL tests"
		CURDIR=`pwd`
		BUILDDIR="${CURDIR}/build_ci/autotools"
		cd "${BUILDDIR}"
		TMPDIR=/tmp_acl_nfsv4 ./libarchive_test -r "${CURDIR}/libarchive/test" -v test_acl_platform_nfs4
	fi
else
	echo "Usage $0 install | test_nfsv4_acls"
	exit 1
fi
