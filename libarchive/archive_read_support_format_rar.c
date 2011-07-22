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
#include <limits.h>

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
#define DICTIONARY_SIZE_64   0x00
#define DICTIONARY_SIZE_128  0x20
#define DICTIONARY_SIZE_256  0x40
#define DICTIONARY_SIZE_512  0x60
#define DICTIONARY_SIZE_1024 0x80
#define DICTIONARY_SIZE_2048 0xA0
#define DICTIONARY_SIZE_4096 0xC0
#define FILE_IS_DIRECTORY    0xE0
#define DICTIONARY_MASK      FILE_IS_DIRECTORY

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

#define DICTIONARY_MAX_SIZE 0x400000

#define MAINCODE_SIZE      299
#define OFFSETCODE_SIZE    60
#define LOWOFFSETCODE_SIZE 17
#define LENGTHCODE_SIZE    28
#define HUFFMAN_TABLE_SIZE \
  MAINCODE_SIZE + OFFSETCODE_SIZE + LOWOFFSETCODE_SIZE + LENGTHCODE_SIZE

#define MAX_SYMBOL_LENGTH 0xF
#define MAX_SYMBOLS       20

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

struct huffman_tree_node
{
  int branches[2];
};

struct huffman_table_entry
{
  unsigned int length;
  int value;
};

struct huffman_code
{
  struct huffman_tree_node *tree;
  int numentries;
  int minlength;
  int maxlength;
  int tablesize;
  struct huffman_table_entry *table;
};

struct lzss
{
  unsigned char *window;
  int mask;
  int64_t position;
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
  int64_t bitoffset;
  char valid;
  struct huffman_code maincode;
  struct huffman_code offsetcode;
  struct huffman_code lowoffsetcode;
  struct huffman_code lengthcode;
  unsigned char lengthtable[HUFFMAN_TABLE_SIZE];
  unsigned char *unp_buffer;
  struct lzss lzss;
  unsigned int dictionary_size;
  char output_last_match;
  unsigned int lastlength;
  unsigned int lastoffset;
  unsigned int oldoffset[4];
  unsigned int lastlowoffset;
  unsigned int numlowoffsetrepeats;
  off_t filterstart;
  char start_new_block;
  char start_new_table;
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
static void read_exttime(const char *, struct rar *);
static int read_symlink_stored(struct archive_read *, struct archive_entry *,
                               struct archive_string_conv *);
static int read_data_stored(struct archive_read *, const void **, size_t *,
                            int64_t *);
static int read_data_lzss(struct archive_read *, const void **, size_t *,
                          int64_t *);
static int parse_codes(struct archive_read *);
static void free_codes(struct archive_read *);
static unsigned char read_bits(struct archive_read *, char);
static unsigned int read_bits_32(struct archive_read *, char);
static int read_next_symbol(struct archive_read *, struct huffman_code *);
static void create_code(struct archive_read *, struct huffman_code *,
                        unsigned char *, int, char);
static int add_value(struct archive_read *, struct huffman_code *, int, int,
                     int);
static int new_node(struct huffman_code *);
static int make_table(struct huffman_code *);
static int make_table_recurse(struct huffman_code *, int,
                              struct huffman_table_entry *, int, int);
static off_t expand(struct archive_read *, off_t);
static int copy_from_lzss_window(struct archive_read *, const void **,
                                   int64_t, int);

/* Find last bit set */
static inline int
rar_fls(unsigned int word)
{
  word |= (word >>  1);
  word |= (word >>  2);
  word |= (word >>  4);
  word |= (word >>  8);
  word |= (word >> 16);
  return word - (word >> 1);
}

/* LZSS functions */
static inline int64_t
lzss_position(struct lzss *lzss)
{
  return lzss->position;
}

static inline int
lzss_mask(struct lzss *lzss)
{
  return lzss->mask;
}

static inline int
lzss_size(struct lzss *lzss)
{
  return lzss->mask + 1;
}

static inline int
lzss_offset_for_position(struct lzss *lzss, int64_t pos)
{
  return pos & lzss->mask;
}

static inline unsigned char *
lzss_pointer_for_position(struct lzss *lzss, int64_t pos)
{
  return &lzss->window[lzss_offset_for_position(lzss, pos)];
}

static inline int
lzss_current_offset(struct lzss *lzss)
{
  return lzss_offset_for_position(lzss, lzss->position);
}

static inline uint8_t *
lzss_current_pointer(struct lzss *lzss)
{
  return lzss_pointer_for_position(lzss, lzss->position);
}

static inline void
lzss_emit_literal(struct rar *rar, uint8_t literal)
{
  *lzss_current_pointer(&rar->lzss) = literal;
  rar->lzss.position++;
}

static inline void
lzss_emit_match(struct rar *rar, int offset, int length)
{
  int i, windowoffs = lzss_current_offset(&rar->lzss);
  for(i = 0; i < length; i++)
  {
    rar->lzss.window[(windowoffs + i) & lzss_mask(&rar->lzss)] =
      rar->lzss.window[(windowoffs + i - offset) & lzss_mask(&rar->lzss)];
  }
  rar->lzss.position += length;
}

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
    return read_data_lzss(a, buff, size, offset);

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
  switch (rar->compression_method)
  {
  case COMPRESS_METHOD_FASTEST:
  case COMPRESS_METHOD_FAST:
  case COMPRESS_METHOD_NORMAL:
    if (rar->offset)
    {
      while (rar->bitoffset > 0)
        rar->bitoffset -= 8 * __archive_read_consume(a, 1);
      break;
    }

  case COMPRESS_METHOD_STORE:
  case COMPRESS_METHOD_GOOD:
  case COMPRESS_METHOD_BEST:
  default:
    __archive_read_consume(a, rar->packed_size);
  }
  return (ARCHIVE_OK);
}

static int
archive_read_format_rar_cleanup(struct archive_read *a)
{
  struct rar *rar;

  rar = (struct rar *)(a->format->data);
  free_codes(a);
  free(rar->unp_buffer);
  free(rar->lzss.window);
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
  unsigned filename_size, offset, end;
  char *filename;
  char *strp;
  char packed_size[8];
  char unp_size[8];
  unsigned char highbyte, flagbyte, flagbits, length;
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
  else
  {
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "RAR solid archive support unavailable.");
    return (ARCHIVE_FATAL);
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
  if ((filename = malloc(filename_size+1)) == NULL)
    return (ARCHIVE_FATAL);
  memcpy(filename, p, filename_size);
  filename[filename_size] = '\0';
  if (rar->file_flags & FHD_UNICODE)
  {
    if (filename_size != strlen(filename))
    {
      end = filename_size;
      filename_size = 0;
      offset = strlen(filename) + 1;
      highbyte = *(p + offset++);
      flagbits = 0;
      while (offset < end)
      {
        if (!flagbits)
        {
          flagbyte = *(p + offset++);
          flagbits = 8;
        }
        flagbits -= 2;
        switch((flagbyte >> flagbits) & 3)
        {
          case 0:
            filename[filename_size++] = '\0';
            filename[filename_size++] = *(p + offset++);
            break;
          case 1:
            filename[filename_size++] = highbyte;
            filename[filename_size++] = *(p + offset++);
            break;
          case 2:
            filename[filename_size++] = *(p + offset + 1);
            filename[filename_size++] = *(p + offset);
            offset += 2;
            break;
          case 3:
          {
            length = *(p + offset++);
            while (length)
            {
              filename[filename_size++] = *(p + offset);
              length--;
            }
          }
          break;
        }
      }
      filename[filename_size++] = '\0';
      filename[filename_size++] = '\0';
    }
    sconv = archive_string_conversion_from_charset(&a->archive, "UTF-16BE", 1);
    if (sconv == NULL)
    {
      free(filename);
      return (ARCHIVE_FATAL);
    }
    strp = filename;
    while (memcmp(strp, "\x00\x00", 2))
    {
      if (!memcmp(strp, "\x00\\", 2))
        *(strp + 1) = '/';
      strp += 2;
    }
    p += offset;
  }
  else
  {
    sconv = archive_string_default_conversion_for_read(&(a->archive));
    while ((strp = strchr(filename, '\\')) != NULL)
      *strp = '/';
    p += filename_size;
  }

  if (rar->file_flags & FHD_SALT)
  {
    memcpy(rar->salt, p, 8);
    p += 8;
  }

  if (rar->file_flags & FHD_EXTTIME)
    read_exttime(p, rar);

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
  rar->lzss.position = rar->dictionary_size = rar->offset = rar->bitoffset = 0;
  rar->valid = 1;
  rar->start_new_table = 1;
  free(rar->unp_buffer);
  rar->unp_buffer = NULL;
  memset(rar->lengthtable, 0, sizeof(rar->lengthtable));

  /* Don't set any archive entries for non-file header types */
  if (head_type == NEWSUB_HEAD)
    return ret;

  archive_entry_set_mtime(entry, rar->mtime, rar->mnsec);
  archive_entry_set_ctime(entry, rar->ctime, rar->cnsec);
  archive_entry_set_atime(entry, rar->atime, rar->ansec);
  archive_entry_set_size(entry, rar->unp_size);
  archive_entry_set_mode(entry, rar->mode);

  if (archive_entry_copy_pathname_l(entry, filename, filename_size, sconv))
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
    if ((ret2 = read_symlink_stored(a, entry, sconv)) < (ARCHIVE_WARN))
      return ret2;
    if (ret > ret2)
      ret = ret2;
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
read_exttime(const char *p, struct rar *rar)
{
  unsigned rmode, flags, rem, j, count;
  int time, i;
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
  filename = malloc(rar->packed_size+1);
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

static int
read_data_lzss(struct archive_read *a, const void **buff, size_t *size,
               int64_t *offset)
{
  struct rar *rar;
  ssize_t bytes_avail;
  off_t start, end, actualend;
  int ret = (ARCHIVE_OK);

  rar = (struct rar *)(a->format->data);
  if (!rar->valid)
    return (ARCHIVE_FATAL);

  if (rar->dictionary_size && rar->offset >= rar->unp_size)
  {
    *buff = NULL;
    *size = 0;
    *offset = rar->offset;
    return (ARCHIVE_EOF);
  }

  if (rar->dictionary_size && rar->bytes_remaining > 0)
  {
    *offset = rar->offset;
    if (rar->offset + rar->bytes_remaining > rar->unp_size)
      *size = rar->unp_size - rar->offset;
    else
      *size = rar->bytes_remaining;
    ret = copy_from_lzss_window(a, buff, *offset, *size);
    rar->offset += *size;
    rar->bytes_remaining -= *size;
    return ret;
  }

  if (rar->start_new_table && ((ret = parse_codes(a)) < (ARCHIVE_WARN)))
    return (ARCHIVE_FATAL);

  __archive_read_ahead(a, 1, &bytes_avail);
  if (bytes_avail <= 0)
  {
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "Truncated RAR file data");
    return (ARCHIVE_FATAL);
  }
  if (bytes_avail > rar->bytes_remaining)
    bytes_avail = rar->bytes_remaining;
  
  start = rar->offset;
  end = start + rar->dictionary_size;
  rar->filterstart = LONG_MAX;

  if ((actualend = expand(a, end)) < 0)
    return (ARCHIVE_FATAL);

  rar->bytes_remaining = actualend - start;
  *offset = rar->offset;
  if (rar->offset + rar->bytes_remaining > rar->unp_size)
    *size = rar->unp_size - rar->offset;
  else
    *size = rar->bytes_remaining;
  ret = copy_from_lzss_window(a, buff, *offset, *size);
  rar->offset += *size;
  rar->bytes_remaining -= *size;
  return ret;
}

static int
parse_codes(struct archive_read *a)
{
  const void *h;
  ssize_t bytes_avail;
  int i, j, val, n;
  unsigned char bitlengths[MAX_SYMBOLS], zerocount;
  struct huffman_code precode;
  struct rar *rar = (struct rar *)(a->format->data);

  free_codes(a);

  /* Skip to the next byte */
  while (rar->bitoffset % 8)
    rar->bitoffset++;

  if ((h = __archive_read_ahead(a, 1, NULL)) == NULL)
  {
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "Truncated RAR file data");
    return (ARCHIVE_FATAL);
  }

  /* PPMd block flag */
  if (read_bits(a, 1))
  {
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "Unsupported compression method for RAR file.");
    return (ARCHIVE_FATAL);
  }

  /* Keep existing table flag */
  if (!read_bits(a, 1))
    memset(rar->lengthtable, 0, sizeof(rar->lengthtable));

  memset(&bitlengths, 0, sizeof(bitlengths));
  for (i = 0; i < MAX_SYMBOLS;)
  {
    bitlengths[i++] = read_bits(a, 4);
    if (bitlengths[i-1] == 0xF)
    {
      zerocount = read_bits(a, 4);
      if (zerocount)
      {
        i--;
        for (j = 0; j < zerocount + 2 && i < MAX_SYMBOLS; j++)
          bitlengths[i++] = 0;
      }
    }
  }

  rar->bitoffset -= 8 * __archive_read_consume(a, (rar->bitoffset / 8));
  h = __archive_read_ahead(a, (rar->bitoffset / 8) + 1, &bytes_avail);
  if (bytes_avail <= 0)
  {
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "Truncated RAR file data");
    return (ARCHIVE_FATAL);
  }

  memset(&precode, 0, sizeof(precode));
  create_code(a, &precode, bitlengths, MAX_SYMBOLS, MAX_SYMBOL_LENGTH);

  for (i = 0; i < HUFFMAN_TABLE_SIZE;)
  {
    if ((val = read_next_symbol(a, &precode)) < 0)
      return (ARCHIVE_FATAL);
    if (val < 16)
    {
      rar->lengthtable[i] = (rar->lengthtable[i] + val) & 0xF;
      i++;
    }
    else if (val < 18)
    {
      if (i == 0)
      {
        archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                          "Internal error extracting RAR file.");
        return (ARCHIVE_FATAL);
      }

      if(val == 16)
        n = read_bits(a, 3) + 3;
      else
        n = read_bits(a, 7) + 11;

      for (j = 0; j < n && i < HUFFMAN_TABLE_SIZE; j++)
      {
        rar->lengthtable[i] = rar->lengthtable[i-1];
        i++;
      }
    }
    else
    {
      if(val == 18)
        n = read_bits(a, 3) + 3;
      else
        n = read_bits(a, 7) + 11;

      for(j = 0; j < n && i < HUFFMAN_TABLE_SIZE; j++)
        rar->lengthtable[i++] = 0;
    }
  }

  create_code(a, &rar->maincode, &rar->lengthtable[0], MAINCODE_SIZE,
              MAX_SYMBOL_LENGTH);
  create_code(a, &rar->offsetcode, &rar->lengthtable[MAINCODE_SIZE],
              OFFSETCODE_SIZE, MAX_SYMBOL_LENGTH);
  create_code(a, &rar->lowoffsetcode,
              &rar->lengthtable[MAINCODE_SIZE + OFFSETCODE_SIZE],
              LOWOFFSETCODE_SIZE, MAX_SYMBOL_LENGTH);
  create_code(a, &rar->lengthcode,
              &rar->lengthtable[MAINCODE_SIZE + OFFSETCODE_SIZE +
              LOWOFFSETCODE_SIZE], LENGTHCODE_SIZE, MAX_SYMBOL_LENGTH);

  if (!rar->dictionary_size || !rar->lzss.window)
  {
    /* Seems as though dictionary sizes are not used. Even so, minimize
     * memory usage as much as possible.
     */
    if (rar->unp_size >= DICTIONARY_MAX_SIZE)
      rar->dictionary_size = DICTIONARY_MAX_SIZE;
    else
      rar->dictionary_size = rar_fls(rar->unp_size) << 1;
    rar->lzss.window = (unsigned char *)realloc(rar->lzss.window,
                                                rar->dictionary_size);
    memset(rar->lzss.window, 0, rar->dictionary_size);
    rar->lzss.mask = rar->dictionary_size - 1;
  }

  rar->start_new_table = 0;
  rar->bitoffset -= 8 * __archive_read_consume(a, (rar->bitoffset / 8));
  return (ARCHIVE_OK);
}

static void
free_codes(struct archive_read *a)
{
  struct rar *rar = (struct rar *)(a->format->data);
  free(rar->maincode.tree);
  free(rar->offsetcode.tree);
  free(rar->lowoffsetcode.tree);
  free(rar->lengthcode.tree);
  free(rar->maincode.table);
  free(rar->offsetcode.table);
  free(rar->lowoffsetcode.table);
  free(rar->lengthcode.table);
  memset(&rar->maincode, 0, sizeof(rar->maincode));
  memset(&rar->offsetcode, 0, sizeof(rar->offsetcode));
  memset(&rar->lowoffsetcode, 0, sizeof(rar->lowoffsetcode));
  memset(&rar->lengthcode, 0, sizeof(rar->lengthcode));
}

static unsigned char
read_bits(struct archive_read *a, char length)
{
  unsigned char ret, m;
  const unsigned char *p;
  struct rar *rar = (struct rar *)(a->format->data);
  if (length <= 0 || length > 8)
  {
    rar->valid = 0;
    return 0;
  }
  if ((p = __archive_read_ahead(a,
    ((rar->bitoffset + length) / 8) + 1, NULL)) == NULL)
  {
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "Truncated RAR file data");
    rar->valid = 0;
    return 0;
  }
  m = rar->bitoffset % 8;
  ret = (*(p + (rar->bitoffset / 8))) << m;
  ret |= (*(p + ((rar->bitoffset + (8 - m)) / 8))) >> (8 - m);
  rar->bitoffset += length;
  return ret >> (8 - length);
}

static unsigned int
read_bits_32(struct archive_read *a, char length)
{
  unsigned char bits[4];
  struct rar *rar = (struct rar *)(a->format->data);
  if (length <= 0 || length > 32)
  {
    rar->valid = 0;
    return 0;
  }
  memset(&bits, 0, sizeof(bits));
  while (length > 0 && rar->valid)
  {
    if (length % 8)
    {
      bits[(32 - length) / 8] = read_bits(a, length % 8);
      length -= length % 8;
    }
    else
    {
      bits[(32 - length) / 8] = read_bits(a, 8);
      length -= 8;
    }
  }
  if (!rar->valid)
    return 0;
  return archive_be32dec(&bits);
}

static int
read_next_symbol(struct archive_read *a, struct huffman_code *code)
{
  unsigned char bit;
  unsigned int bits;
  int length, value, node;
  struct rar *rar;

  if (!code->table)
  {
    if (make_table(code) != (ARCHIVE_OK))
    {
      archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                        "Error in generating table.");
      return -1;
    }
  }

  rar = (struct rar *)(a->format->data);

  /* Look ahead (peek) at bits */
  bits = read_bits_32(a, code->tablesize);
  rar->bitoffset -= code->tablesize;

  length = code->table[bits].length;
  value = code->table[bits].value;

  if (length < 0)
  {
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "Invalid prefix code in bitstream");
    return -1;
  }

  if (length <= code->tablesize)
  {
    /* Skip length bits */
    rar->bitoffset += length;
    return value;
  }

  /* Skip tablesize bits */
  rar->bitoffset += code->tablesize;

  node = value;
  while (!(code->tree[node].branches[0] ==
    code->tree[node].branches[1]))
  {
    bit = read_bits(a, 1);
    if (code->tree[node].branches[bit] < 0)
    {
      archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                        "Invalid prefix code in bitstream");
      return -1;
    }
    node = code->tree[node].branches[bit];
  }

  return code->tree[node].branches[0];
}

static void
create_code(struct archive_read *a, struct huffman_code *code,
            unsigned char *lengths, int numsymbols, char maxlength)
{
  int i, j, codebits = 0, symbolsleft = numsymbols;
  new_node(code);
  code->numentries = 1;
  code->minlength = INT_MAX;
  code->maxlength = INT_MIN;
  codebits = 0;
  for(i = 1; i <= maxlength; i++)
  {
    for(j = 0; j < numsymbols; j++)
    {
      if (lengths[j] != i) continue;
      add_value(a, code, j, codebits, i);
      codebits++;
      if (--symbolsleft <= 0) { break; break; }
    }
    codebits <<= 1;
  }
}

static int
add_value(struct archive_read *a, struct huffman_code *code, int value,
          int codebits, int length)
{
  int repeatpos, lastnode, bitpos, bit, repeatnode, nextnode;

  free(code->table);
  code->table = NULL;

  if(length > code->maxlength)
    code->maxlength = length;
  if(length < code->minlength)
    code->minlength = length;

  repeatpos = -1;
  if (repeatpos == 0 || (repeatpos >= 0
    && (((codebits >> (repeatpos - 1)) & 3) == 0
    || ((codebits >> (repeatpos - 1)) & 3) == 3)))
  {
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "Invalid repeat position");
    return (ARCHIVE_FATAL);
  }

  lastnode = 0;
  for (bitpos = length - 1; bitpos >= 0; bitpos--)
  {
    bit = (codebits >> bitpos) & 1;

    /* Leaf node check */
    if (code->tree[lastnode].branches[0] ==
      code->tree[lastnode].branches[1])
    {
      archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                        "Prefix found");
      return (ARCHIVE_FATAL);
    }

    if (bitpos == repeatpos)
    {
      /* Open branch check */
      if (!(code->tree[lastnode].branches[bit] < 0))
      {
        archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                          "Invalid repeating code");
        return (ARCHIVE_FATAL);
      }

      repeatnode = new_node(code);
      nextnode = new_node(code);

      /* Set branches */
      code->tree[lastnode].branches[bit] = repeatnode;
      code->tree[repeatnode].branches[bit] = repeatnode;
      code->tree[repeatnode].branches[bit^1] = nextnode;
      lastnode = nextnode;

      bitpos++; /* terminating bit already handled, skip it */
    }
    else
    {
      /* Open branch check */
      if (code->tree[lastnode].branches[bit] < 0)
      {
        new_node(code);
        code->tree[lastnode].branches[bit] = code->numentries++;
      }

      /* set to branch */
      lastnode = code->tree[lastnode].branches[bit];
    }
  }

  if (!(code->tree[lastnode].branches[0] == -1
    && code->tree[lastnode].branches[1] == -2))
  {
    archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                      "Prefix found");
    return (ARCHIVE_FATAL);
  }

  /* Set leaf value */
  code->tree[lastnode].branches[0] = value;
  code->tree[lastnode].branches[1] = value;

  return (ARCHIVE_OK);
}

static int new_node(struct huffman_code *code)
{
  code->tree = (struct huffman_tree_node *)realloc(code->tree,
    (code->numentries + 1) * sizeof(*code->tree));
  code->tree[code->numentries].branches[0] = -1;
  code->tree[code->numentries].branches[1] = -2;
  return 1;
}

static int
make_table(struct huffman_code *code)
{
  if (code->maxlength < code->minlength || code->maxlength > 10)
    code->tablesize = 10;
  else
    code->tablesize = code->maxlength;

  code->table =
    (struct huffman_table_entry *)malloc(sizeof(*code->table)
    * (1 << code->tablesize));

  return make_table_recurse(code, 0, code->table, 0, code->tablesize);
}

static int
make_table_recurse(struct huffman_code *code, int node,
                   struct huffman_table_entry *table, int depth,
                   int maxdepth)
{
  int currtablesize, i, ret = (ARCHIVE_OK);

  currtablesize = 1 << (maxdepth - depth);

  if (!code->tree)
    return (ARCHIVE_FATAL);

  if (code->tree[node].branches[0] ==
    code->tree[node].branches[1])
  {
    for(i = 0; i < currtablesize; i++)
    {
      table[i].length = depth;
      table[i].value = code->tree[node].branches[0];
    }
  }
  else if (node < 0)
  {
    for(i = 0; i < currtablesize; i++)
      table[i].length = -1;
  }
  else
  {
    if(depth == maxdepth)
    {
      table[0].length = maxdepth + 1;
      table[0].value = node;
    }
    else
    {
      ret |= make_table_recurse(code, code->tree[node].branches[0], table,
                                depth + 1, maxdepth);
      ret |= make_table_recurse(code, code->tree[node].branches[1],
                         table + currtablesize / 2, depth + 1, maxdepth);
    }
  }
  return ret;
}

static off_t
expand(struct archive_read *a, off_t end)
{
  static const unsigned char lengthbases[] =
    {   0,   1,   2,   3,   4,   5,   6,
        7,   8,  10,  12,  14,  16,  20,
       24,  28,  32,  40,  48,  56,  64,
       80,  96, 112, 128, 160, 192, 224 };
  static const unsigned char lengthbits[] =
    { 0, 0, 0, 0, 0, 0, 0,
      0, 1, 1, 1, 1, 2, 2,
      2, 2, 3, 3, 3, 3, 4,
      4, 4, 4, 5, 5, 5, 5 };
  static const unsigned int offsetbases[] =
    {       0,       1,       2,       3,       4,       6,
            8,      12,      16,      24,      32,      48,
           64,      96,     128,     192,     256,     384,
          512,     768,    1024,    1536,    2048,    3072,
         4096,    6144,    8192,   12288,   16384,   24576,
        32768,   49152,   65536,   98304,  131072,  196608,
       262144,  327680,  393216,  458752,  524288,  589824,
       655360,  720896,  786432,  851968,  917504,  983040,
      1048576, 1310720, 1572864, 1835008, 2097152, 2359296,
      2621440, 2883584, 3145728, 3407872, 3670016, 3932160 };
  static const unsigned char offsetbits[] =
    {  0,  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,
       5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10,
      11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18 };
  static const unsigned char shortbases[] =
    { 0, 4, 8, 16, 32, 64, 128, 192 };
  static const unsigned char shortbits[] =
    { 2, 2, 3, 4, 5, 6, 6, 6 };

  int symbol, offs, len, offsindex, lensymbol, i, offssymbol, lowoffsetsymbol;
  unsigned char newfile;
  struct rar *rar = (struct rar *)(a->format->data);

  if (rar->filterstart < end)
    end = rar->filterstart;

  while (1)
  {
    if (rar->output_last_match &&
      lzss_position(&rar->lzss) + rar->lastlength <= end)
    {
      lzss_emit_match(rar, rar->lastoffset, rar->lastlength);
      rar->output_last_match = 0;
    }

    if(rar->output_last_match || lzss_position(&rar->lzss) >= end)
      return lzss_position(&rar->lzss);

    rar->bitoffset -= 8 * __archive_read_consume(a, (rar->bitoffset / 8));
    if ((symbol = read_next_symbol(a, &rar->maincode)) < 0)
      return -1;
    rar->output_last_match = 0;
    
    if (symbol < 256)
    {
      lzss_emit_literal(rar, symbol);
      continue;
    }
    else if (symbol == 256)
    {
      newfile = !read_bits(a, 1);

      if(newfile)
      {
        rar->start_new_block = 1;
        rar->start_new_table = read_bits(a, 1);
        return lzss_position(&rar->lzss);
      }
      else
      {
        parse_codes(a);
        continue;
      }
    }
    else if(symbol==257)
    {
      archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
                        "Parsing filters is unsupported.");
      return -1;
    }
    else if(symbol==258)
    {
      if(rar->lastlength == 0)
        continue;

      offs = rar->lastoffset;
      len = rar->lastlength;
    }
    else if (symbol <= 262)
    {
      offsindex = symbol - 259;
      offs = rar->oldoffset[offsindex];

      if ((lensymbol = read_next_symbol(a, &rar->lengthcode)) < 0)
        return -1;
      len = lengthbases[lensymbol] + 2;
      if (lengthbits[lensymbol] > 0)
        len += read_bits_32(a, lengthbits[lensymbol]);

      for (i = offsindex; i > 0; i--)
        rar->oldoffset[i] = rar->oldoffset[i-1];
      rar->oldoffset[0] = offs;
    }
    else if(symbol<=270)
    {
      offs = shortbases[symbol-263] + 1;
      if(shortbits[symbol-263] > 0)
        offs += read_bits_32(a, shortbits[symbol-263]);

      len = 2;

      for(i = 3; i > 0; i--)
        rar->oldoffset[i] = rar->oldoffset[i-1];
      rar->oldoffset[0] = offs;
    }
    else
    {
      len = lengthbases[symbol-271]+3;
      if(lengthbits[symbol-271] > 0)
        len += read_bits_32(a, lengthbits[symbol-271]);

      if ((offssymbol = read_next_symbol(a, &rar->offsetcode)) < 0)
        return -1;
      offs = offsetbases[offssymbol]+1;
      if(offsetbits[offssymbol] > 0)
      {
        if(offssymbol > 9)
        {
          if(offsetbits[offssymbol] > 4)
            offs += read_bits_32(a, offsetbits[offssymbol] - 4) << 4;

          if(rar->numlowoffsetrepeats > 0)
          {
            rar->numlowoffsetrepeats--;
            offs += rar->lastlowoffset;
          }
          else
          {
            if ((lowoffsetsymbol =
              read_next_symbol(a, &rar->lowoffsetcode)) < 0)
              return -1;
            if(lowoffsetsymbol == 16)
            {
              rar->numlowoffsetrepeats = 15;
              offs += rar->lastlowoffset;
            }
            else
            {
              offs += lowoffsetsymbol;
              rar->lastlowoffset = lowoffsetsymbol;
            }
          }
        }
        else
          offs += read_bits_32(a, offsetbits[offssymbol]);
      }

      if (offs >= 0x40000)
        len++;
      if (offs >= 0x2000)
        len++;

      for(i = 3; i > 0; i--)
        rar->oldoffset[i] = rar->oldoffset[i-1];
      rar->oldoffset[0] = offs;
    }

    rar->lastoffset = offs;
    rar->lastlength = len;
    rar->output_last_match = 1;
  }
}

static int
copy_from_lzss_window(struct archive_read *a, const void **buffer,
                        int64_t startpos, int length)
{
  int windowoffs, firstpart;
  struct rar *rar = (struct rar *)(a->format->data);

  if (!rar->unp_buffer)
  {
    if ((rar->unp_buffer = malloc(rar->dictionary_size)) == NULL)
    {
      archive_set_error(&a->archive, ENOMEM,
                        "Unable to allocate memory for uncompressed data.");
      return (ARCHIVE_FATAL);
    }
  }

  windowoffs = lzss_offset_for_position(&rar->lzss, startpos);
  if(windowoffs + length <= lzss_size(&rar->lzss))
    *buffer = &rar->lzss.window[windowoffs];
  else
  {
    firstpart = lzss_size(&rar->lzss) - windowoffs;
    memcpy(&rar->unp_buffer[0], &rar->lzss.window[windowoffs], firstpart);
    memcpy(&rar->unp_buffer[firstpart], &rar->lzss.window[0],
           length - firstpart);
    *buffer = rar->unp_buffer;
  }
  return (ARCHIVE_OK);
}
