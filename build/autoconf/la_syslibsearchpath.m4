# la_syslibsearchpath.m4 - attempt to determine the compiler's
#                          library search directories.
# sets $la_sys_lib_search_path_spec to space-separated list of
# directories, specifing the compiler's built-in search path
# for libraries. This is used by configure.ac when $host_os is
# cygwin, to locate a special *object* we need to link against:
# binmode.o.  We know this object is located in that search path.
# However, because gcc does not search for objects in its
# libsearchpath (there is no -l for .o's), we must extract the
# path(s) and search manually.
#
# Adapted from libtool.m4 (_LT_SYS_DYNAMIC_LINKER):
#
#   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2003, 2004, 2005,
#                 2006, 2007, 2008, 2009 Free Software Foundation, Inc.
#   Written by Gordon Matzigkeit, 1996
#
# This file is free software; the Free Software Foundation gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
AC_DEFUN([LA_SYS_LIB_SEARCH_PATH],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_PROG_SED])dnl
AC_REQUIRE([AC_PROG_AWK])dnl
AC_MSG_CHECKING([compiler library search path])
# don't bother with EBCDIC systems here:
la_NL2SP="tr \\015\\012 \\040\\040"
if test "$GCC" = yes; then
  case $host_os in
    darwin*) la_awk_arg="/^libraries:/,/LR/" ;;
    *) la_awk_arg="/^libraries:/" ;;
  esac
  la_search_path_spec=`$CC -print-search-dirs | $AWK $la_awk_arg | $SED -e "s/^libraries://" -e "s,=/,/,g"`
  case $la_search_path_spec in
  *\;*)
    # if the path contains ";" then we assume it to be the separator
    # otherwise default to the standard path separator (i.e. ":") - it is
    # assumed that no part of a normal pathname contains ";" but that should
    # okay in the real world where ";" in dirpaths is itself problematic.
    la_search_path_spec=`echo "$la_search_path_spec" | $SED 's/;/ /g'`
    ;;
  *)
    la_search_path_spec=`echo "$la_search_path_spec" | $SED "s/$PATH_SEPARATOR/ /g"`
    ;;
  esac
  # Ok, now we have the path, separated by spaces, we can step through it
  # and add multilib dir if necessary.
  la_tmp_la_search_path_spec=
  la_multi_os_dir=`$CC $CPPFLAGS $CFLAGS $LDFLAGS -print-multi-os-directory 2>/dev/null`
  for la_sys_path in $la_search_path_spec; do
    if test -d "$la_sys_path/$la_multi_os_dir"; then
      la_tmp_la_search_path_spec="$la_tmp_la_search_path_spec $la_sys_path/$la_multi_os_dir"
    else
      test -d "$la_sys_path" && \
        la_tmp_la_search_path_spec="$la_tmp_la_search_path_spec $la_sys_path"
    fi
  done
  la_search_path_spec=`echo "$la_tmp_la_search_path_spec" | $AWK '
BEGIN {RS=" "; FS="/|\n";} {
  la_foo="";
  la_count=0;
  for (la_i = NF; la_i > 0; la_i--) {
    if ($la_i != "" && $la_i != ".") {
      if ($la_i == "..") {
        la_count++;
      } else {
        if (la_count == 0) {
          la_foo="/" $la_i la_foo;
        } else {
          la_count--;
        }
      }
    }
  }
  if (la_foo != "") { la_freq[[la_foo]]++; }
  if (la_freq[[la_foo]] == 1) { print la_foo; }
}'`
  la_sys_lib_search_path_spec=`echo "$la_search_path_spec" | $la_NL2SP`
else
  la_sys_lib_search_path_spec="/lib /usr/lib /usr/local/lib"
fi
AC_MSG_RESULT($la_sys_lib_search_path_spec)])

