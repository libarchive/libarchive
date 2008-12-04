## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS= foreign subdir-objects

#
# What to build and install
#
lib_LTLIBRARIES=	libarchive.la
bin_PROGRAMS=	$(bsdtar_programs) $(bsdcpio_programs)
man_MANS= $(libarchive_man_MANS) $(bsdtar_man_MANS) $(bsdcpio_man_MANS)
BUILT_SOURCES= libarchive/test/list.h tar/test/list.h cpio/test/list.h

#
# What to test: We always test libarchive, test bsdtar and bsdcpio only
# if we built them.
#
check_PROGRAMS= libarchive_test $(bsdtar_test_programs) $(bsdcpio_test_programs)
TESTS= libarchive_test $(bsdtar_test_programs) $(bsdcpio_test_programs)
TESTS_ENVIRONMENT= $(libarchive_TESTS_ENVIRONMENT) $(bsdtar_TESTS_ENVIRONMENT) $(bsdcpio_TESTS_ENVIRONMENT)
# Always build and test both bsdtar and bsdcpio as part of 'distcheck'
DISTCHECK_CONFIGURE_FLAGS = --enable-bsdtar --enable-bsdcpio

#
# What to include in the distribution
#
EXTRA_DIST= version			\
	contrib				\
	doc				\
	examples			\
	windows				\
	$(libarchive_EXTRA_DIST)	\
	$(libarchive_test_EXTRA_DIST)	\
	$(bsdtar_EXTRA_DIST)		\
	$(bsdtar_test_EXTRA_DIST)	\
	$(bsdcpio_EXTRA_DIST)		\
	$(bsdcpio_test_EXTRA_DIST)

# a) Clean out some unneeded files and directories
# b) Collect all documentation and format it for distribution.
dist-hook:
	rm -rf `find $(distdir) -name CVS -type d`
	rm -rf `find $(distdir) -name .svn -type d`
	rm -f `find $(distdir) -name '*~'`
	rm -f `find $(distdir) -name '*.out'`
	rm -f `find $(distdir) -name '*.core'`
	-rm -f $(distdir)/*/Makefile $(distdir)/*/*/Makefile
	cd $(distdir)/doc && /bin/sh update.sh

#
# Extra rules for cleanup
#
DISTCLEANFILES= tar/getdate.c tar/getdate.h	\
	libarchive/test/list.h			\
	tar/test/list.h				\
	cpio/test/list.h

distclean-local:
	-rm -rf .ref
	-rm -rf autom4te.cache/
	-rm -f *~
	-[ -f libarchive/Makefile ] && cd libarchive && make clean
	-[ -f libarchive/test/Makefile ] && cd libarchive/test && make clean
	-[ -f tar/Makefile ] && cd tar && make clean
	-[ -f tar/test/Makefile ] && cd tar/test && make clean
	-[ -f cpio/Makefile ] && cd cpio && make clean
	-[ -f cpio/test/Makefile ] && cd cpio/test && make clean

#
# Libarchive headers, source, etc.
#
#

include_HEADERS= libarchive/archive.h libarchive/archive_entry.h

libarchive_la_SOURCES=						\
	libarchive/archive_check_magic.c			\
	libarchive/archive_endian.h				\
	libarchive/archive_entry.c				\
	libarchive/archive_entry.h				\
	libarchive/archive_entry_copy_stat.c			\
	libarchive/archive_entry_link_resolver.c		\
	libarchive/archive_entry_private.h			\
	libarchive/archive_entry_stat.c				\
	libarchive/archive_entry_strmode.c			\
	libarchive/archive_platform.h				\
	libarchive/archive_private.h				\
	libarchive/archive_read.c				\
	libarchive/archive_read_data_into_fd.c			\
	libarchive/archive_read_extract.c			\
	libarchive/archive_read_open_fd.c			\
	libarchive/archive_read_open_file.c			\
	libarchive/archive_read_open_filename.c			\
	libarchive/archive_read_open_memory.c			\
	libarchive/archive_read_private.h			\
	libarchive/archive_read_support_compression_all.c	\
	libarchive/archive_read_support_compression_bzip2.c	\
	libarchive/archive_read_support_compression_compress.c	\
	libarchive/archive_read_support_compression_gzip.c	\
	libarchive/archive_read_support_compression_none.c	\
	libarchive/archive_read_support_compression_program.c	\
	libarchive/archive_read_support_compression_lzma.c	\
	libarchive/archive_read_support_format_all.c		\
	libarchive/archive_read_support_format_ar.c		\
	libarchive/archive_read_support_format_cpio.c		\
	libarchive/archive_read_support_format_empty.c		\
	libarchive/archive_read_support_format_iso9660.c	\
	libarchive/archive_read_support_format_mtree.c		\
	libarchive/archive_read_support_format_tar.c		\
	libarchive/archive_read_support_format_zip.c		\
	libarchive/archive_string.c				\
	libarchive/archive_string.h				\
	libarchive/archive_string_sprintf.c			\
	libarchive/archive_util.c				\
	libarchive/archive_virtual.c				\
	libarchive/archive_write.c				\
	libarchive/archive_write_disk.c				\
	libarchive/archive_write_disk_private.h			\
	libarchive/archive_write_disk_set_standard_lookup.c	\
	libarchive/archive_write_open_fd.c			\
	libarchive/archive_write_open_file.c			\
	libarchive/archive_write_open_filename.c		\
	libarchive/archive_write_open_memory.c			\
	libarchive/archive_write_private.h			\
	libarchive/archive_write_set_compression_bzip2.c	\
	libarchive/archive_write_set_compression_compress.c	\
	libarchive/archive_write_set_compression_gzip.c		\
	libarchive/archive_write_set_compression_none.c		\
	libarchive/archive_write_set_compression_program.c	\
	libarchive/archive_write_set_format.c			\
	libarchive/archive_write_set_format_ar.c		\
	libarchive/archive_write_set_format_by_name.c		\
	libarchive/archive_write_set_format_cpio.c		\
	libarchive/archive_write_set_format_cpio_newc.c		\
	libarchive/archive_write_set_format_mtree.c		\
	libarchive/archive_write_set_format_pax.c		\
	libarchive/archive_write_set_format_shar.c		\
	libarchive/archive_write_set_format_ustar.c		\
	libarchive/config_freebsd.h				\
	libarchive/config_windows.h				\
	libarchive/filter_fork.c				\
	libarchive/filter_fork.h

libarchive_la_CPPFLAGS=-I $(top_builddir)/libarchive
# -no-undefined marks that libarchive doesn't rely on symbols
# defined in the application.  This is mandatory for cygwin.
libarchive_la_LDFLAGS= -no-undefined -version-info $(ARCHIVE_LIBTOOL_VERSION)

# Manpages to install
libarchive_man_MANS=						\
	libarchive/archive_entry.3				\
	libarchive/archive_read.3				\
	libarchive/archive_util.3				\
	libarchive/archive_write.3				\
	libarchive/archive_write_disk.3				\
	libarchive/cpio.5					\
	libarchive/libarchive.3					\
	libarchive/libarchive_internals.3			\
	libarchive/libarchive-formats.5				\
	libarchive/mtree.5					\
	libarchive/tar.5

# Additional libarchive files to include in the distribution
libarchive_EXTRA_DIST=			\
	libarchive/test/list.h		\
	libarchive/archive_windows.c	\
	libarchive/archive_windows.h	\
	$(libarchive_man_MANS)

#
#
# libarchive_test program
#
#
libarchive_test_SOURCES=					\
	$(libarchive_la_SOURCES)				\
	libarchive/test/main.c					\
	libarchive/test/read_open_memory.c			\
	libarchive/test/test.h					\
	libarchive/test/test_acl_basic.c			\
	libarchive/test/test_acl_freebsd.c			\
	libarchive/test/test_acl_pax.c				\
	libarchive/test/test_archive_api_feature.c		\
	libarchive/test/test_bad_fd.c				\
	libarchive/test/test_compat_bzip2.c			\
	libarchive/test/test_compat_gtar.c			\
	libarchive/test/test_compat_gzip.c			\
	libarchive/test/test_compat_tar_hardlink.c		\
	libarchive/test/test_compat_zip.c			\
	libarchive/test/test_empty_write.c			\
	libarchive/test/test_entry.c				\
	libarchive/test/test_fuzz.c				\
	libarchive/test/test_entry_strmode.c			\
	libarchive/test/test_link_resolver.c			\
	libarchive/test/test_pax_filename_encoding.c		\
	libarchive/test/test_read_compress_program.c		\
	libarchive/test/test_read_data_large.c			\
	libarchive/test/test_read_extract.c			\
	libarchive/test/test_read_format_ar.c			\
	libarchive/test/test_read_format_cpio_bin.c		\
	libarchive/test/test_read_format_cpio_bin_Z.c		\
	libarchive/test/test_read_format_cpio_bin_bz2.c		\
	libarchive/test/test_read_format_cpio_bin_gz.c		\
	libarchive/test/test_read_format_cpio_odc.c		\
	libarchive/test/test_read_format_cpio_svr4_gzip.c	\
	libarchive/test/test_read_format_cpio_svr4c_Z.c		\
	libarchive/test/test_read_format_empty.c		\
	libarchive/test/test_read_format_gtar_gz.c		\
	libarchive/test/test_read_format_gtar_lzma.c		\
	libarchive/test/test_read_format_gtar_sparse.c		\
	libarchive/test/test_read_format_iso_gz.c		\
	libarchive/test/test_read_format_isorr_bz2.c		\
	libarchive/test/test_read_format_mtree.c		\
	libarchive/test/test_read_format_pax_bz2.c		\
	libarchive/test/test_read_format_tar.c			\
	libarchive/test/test_read_format_tar_empty_filename.c	\
	libarchive/test/test_read_format_tbz.c			\
	libarchive/test/test_read_format_tgz.c			\
	libarchive/test/test_read_format_tz.c			\
	libarchive/test/test_read_format_zip.c			\
	libarchive/test/test_read_large.c			\
	libarchive/test/test_read_pax_truncated.c		\
	libarchive/test/test_read_position.c			\
	libarchive/test/test_read_truncated.c			\
	libarchive/test/test_tar_filenames.c			\
	libarchive/test/test_tar_large.c			\
	libarchive/test/test_ustar_filenames.c			\
	libarchive/test/test_write_compress.c			\
	libarchive/test/test_write_compress_program.c		\
	libarchive/test/test_write_disk.c			\
	libarchive/test/test_write_disk_hardlink.c		\
	libarchive/test/test_write_disk_perms.c			\
	libarchive/test/test_write_disk_secure.c		\
	libarchive/test/test_write_disk_times.c			\
	libarchive/test/test_write_format_ar.c			\
	libarchive/test/test_write_format_cpio.c		\
	libarchive/test/test_write_format_cpio_empty.c		\
	libarchive/test/test_write_format_cpio_odc.c		\
	libarchive/test/test_write_format_cpio_newc.c		\
	libarchive/test/test_write_format_pax.c			\
	libarchive/test/test_write_format_shar_empty.c		\
	libarchive/test/test_write_format_tar.c			\
	libarchive/test/test_write_format_tar_empty.c		\
	libarchive/test/test_write_format_tar_ustar.c		\
	libarchive/test/test_write_open_memory.c

libarchive_test_CPPFLAGS= -I$(top_builddir)/libarchive -I$(top_srcdir)/libarchive -I$(top_builddir)/libarchive/test

# The "list.h" file just lists all of the tests defined in all of the sources.
# Building it automatically provides a sanity-check on libarchive_test_SOURCES
# above.
libarchive/test/list.h: Makefile
	cat $(top_srcdir)/libarchive/test/test_*.c | grep DEFINE_TEST > libarchive/test/list.h

libarchive_TESTS_ENVIRONMENT= LIBARCHIVE_TEST_FILES=`cd $(top_srcdir);/bin/pwd`/libarchive/test

libarchive_test_EXTRA_DIST=\
	libarchive/test/test_compat_bzip2_1.tbz.uu			\
	libarchive/test/test_compat_bzip2_2.tbz.uu			\
	libarchive/test/test_compat_gtar_1.tgz.uu			\
	libarchive/test/test_compat_gzip_1.tgz.uu			\
	libarchive/test/test_compat_gzip_2.tgz.uu			\
	libarchive/test/test_compat_tar_hardlink_1.tar.uu		\
	libarchive/test/test_compat_zip_1.zip.uu			\
	libarchive/test/test_fuzz_1.iso.uu				\
	libarchive/test/test_pax_filename_encoding.tar.gz.uu		\
	libarchive/test/test_read_format_gtar_sparse_1_13.tgz.uu	\
	libarchive/test/test_read_format_gtar_sparse_1_17.tgz.uu	\
	libarchive/test/test_read_format_gtar_sparse_1_17_posix00.tgz.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17_posix01.tgz.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17_posix10.tgz.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17_posix10_modified.tar.uu \
	libarchive/test/test_read_format_isorr_bz2.iso.bz2.uu		\
	libarchive/test/test_read_format_tar_empty_filename.tar.uu	\
	libarchive/test/test_read_format_zip.zip.uu


#
#
# bsdtar source, docs, etc.
#
#

bsdtar_SOURCES=				\
		tar/bsdtar.c		\
		tar/bsdtar.h		\
		tar/bsdtar_platform.h	\
		tar/cmdline.c		\
		tar/getdate.y		\
		tar/matching.c		\
		tar/read.c		\
		tar/siginfo.c		\
		tar/subst.c		\
		tar/tree.c		\
		tar/tree.h		\
		tar/util.c		\
		tar/write.c

bsdtar_DEPENDENCIES= libarchive.la

if STATIC_BSDTAR
bsdtar_static= -static
else
bsdtar_static=
endif

bsdtar_LDADD= libarchive.la
bsdtar_CPPFLAGS= -I$(top_builddir)/libarchive -I$(top_srcdir)/libarchive
bsdtar_LDFLAGS= $(bsdtar_static)
bsdtar_EXTRA_DIST=		\
	tar/bsdtar.1

if BUILD_BSDTAR
bsdtar_man_MANS= tar/bsdtar.1
bsdtar_programs= bsdtar
else
bsdtar_man_MANS=
bsdtar_programs=
endif

#
# bsdtar_test
#

bsdtar_test_SOURCES=						\
	tar/getdate.c						\
	tar/test/main.c						\
	tar/test/test.h						\
	tar/test/test_0.c					\
	tar/test/test_basic.c					\
	tar/test/test_copy.c					\
	tar/test/test_getdate.c					\
	tar/test/test_help.c					\
	tar/test/test_option_q.c				\
	tar/test/test_option_T.c				\
	tar/test/test_patterns.c				\
	tar/test/test_stdio.c					\
	tar/test/test_strip_components.c			\
	tar/test/test_symlink_dir.c				\
	tar/test/test_version.c

bsdtar_test_CPPFLAGS= -I$(top_builddir)/tar/test

tar/test/list.h: Makefile
	cat $(top_srcdir)/tar/test/test_*.c | grep DEFINE_TEST > tar/test/list.h

if BUILD_BSDTAR
bsdtar_test_programs= bsdtar_test
bsdtar_TESTS_ENVIRONMENT= BSDTAR=`cd $(top_builddir);/bin/pwd`/bsdtar BSDTAR_TEST_FILES=`cd $(top_srcdir);/bin/pwd`/tar/test
else
bsdtar_test_programs=
bsdtar_TESTS_ENVIRONMENT=
endif

bsdtar_test_EXTRA_DIST=			\
	tar/test/test_patterns_2.tgz.uu	\
	tar/test/test_patterns_3.tgz.uu


#
#
# bsdcpio source, docs, etc.
#
#

bsdcpio_SOURCES=			\
		cpio/cmdline.c		\
		cpio/cpio.c		\
		cpio/cpio.h		\
		cpio/cpio_platform.h	\
		cpio/err.c		\
		cpio/matching.c		\
		cpio/matching.h		\
		cpio/pathmatch.c	\
		cpio/pathmatch.h

bsdcpio_DEPENDENCIES = libarchive.la


if STATIC_BSDCPIO
bsdcpio_static= -static
else
bsdcpio_static=
endif

bsdcpio_LDADD= libarchive.la
bsdcpio_CPPFLAGS= -I$(top_builddir)/libarchive -I$(top_srcdir)/libarchive
bsdcpio_LDFLAGS= $(bsdcpio_static)
bsdcpio_EXTRA_DIST=		\
	cpio/test/list.h	\
	cpio/bsdcpio.1

if BUILD_BSDCPIO
# Manpages to install
bsdcpio_man_MANS= cpio/bsdcpio.1
bsdcpio_programs= bsdcpio
else
bsdcpio_man_MANS=
bsdcpio_programs=
endif

#
# bsdcpio_test
#

bsdcpio_test_SOURCES=						\
	cpio/cmdline.c						\
	cpio/err.c						\
	cpio/pathmatch.c					\
	cpio/test/main.c					\
	cpio/test/test.h					\
	cpio/test/test_0.c					\
	cpio/test/test_basic.c					\
	cpio/test/test_format_newc.c				\
	cpio/test/test_gcpio_compat.c				\
	cpio/test/test_option_B.c				\
	cpio/test/test_option_L.c				\
	cpio/test/test_option_a.c				\
	cpio/test/test_option_c.c				\
	cpio/test/test_option_d.c				\
	cpio/test/test_option_ell.c				\
	cpio/test/test_option_f.c				\
	cpio/test/test_option_help.c				\
	cpio/test/test_option_m.c				\
	cpio/test/test_option_t.c				\
	cpio/test/test_option_u.c				\
	cpio/test/test_option_version.c				\
	cpio/test/test_option_y.c				\
	cpio/test/test_option_z.c				\
	cpio/test/test_owner_parse.c				\
	cpio/test/test_passthrough_dotdot.c			\
	cpio/test/test_passthrough_reverse.c			\
	cpio/test/test_pathmatch.c

bsdcpio_test_CPPFLAGS= -I$(top_builddir)/libarchive -I$(top_srcdir)/libarchive -I$(top_builddir)/cpio/test

cpio/test/list.h: Makefile
	cat $(top_srcdir)/cpio/test/test_*.c | grep DEFINE_TEST > cpio/test/list.h

if BUILD_BSDCPIO
bsdcpio_test_programs= bsdcpio_test
bsdcpio_TESTS_ENVIRONMENT= BSDCPIO=`cd $(top_builddir);/bin/pwd`/bsdcpio BSDCPIO_TEST_FILES=`cd $(top_srcdir);/bin/pwd`/cpio/test
else
bsdcpio_test_programs=
bsdcpio_TESTS_ENVIRONMENT=
endif

bsdcpio_test_EXTRA_DIST=			\
	cpio/test/test_gcpio_compat_ref.bin.uu	\
	cpio/test/test_gcpio_compat_ref.crc.uu	\
	cpio/test/test_gcpio_compat_ref.newc.uu	\
	cpio/test/test_gcpio_compat_ref.ustar.uu \
	cpio/test/test_option_f.cpio.uu		\
	cpio/test/test_option_m.cpio.uu		\
	cpio/test/test_option_t.cpio.uu		\
	cpio/test/test_option_t.stdout.uu	\
	cpio/test/test_option_tv.stdout.uu
