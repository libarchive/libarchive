/*-
* Copyright (c) 2003-2007 Tim Kientzle
* Copyright (c) 2011 Andres Mejia
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "archive_platform.h"

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <time.h>

#include "archive.h"
#include "archive_endian.h"
#include "archive_entry.h"
#include "archive_entry_locale.h"
#include "archive_private.h"
#include "archive_read_private.h"

/* RAR signature, also known as the mark header */
#define RAR_SIGNATURE "\x52\x61\x72\x21\x1A\x07\x00"

/* Header types */
#define MARK_HEAD    0x72
#define MAIN_HEAD    0x73
#define FILE_HEAD    0x74
#define COMM_HEAD    0x75
#define AV_HEAD      0x76
#define SUB_HEAD     0x77
#define PROTECT_HEAD 0x78
#define SIGN_HEAD    0x79
#define NEWSUB_HEAD  0x7a
#define ENDARC_HEAD  0x7b

/* Main Header Flags */
#define MHD_VOLUME       0x0001
#define MHD_COMMENT      0x0002
#define MHD_LOCK         0x0004
#define MHD_SOLID        0x0008
#define MHD_NEWNUMBERING 0x0010
#define MHD_AV           0x0020
#define MHD_PROTECT      0x0040
#define MHD_PASSWORD     0x0080
#define MHD_FIRSTVOLUME  0x0100
#define MHD_ENCRYPTVER   0x0200

/* Flags common to all headers */
#define HD_MARKDELETION     0x4000
#define HD_ADD_SIZE_PRESENT 0x8000

/* File Header Flags */
#define FHD_SPLIT_BEFORE 0x0001
#define FHD_SPLIT_AFTER  0x0002
#define FHD_PASSWORD     0x0004
#define FHD_COMMENT      0x0008
#define FHD_SOLID        0x0010
#define FHD_LARGE        0x0100
#define FHD_UNICODE      0x0200
#define FHD_SALT         0x0400
#define FHD_VERSION      0x0800
#define FHD_EXTTIME      0x1000
#define FHD_EXTFLAGS     0x2000

/* File dictionary sizes */
#define DICTIONARY_SIZE_64   0
#define DICTIONARY_SIZE_128  1
#define DICTIONARY_SIZE_256  2
#define DICTIONARY_SIZE_512  3
#define DICTIONARY_SIZE_1024 4
#define DICTIONARY_SIZE_2048 5
#define DICTIONARY_SIZE_4096 6

#define FILE_IS_DIRECTORY    7

/* OS Flags */
#define OS_MSDOS  0
#define OS_OS2    1
#define OS_WIN32  2
#define OS_UNIX   3
#define OS_MAC_OS 4
#define OS_BEOS   5

/* Compression Methods */
#define COMPRESS_METHOD_STORE   0x30
/* LZSS */
#define COMPRESS_METHOD_FASTEST 0x31
#define COMPRESS_METHOD_FAST    0x32
#define COMPRESS_METHOD_NORMAL  0x33
/* PPMd Variant H */
#define COMPRESS_METHOD_GOOD    0x34
#define COMPRESS_METHOD_BEST    0x35

#define CRC_POLYNOMIAL 0xEDB88320

#define NS_UNIT 10000000

/* Define this here for non-Windows platforms */
#if !((defined(__WIN32__) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__))
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#endif

/* Fields common to all headers */
struct rar_header
{
  char crc[2];
  char type;
  char flags[2];
  char size[2];
};

/* Fields common to all file headers */
struct rar_file_header
{
  char pack_size[4];
  char unp_size[4];
  char host_os;
  char file_crc[4];
  char file_time[4];
  char unp_ver;
  char method;
  char name_size[2];
  char file_attr[4];
};

struct rar
{
  /* Entries from main RAR header */
  unsigned main_flags;
  char reserved1[2];
  char reserved2[4];
  char encryptver;

  /* File header entries */
  char compression_method;
  unsigned file_flags;
  int64_t packed_size;
  int64_t unp_size;
  time_t mtime;
  long mnsec;
  mode_t mode;

  /* File header optional entries */
  char salt[8];
  time_t atime;
  long ansec;
  time_t ctime;
  long cnsec;
  time_t arctime;
  long arcnsec;

  /* Fields to help with tracking decompression of files. */
  int64_t bytes_remaining;
  int64_t offset;
};

static int archive_read_format_rar_bid(struct archive_read *);
static int archive_read_format_rar_read_header(struct archive_read *,
    struct archive_entry *);
static int archive_read_format_rar_read_data(struct archive_read *,
    const void **, size_t *, int64_t *);
static int archive_read_format_rar_read_data_skip(struct archive_read *a);
static int archive_read_format_rar_cleanup(struct archive_read *);

/* Support functions */
static int read_header(struct archive_read *, struct archive_entry *, char);
static time_t get_time(int time);
static void read_exttime(struct archive_entry *, const char *, struct rar *);
static int read_symlink_stored(struct archive_read *, struct archive_entry *,
                               struct archive_string_conv *);
static int read_data_stored(struct archive_read *, const void **, size_t *,
                            int64_t *);

int
archive_read_support_format_rar(struct archive *_a)
{
  struct archive_read *a = (struct archive_read *)_a;
  struct rar *rar;
  int r;

  archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
                      "archive_read_support_format_rar");

  rar = (struct rar *)malloc(sizeof(*rar));
  if (rar == NULL)
  {
    archive_set_error(&a->archive, ENOMEM, "Can't allocate rar data");
    return (ARCHIVE_FATAL);
  }
  memset(rar, 0, sizeof(*rar));

  r = __archive_read_register_format(a,
                                     rar,
                                     "rar",
                                     archive_read_format_rar_bid,
                                     NULL,
                                     archive_read_format_rar_read_header,
                                     archive_read_format_rar_read_data,
                                     archive_read_format_rar_read_data_skip,
                                     archive_read_format_rar_cleanup);

  if (r != ARCHIVE_OK)
    free(rar);
  return (r);
}

static int
archive_read_format_rar_bid(struct archive_read *a)
{
  const char *p;

  if ((p = __archive_read_ahead(a, 7, NULL)) == NULL)
    return (-1);

  if (memcmp(p, RAR_SIGNATURE, 7) == 0)
    return (30);

  return (0);
}

static int
archive_read_format_rar_read_header(struct archive_read *a,
                                    struct archive_entry *entry)
{
  const void *h;
  const char *p;
  struct rar *rar;
  size_t skip;
  char head_type;
  int ret;
  unsigned flags;

  a->archive.archive_format = ARCHIVE_FORMAT_RAR;
  if (a->archive.archive_format_name == NULL)
    a->archive.archive_format_name = "RAR";

  rar = (struct rar *)(a->format->data);

  /* RAR files can be generated without EOF headers, so return ARCHIVE_EOF if
  * this fails.
  */
  if ((h = __archive_read_ahead(a, 7, NULL)) == NULL)
    return (ARCHIVE_EOF);
  p = h;

  while (1)
  {
    head_type = p[2];
    switch(head_type)
    {
    case MARK_HEAD:
      __archive_read_consume(a, 7);
      if ((h = __archive_read_ahead(a, 7, NULL)) == NULL)
        return (ARCHIVE_FATAL);
      p = h;
      continue;

    case MAIN_HEAD:
      rar->main_flags = archive_le16dec(p + 3);
      skip = archive_le16dec(p + 5);
      memcpy(rar->reserved1, p + 7, sizeof(rar->reserved1));
      memcpy(rar->reserved2, p + 7 + sizeof(rar->reserved1),
             sizeof(rar->reserved2));
      if (rar->main_flags & MHD_ENCRYPTVER)
        rar->encryptver = *(p + 7 + sizeof(rar->reserved1) +
                            sizeof(rar->reserved2));

      if (rar->main_flags & MHD_VOLUME ||
          rar->main_flags & MHD_FIRSTVOLUME)
      {
        archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                          "RAR volume support unavailable.");
        return (ARCHIVE_FATAL);
      }
      if (rar->main_flags & MHD_PASSWORD)
      {
        archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                          "RAR encryption support unavailable.");
        return (ARCHIVE_FATAL);
      }

      __archive_read_consume(a, skip);
      if ((h = __archive_read_ahead(a, 7, NULL)) == NULL)
        return (ARCHIVE_FATAL);
      p = h;
      continue;

    case FILE_HEAD:
      return read_header(a, entry, head_type);

    case COMM_HEAD:
    case AV_HEAD:
    case SUB_HEAD:
    case PROTECT_HEAD:
    case SIGN_HEAD:
      flags = archive_le16dec(p + 3);
      skip = archive_le16dec(p + 5);
      if (flags & HD_ADD_SIZE_PRESENT)
      {
        if ((h = __archive_read_ahead(a, 13, NULL)) == NULL)
          return (ARCHIVE_FATAL);
        p = h;
        skip += archive_le32dec(p + 7);
      }
      __archive_read_consume(a, skip);
      if ((h = __archive_read_ahead(a, 7, NULL)) == NULL)
        return (ARCHIVE_FATAL);
      p = h;
      continue;

    case NEWSUB_HEAD:
      if ((ret = read_header(a, entry, head_type)) < ARCHIVE_WARN)
        return ret;
      if ((h = __archive_read_ahead(a, 7, NULL)) == NULL)
        return (ARCHIVE_FATAL);
      p = h;
      continue;

    case ENDARC_HEAD:
      return (ARCHIVE_EOF);
    }
    break;
  }

  archive_set_error(&a->archive,  ARCHIVE_ERRNO_FILE_FORMAT, "Bad RAR file");
  return (ARCHIVE_FATAL);
}

static int
archive_read_format_rar_read_data(struct archive_read *a, const void **buff,
                                  size_t *size, int64_t *offset)
{
  struct rar *rar = (struct rar *)(a->format->data);
  switch (rar->compression_method)
  {
  case COMPRESS_METHOD_STORE:
    return read_data_stored(a, buff, size, offset);

  case COMPRESS_METHOD_FASTEST:
  case COMPRESS_METHOD_FAST:
  case COMPRESS_METHOD_NORMAL:
  case COMPRESS_METHOD_GOOD:
  case COMPRESS_METHOD_BEST:
  default:
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "Unsupported compression method for RAR file.");
  }
  return (ARCHIVE_FATAL);
}

static int
archive_read_format_rar_read_data_skip(struct archive_read *a)
{
  struct rar *rar;

  rar = (struct rar *)(a->format->data);
  __archive_read_consume(a, rar->packed_size);
  return (ARCHIVE_OK);
}

static int
archive_read_format_rar_cleanup(struct archive_read *a)
{
  struct rar *rar;

  rar = (struct rar *)(a->format->data);
  free(rar);
  (a->format->data) = NULL;
  return (ARCHIVE_OK);
}

static int
read_header(struct archive_read *a, struct archive_entry *entry,
            char head_type)
{
  const void *h;
  const char *p;
  struct rar *rar;
  struct rar_header rar_header;
  struct rar_file_header file_header;
  int64_t header_size;
  unsigned filename_size;
  char *filename;
  char *strp;
  char packed_size[8];
  char unp_size[8];
  int time;
  struct archive_string_conv *sconv;
  int ret = (ARCHIVE_OK), ret2;

  rar = (struct rar *)(a->format->data);

  if ((h = __archive_read_ahead(a, 7, NULL)) == NULL)
    return (ARCHIVE_FATAL);
  p = h;
  memcpy(&rar_header, p, sizeof(rar_header));
  rar->file_flags = archive_le16dec(rar_header.flags);
  header_size = archive_le16dec(rar_header.size);
  __archive_read_consume(a, 7);

  if (!(rar->file_flags & FHD_SOLID))
  {
    rar->compression_method = 0;
    rar->packed_size = 0;
    rar->unp_size = 0;
    rar->mtime = 0;
    rar->ctime = 0;
    rar->atime = 0;
    rar->arctime = 0;
    rar->mode = 0;
    memset(&rar->salt, 0, sizeof(rar->salt));
    rar->atime = 0;
    rar->ansec = 0;
    rar->ctime = 0;
    rar->cnsec = 0;
    rar->mtime = 0;
    rar->mnsec = 0;
    rar->arctime = 0;
    rar->arcnsec = 0;
  }

  if ((h = __archive_read_ahead(a, header_size - 7, NULL)) == NULL)
    return (ARCHIVE_FATAL);
  p = h;
  memcpy(&file_header, p, sizeof(file_header));
  p += sizeof(file_header);

  rar->compression_method = file_header.method;

  time = archive_le32dec(file_header.file_time);
  rar->mtime = get_time(time);

  if (rar->file_flags & FHD_PASSWORD)
  {
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "RAR encryption support unavailable.");
    return (ARCHIVE_FATAL);
  }

  if (rar->file_flags & FHD_LARGE)
  {
    memcpy(packed_size, file_header.pack_size, 4);
    memcpy(packed_size + 4, p, 4); /* High pack size */
    p += 4;
    memcpy(unp_size, file_header.unp_size, 4);
    memcpy(unp_size + 4, p, 4); /* High unpack size */
    p += 4;
    rar->packed_size = archive_le64dec(&packed_size);
    rar->unp_size = archive_le64dec(&unp_size);
  }
  else
  {
    rar->packed_size = archive_le32dec(file_header.pack_size);
    rar->unp_size = archive_le32dec(file_header.unp_size);
  }

  /* TODO: RARv3 subblocks contain comments. For now the complete block is
   * consumed at the end.
   */
  if (head_type == NEWSUB_HEAD)
    header_size += rar->packed_size;

  filename_size = archive_le16dec(file_header.name_size);
  if ((filename = malloc(filename_size)) == NULL)
    return (ARCHIVE_FATAL);
  memcpy(filename, p, filename_size);
  filename[filename_size] = '\0';
  if (rar->file_flags & FHD_UNICODE)
  {
    if (filename_size != strlen(filename))
    {
      memcpy(filename, p + strlen(filename), filename_size - strlen(filename));
      filename[filename_size - strlen(filename)] = '\0';
    }
    sconv = archive_string_conversion_from_charset(&a->archive, "UTF-8", 1);
    if (sconv == NULL)
    {
      free(filename);
      return (ARCHIVE_FATAL);
    }
  }
  else
    sconv = archive_string_default_conversion_for_read(&(a->archive));

#if !((defined(__WIN32__) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__))
  while ((strp = strchr(filename, '\\')) != NULL)
    *strp = '/';
#else
  (void)strp;
#endif
  p += filename_size;

  if (rar->file_flags & FHD_SALT)
  {
    memcpy(rar->salt, p, 8);
    p += 8;
  }

  if (rar->file_flags & FHD_EXTTIME)
    read_exttime(entry, p, rar);

  __archive_read_consume(a, header_size - 7);

  switch(file_header.host_os)
  {
  case OS_WIN32:
    rar->mode = archive_le32dec(file_header.file_attr);
    if (rar->mode & FILE_ATTRIBUTE_DIRECTORY)
      rar->mode = AE_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
    else
      rar->mode = AE_IFREG;
    rar->mode |= S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    break;

  case OS_UNIX:
    rar->mode = archive_le32dec(file_header.file_attr);
    break;

  case OS_MSDOS:
  case OS_OS2:
  case OS_MAC_OS:
  case OS_BEOS:
  default:
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "Unknown file attributes from RAR file's host OS");
    free(filename);
    return (ARCHIVE_FATAL);
  }

  rar->bytes_remaining = rar->packed_size;
  rar->offset = 0;

  /* Don't set any archive entries for non-file header types */
  if (head_type == NEWSUB_HEAD)
    return ret;

  archive_entry_set_mtime(entry, rar->mtime, rar->mnsec);
  archive_entry_set_ctime(entry, rar->ctime, rar->cnsec);
  archive_entry_set_atime(entry, rar->atime, rar->ansec);
  archive_entry_set_size(entry, rar->unp_size);
  archive_entry_set_mode(entry, rar->mode);

  if (archive_entry_copy_pathname_l(entry, filename, strlen(filename), sconv))
  {
    if (errno == ENOMEM)
    {
      archive_set_error(&a->archive, ENOMEM,
                        "Can't allocate memory for Pathname");
      return (ARCHIVE_FATAL);
    }
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "Pathname cannot be converted from %s to current locale.",
                      archive_string_conversion_charset_name(sconv));
    ret = (ARCHIVE_WARN);
  }
  free(filename);

  if (((rar->mode) & AE_IFMT) == AE_IFLNK)
  {
    switch (rar->compression_method)
    {
    case COMPRESS_METHOD_STORE:
      if ((ret2 = read_symlink_stored(a, entry, sconv)) < (ARCHIVE_WARN))
        return ret2;
      if (ret > ret2)
        ret = ret2;
      break;

    case COMPRESS_METHOD_FASTEST:
    case COMPRESS_METHOD_FAST:
    case COMPRESS_METHOD_NORMAL:
    case COMPRESS_METHOD_GOOD:
    case COMPRESS_METHOD_BEST:
    default:
      archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                        "Unsupported compression method for RAR file.");
      return (ARCHIVE_FATAL);
    }
  }

  return ret;
}

static time_t
get_time(int time)
{
  struct tm tm;
  tm.tm_sec = 2 * (time & 0x1f);
  tm.tm_min = (time >> 5) & 0x3f;
  tm.tm_hour = (time >> 11) & 0x1f;
  tm.tm_mday = (time >> 16) & 0x1f;
  tm.tm_mon = ((time >> 21) & 0x0f) - 1;
  tm.tm_year = ((time >> 25) & 0x7f) + 80;
  tm.tm_isdst = -1;
  return mktime(&tm);
}

static void
read_exttime(struct archive_entry *entry, const char *p, struct rar *rar)
{
  unsigned rmode, flags, rem, count;
  int time, i, j;
  struct tm *tm;
  time_t t;
  long nsec;

  flags = archive_le16dec(p);
  p += 2;

  for (i = 3; i >= 0; i--)
  {
    t = 0;
    if (i == 3)
      t = rar->mtime;
    rmode = flags >> i * 4;
    if (rmode & 8)
    {
      if (!t)
      {
        time = archive_le32dec(p);
        t = get_time(time);
        p += 4;
      }
      rem = 0;
      count = rmode & 3;
      for (j = 0; j < count; j++)
      {
        rem = ((*p) << 16) | (rem >> 8);
        p++;
      }
      tm = localtime(&t);
      nsec = tm->tm_sec + rem / NS_UNIT;
      if (rmode & 4)
      {
        tm->tm_sec++;
        t = mktime(tm);
      }
      if (i == 3)
      {
        rar->mtime = t;
        rar->mnsec = nsec;
      }
      else if (i == 2)
      {
        rar->ctime = t;
        rar->cnsec = nsec;
      }
      else if (i == 1)
      {
        rar->atime = t;
        rar->ansec = nsec;
      }
      else
      {
        rar->arctime = t;
        rar->arcnsec = nsec;
      }
    }
  }
}

static int
read_symlink_stored(struct archive_read *a, struct archive_entry *entry,
                    struct archive_string_conv *sconv)
{
  const void *h;
  const char *p;
  struct rar *rar;
  char *filename;
  int ret = (ARCHIVE_OK);

  rar = (struct rar *)(a->format->data);
  if ((h = __archive_read_ahead(a, rar->packed_size, NULL)) == NULL)
    return (ARCHIVE_FATAL);
  p = h;
  filename = malloc(rar->packed_size);
  memcpy(filename, p, rar->packed_size);
  filename[rar->packed_size] = '\0';

  if (archive_entry_copy_symlink_l(entry, filename, strlen(filename), sconv))
  {
    if (errno == ENOMEM)
    {
      archive_set_error(&a->archive, ENOMEM,
                        "Can't allocate memory for link");
      free(filename);
      return (ARCHIVE_FATAL);
    }
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "link cannot be converted from %s to current locale.",
                      archive_string_conversion_charset_name(sconv));
    ret = (ARCHIVE_WARN);
  }
  free(filename);
  return ret;
}

static int
read_data_stored(struct archive_read *a, const void **buff, size_t *size,
                 int64_t *offset)
{
  struct rar *rar;
  ssize_t bytes_avail;

  rar = (struct rar *)(a->format->data);
  if (rar->bytes_remaining == 0)
  {
    *buff = NULL;
    *size = 0;
    *offset = rar->offset;
    return (ARCHIVE_EOF);
  }

  *buff = __archive_read_ahead(a, 1, &bytes_avail);
  if (bytes_avail <= 0)
  {
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "Truncated RAR file data");
    return (ARCHIVE_FATAL);
  }
  if (bytes_avail > rar->bytes_remaining)
    bytes_avail = rar->bytes_remaining;

  *size = bytes_avail;
  *offset = rar->offset;
  rar->offset += bytes_avail;
  rar->bytes_remaining -= bytes_avail;
  return (ARCHIVE_OK);
}
