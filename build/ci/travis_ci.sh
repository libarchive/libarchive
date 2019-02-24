#!/bin/sh
set -e
UNAME=`uname`
CURDIR=`pwd`
SRCDIR="${SRCDIR:-`pwd`}"
if [ -z "${BUILDDIR}" ]; then
        BUILDDIR="${CURDIR}/build_ci/${BS}"
fi
mkdir -p "${BUILDDIR}"
cd "${BUILDDIR}"
case "$UNAME" in
	MSYS*)
	if [ "${BS}" = "msbuild" ]; then
		set -x
		export PATH=${PATH}:"/c/Program Files (x86)/Microsoft Visual Studio/2017/BuildTools/MSBuild/15.0/Bin"
		cmake -G "Visual Studio 15 2017" -D CMAKE_BUILD_TYPE="Release" "${SRCDIR}"
		MSBuild.exe libarchive.sln //t:ALL_BUILD
		MSBuild.exe libarchive.sln //t:RUN_ALL_TESTS
		set +x
	elif [ "${BS}" = "mingw" ]; then
		set -x
		cmake -G "MSYS Makefiles" -D CMAKE_C_COMPILER="${CC}" -D CMAKE_MAKE_PROGRAM="mingw32-make" -D CMAKE_BUILD_TYPE="Release" "${SRCDIR}"
		mingw32-make
		mingw32-make test
		set +x
	else
		echo "Unknown or unspecified build type: ${BS}"
		exit 1
	fi
	;;
esac
