#ifndef ARCHIVE_BASE64_PRIVATE_H_INCLUDED
#define ARCHIVE_BASE64_PRIVATE_H_INCLUDED

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

char *base64_decode(const char *s, size_t len, size_t *out_len);
char *base64_encode(const char *s, size_t len);

#endif
