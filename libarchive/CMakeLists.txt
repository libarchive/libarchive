
############################################
#
# How to build libarchive
#
############################################

if (ANDROID)
  include_directories(${PROJECT_SOURCE_DIR}/contrib/android/include)
endif()

# Public headers
SET(include_HEADERS
  archive.h
  archive_entry.h
)

# Sources and private headers
SET(libarchive_SOURCES
  archive_acl.c
  archive_acl_private.h
  archive_check_magic.c
  archive_cmdline.c
  archive_cmdline_private.h
  archive_crc32.h
  archive_cryptor.c
  archive_cryptor_private.h
  archive_digest.c
  archive_digest_private.h
  archive_endian.h
  archive_entry.c
  archive_entry.h
  archive_entry_copy_stat.c
  archive_entry_link_resolver.c
  archive_entry_locale.h
  archive_entry_private.h
  archive_entry_sparse.c
  archive_entry_stat.c
  archive_entry_strmode.c
  archive_entry_xattr.c
  archive_hmac.c
  archive_hmac_private.h
  archive_match.c
  archive_openssl_evp_private.h
  archive_openssl_hmac_private.h
  archive_options.c
  archive_options_private.h
  archive_pack_dev.h
  archive_pack_dev.c
  archive_parse_date.c
  archive_pathmatch.c
  archive_pathmatch.h
  archive_platform.h
  archive_platform_acl.h
  archive_platform_xattr.h
  archive_ppmd_private.h
  archive_ppmd8.c
  archive_ppmd8_private.h
  archive_ppmd7.c
  archive_ppmd7_private.h
  archive_private.h
  archive_random.c
  archive_random_private.h
  archive_rb.c
  archive_rb.h
  archive_read.c
  archive_read_add_passphrase.c
  archive_read_append_filter.c
  archive_read_data_into_fd.c
  archive_read_disk_entry_from_file.c
  archive_read_disk_posix.c
  archive_read_disk_private.h
  archive_read_disk_set_standard_lookup.c
  archive_read_extract.c
  archive_read_extract2.c
  archive_read_open_fd.c
  archive_read_open_file.c
  archive_read_open_filename.c
  archive_read_open_memory.c
  archive_read_private.h
  archive_read_set_format.c
  archive_read_set_options.c
  archive_read_support_filter_all.c
  archive_read_support_filter_by_code.c
  archive_read_support_filter_bzip2.c
  archive_read_support_filter_compress.c
  archive_read_support_filter_gzip.c
  archive_read_support_filter_grzip.c
  archive_read_support_filter_lrzip.c
  archive_read_support_filter_lz4.c
  archive_read_support_filter_lzop.c
  archive_read_support_filter_none.c
  archive_read_support_filter_program.c
  archive_read_support_filter_rpm.c
  archive_read_support_filter_uu.c
  archive_read_support_filter_xz.c
  archive_read_support_filter_zstd.c
  archive_read_support_format_7zip.c
  archive_read_support_format_all.c
  archive_read_support_format_ar.c
  archive_read_support_format_by_code.c
  archive_read_support_format_cab.c
  archive_read_support_format_cpio.c
  archive_read_support_format_empty.c
  archive_read_support_format_iso9660.c
  archive_read_support_format_lha.c
  archive_read_support_format_mtree.c
  archive_read_support_format_rar.c
  archive_read_support_format_rar5.c
  archive_read_support_format_raw.c
  archive_read_support_format_tar.c
  archive_read_support_format_warc.c
  archive_read_support_format_xar.c
  archive_read_support_format_zip.c
  archive_string.c
  archive_string.h
  archive_string_composition.h
  archive_string_sprintf.c
  archive_time.c
  archive_time_private.h
  archive_util.c
  archive_version_details.c
  archive_virtual.c
  archive_write.c
  archive_write_disk_posix.c
  archive_write_disk_private.h
  archive_write_disk_set_standard_lookup.c
  archive_write_private.h
  archive_write_open_fd.c
  archive_write_open_file.c
  archive_write_open_filename.c
  archive_write_open_memory.c
  archive_write_add_filter.c
  archive_write_add_filter_b64encode.c
  archive_write_add_filter_by_name.c
  archive_write_add_filter_bzip2.c
  archive_write_add_filter_compress.c
  archive_write_add_filter_grzip.c
  archive_write_add_filter_gzip.c
  archive_write_add_filter_lrzip.c
  archive_write_add_filter_lz4.c
  archive_write_add_filter_lzop.c
  archive_write_add_filter_none.c
  archive_write_add_filter_program.c
  archive_write_add_filter_uuencode.c
  archive_write_add_filter_xz.c
  archive_write_add_filter_zstd.c
  archive_write_set_format.c
  archive_write_set_format_7zip.c
  archive_write_set_format_ar.c
  archive_write_set_format_by_name.c
  archive_write_set_format_cpio.c
  archive_write_set_format_cpio_binary.c
  archive_write_set_format_cpio_newc.c
  archive_write_set_format_cpio_odc.c
  archive_write_set_format_filter_by_ext.c
  archive_write_set_format_gnutar.c
  archive_write_set_format_iso9660.c
  archive_write_set_format_mtree.c
  archive_write_set_format_pax.c
  archive_write_set_format_private.h
  archive_write_set_format_raw.c
  archive_write_set_format_shar.c
  archive_write_set_format_ustar.c
  archive_write_set_format_v7tar.c
  archive_write_set_format_warc.c
  archive_write_set_format_xar.c
  archive_write_set_format_zip.c
  archive_write_set_options.c
  archive_write_set_passphrase.c
  archive_xxhash.h
  filter_fork_posix.c
  filter_fork.h
  xxhash.c
)

# Man pages
SET(libarchive_MANS
  archive_entry.3
  archive_entry_acl.3
  archive_entry_linkify.3
  archive_entry_misc.3
  archive_entry_paths.3
  archive_entry_perms.3
  archive_entry_stat.3
  archive_entry_time.3
  archive_read.3
  archive_read_add_passphrase.3
  archive_read_data.3
  archive_read_disk.3
  archive_read_extract.3
  archive_read_filter.3
  archive_read_format.3
  archive_read_free.3
  archive_read_header.3
  archive_read_new.3
  archive_read_open.3
  archive_read_set_options.3
  archive_util.3
  archive_write.3
  archive_write_blocksize.3
  archive_write_data.3
  archive_write_disk.3
  archive_write_filter.3
  archive_write_finish_entry.3
  archive_write_format.3
  archive_write_free.3
  archive_write_header.3
  archive_write_new.3
  archive_write_open.3
  archive_write_set_options.3
  archive_write_set_passphrase.3
  cpio.5
  libarchive.3
  libarchive_changes.3
  libarchive_internals.3
  libarchive-formats.5
  mtree.5
  tar.5
)

IF(WIN32 AND NOT CYGWIN)
  LIST(APPEND libarchive_SOURCES archive_entry_copy_bhfi.c)
  LIST(APPEND libarchive_SOURCES archive_read_disk_windows.c)
  LIST(APPEND libarchive_SOURCES archive_windows.c)
  LIST(APPEND libarchive_SOURCES archive_windows.h)
  LIST(APPEND libarchive_SOURCES archive_write_disk_windows.c)
  LIST(APPEND libarchive_SOURCES filter_fork_windows.c)
ENDIF(WIN32 AND NOT CYGWIN)

IF(ARCHIVE_BLAKE2)
  LIST(APPEND libarchive_SOURCES archive_blake2sp_ref.c)
  LIST(APPEND libarchive_SOURCES archive_blake2s_ref.c)
ENDIF(ARCHIVE_BLAKE2)

IF(ARCHIVE_ACL_DARWIN)
  LIST(APPEND libarchive_SOURCES archive_disk_acl_darwin.c)
ELSEIF(ARCHIVE_ACL_FREEBSD)
  LIST(APPEND libarchive_SOURCES archive_disk_acl_freebsd.c)
ELSEIF(ARCHIVE_ACL_LIBACL)
  LIST(APPEND libarchive_SOURCES archive_disk_acl_linux.c)
ELSEIF(ARCHIVE_ACL_SUNOS)
  LIST(APPEND libarchive_SOURCES archive_disk_acl_sunos.c)
ENDIF()

# Libarchive is a shared library
IF(BUILD_SHARED_LIBS)
  ADD_LIBRARY(archive SHARED ${libarchive_SOURCES} ${include_HEADERS})
  TARGET_INCLUDE_DIRECTORIES(archive PUBLIC .)
  TARGET_LINK_LIBRARIES(archive ${ADDITIONAL_LIBS})
  SET_TARGET_PROPERTIES(archive PROPERTIES
                        VERSION ${SOVERSION_FULL}
                        SOVERSION ${SOVERSION}
                        MACHO_COMPATIBILITY_VERSION ${MACHO_COMPATIBILITY_VERSION}
                        MACHO_CURRENT_VERSION ${MACHO_CURRENT_VERSION})
  IF(WIN32 AND MSVC AND MSVC_USE_STATIC_CRT)
    SET_PROPERTY(TARGET archive PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  ENDIF(WIN32 AND MSVC AND MSVC_USE_STATIC_CRT)
ENDIF(BUILD_SHARED_LIBS)

# archive_static is a static library
ADD_LIBRARY(archive_static STATIC ${libarchive_SOURCES} ${include_HEADERS})
TARGET_INCLUDE_DIRECTORIES(archive_static PUBLIC .)
TARGET_LINK_LIBRARIES(archive_static ${ADDITIONAL_LIBS})
SET_TARGET_PROPERTIES(archive_static PROPERTIES COMPILE_DEFINITIONS
  LIBARCHIVE_STATIC)
IF(WIN32 AND MSVC AND MSVC_USE_STATIC_CRT)
  SET_PROPERTY(TARGET archive_static PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
ENDIF(WIN32 AND MSVC AND MSVC_USE_STATIC_CRT)
# On Posix systems, libarchive.so and libarchive.a can co-exist.
IF(NOT WIN32 OR CYGWIN OR NOT BUILD_SHARED_LIBS)
  SET_TARGET_PROPERTIES(archive_static PROPERTIES OUTPUT_NAME archive)
ENDIF(NOT WIN32 OR CYGWIN OR NOT BUILD_SHARED_LIBS)

IF(ENABLE_INSTALL)
  # How to install the libraries
  IF(BUILD_SHARED_LIBS)
    INSTALL(TARGETS archive
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  ENDIF(BUILD_SHARED_LIBS)
  INSTALL(TARGETS archive_static
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  INSTALL_MAN(${libarchive_MANS})
  INSTALL(FILES ${include_HEADERS} DESTINATION include)
ENDIF()

add_subdirectory(test)
