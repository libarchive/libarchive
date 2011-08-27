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

#include "archive.h"
#include "archive_crypto_private.h"

/* MD5 implementations */
#if defined(ARCHIVE_CRYPTO_MD5_LIBC)

static int
__archive_libc_md5init(archive_md5_ctx *ctx)
{
  MD5Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libc_md5update(archive_md5_ctx *ctx, const void *indata,
    size_t insize)
{
  MD5Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libc_md5final(archive_md5_ctx *ctx, void *md)
{
  MD5Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_MD5_LIBSYSTEM)

static int
__archive_libsystem_md5init(archive_md5_ctx *ctx)
{
  CC_MD5_Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libsystem_md5update(archive_md5_ctx *ctx, const void *indata,
    size_t insize)
{
  CC_MD5_Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libsystem_md5final(archive_md5_ctx *ctx, void *md)
{
  CC_MD5_Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_MD5_OPENSSL)

static int
__archive_openssl_md5init(archive_md5_ctx *ctx)
{
  EVP_DigestInit(ctx, EVP_md5());
  return (ARCHIVE_OK);
}

static int
__archive_openssl_md5update(archive_md5_ctx *ctx, const void *indata,
    size_t insize)
{
  EVP_DigestUpdate(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_openssl_md5final(archive_md5_ctx *ctx, void *md)
{
  /* HACK: archive_write_set_format_xar.c is finalizing empty contexts, so
   * this is meant to cope with that. Real fix is probably to fix
   * archive_write_set_format_xar.c
   */
  if (ctx->digest)
    EVP_DigestFinal(ctx, md, NULL);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_MD5_WIN)

static int
__archive_windowsapi_md5init(archive_md5_ctx *ctx)
{
  __la_hash_Init(ctx, CALG_MD5);
  return (ARCHIVE_OK);
}

static int
__archive_windowsapi_md5update(archive_md5_ctx *ctx, const void *indata,
    size_t insize)
{
  __la_hash_Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_windowsapi_md5final(archive_md5_ctx *ctx, void *md)
{
  __la_hash_Final(md, 16, ctx);
  return (ARCHIVE_OK);
}

#endif

/* RIPEMD160 implementations */
#if defined(ARCHIVE_CRYPTO_RMD160_LIBC)

static int
__archive_libc_ripemd160init(archive_rmd160_ctx *ctx)
{
  RMD160Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libc_ripemd160update(archive_rmd160_ctx *ctx, const void *indata,
    size_t insize)
{
  RMD160Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libc_ripemd160final(archive_rmd160_ctx *ctx, void *md)
{
  RMD160Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_RMD160_OPENSSL)

static int
__archive_openssl_ripemd160init(archive_rmd160_ctx *ctx)
{
  EVP_DigestInit(ctx, EVP_ripemd160());
  return (ARCHIVE_OK);
}

static int
__archive_openssl_ripemd160update(archive_rmd160_ctx *ctx, const void *indata,
    size_t insize)
{
  EVP_DigestUpdate(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_openssl_ripemd160final(archive_rmd160_ctx *ctx, void *md)
{
  EVP_DigestFinal(ctx, md, NULL);
  return (ARCHIVE_OK);
}

#endif

/* SHA1 implementations */
#if defined(ARCHIVE_CRYPTO_SHA1_LIBC)

static int
__archive_libc_sha1init(archive_sha1_ctx *ctx)
{
  SHA1Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libc_sha1update(archive_sha1_ctx *ctx, const void *indata,
    size_t insize)
{
  SHA1Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libc_sha1final(archive_sha1_ctx *ctx, void *md)
{
  SHA1Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA1_LIBSYSTEM)

static int
__archive_libsystem_sha1init(archive_sha1_ctx *ctx)
{
  CC_SHA1_Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libsystem_sha1update(archive_sha1_ctx *ctx, const void *indata,
    size_t insize)
{
  CC_SHA1_Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libsystem_sha1final(archive_sha1_ctx *ctx, void *md)
{
  CC_SHA1_Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA1_OPENSSL)

static int
__archive_openssl_sha1init(archive_sha1_ctx *ctx)
{
  EVP_DigestInit(ctx, EVP_sha1());
  return (ARCHIVE_OK);
}

static int
__archive_openssl_sha1update(archive_sha1_ctx *ctx, const void *indata,
    size_t insize)
{
  EVP_DigestUpdate(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_openssl_sha1final(archive_sha1_ctx *ctx, void *md)
{
  /* HACK: archive_write_set_format_xar.c is finalizing empty contexts, so
   * this is meant to cope with that. Real fix is probably to fix
   * archive_write_set_format_xar.c
   */
  if (ctx->digest)
    EVP_DigestFinal(ctx, md, NULL);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA1_WIN)

static int
__archive_windowsapi_sha1init(archive_sha1_ctx *ctx)
{
  __la_hash_Init(ctx, CALG_SHA1);
  return (ARCHIVE_OK);
}

static int
__archive_windowsapi_sha1update(archive_sha1_ctx *ctx, const void *indata,
    size_t insize)
{
  __la_hash_Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_windowsapi_sha1final(archive_sha1_ctx *ctx, void *md)
{
  __la_hash_Final(md, 20, ctx);
  return (ARCHIVE_OK);
}

#endif

/* SHA256 implementations */
#if defined(ARCHIVE_CRYPTO_SHA256_LIBC)

static int
__archive_libc_sha256init(archive_sha256_ctx *ctx)
{
  SHA256_Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libc_sha256update(archive_sha256_ctx *ctx, const void *indata,
    size_t insize)
{
  SHA256_Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libc_sha256final(archive_sha256_ctx *ctx, void *md)
{
  SHA256_Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA256_LIBC2)

static int
__archive_libc2_sha256init(archive_sha256_ctx *ctx)
{
  SHA256Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libc2_sha256update(archive_sha256_ctx *ctx, const void *indata,
    size_t insize)
{
  SHA256Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libc2_sha256final(archive_sha256_ctx *ctx, void *md)
{
  SHA256Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA256_LIBC3)

static int
__archive_libc3_sha256init(archive_sha256_ctx *ctx)
{
  SHA256Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libc3_sha256update(archive_sha256_ctx *ctx, const void *indata,
    size_t insize)
{
  SHA256Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libc3_sha256final(archive_sha256_ctx *ctx, void *md)
{
  SHA256Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA256_LIBSYSTEM)

static int
__archive_libsystem_sha256init(archive_sha256_ctx *ctx)
{
  CC_SHA256_Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libsystem_sha256update(archive_sha256_ctx *ctx, const void *indata,
    size_t insize)
{
  CC_SHA256_Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libsystem_sha256final(archive_sha256_ctx *ctx, void *md)
{
  CC_SHA256_Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA256_OPENSSL)

static int
__archive_openssl_sha256init(archive_sha256_ctx *ctx)
{
  EVP_DigestInit(ctx, EVP_sha256());
  return (ARCHIVE_OK);
}

static int
__archive_openssl_sha256update(archive_sha256_ctx *ctx, const void *indata,
    size_t insize)
{
  EVP_DigestUpdate(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_openssl_sha256final(archive_sha256_ctx *ctx, void *md)
{
  EVP_DigestFinal(ctx, md, NULL);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA256_WIN)

static int
__archive_windowsapi_sha256init(archive_sha256_ctx *ctx)
{
  __la_hash_Init(ctx, CALG_SHA256);
  return (ARCHIVE_OK);
}

static int
__archive_windowsapi_sha256update(archive_sha256_ctx *ctx, const void *indata,
    size_t insize)
{
  __la_hash_Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_windowsapi_sha256final(archive_sha256_ctx *ctx, void *md)
{
  __la_hash_Final(md, 32, ctx);
  return (ARCHIVE_OK);
}

#endif

/* SHA384 implementations */
#if defined(ARCHIVE_CRYPTO_SHA384_LIBC)

static int
__archive_libc_sha384init(archive_sha384_ctx *ctx)
{
  SHA384_Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libc_sha384update(archive_sha384_ctx *ctx, const void *indata,
    size_t insize)
{
  SHA384_Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libc_sha384final(archive_sha384_ctx *ctx, void *md)
{
  SHA384_Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA384_LIBC2)

static int
__archive_libc2_sha384init(archive_sha384_ctx *ctx)
{
  SHA384Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libc2_sha384update(archive_sha384_ctx *ctx, const void *indata,
    size_t insize)
{
  SHA384Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libc2_sha384final(archive_sha384_ctx *ctx, void *md)
{
  SHA384Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA384_LIBC3)

static int
__archive_libc3_sha384init(archive_sha384_ctx *ctx)
{
  SHA384Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libc3_sha384update(archive_sha384_ctx *ctx, const void *indata,
    size_t insize)
{
  SHA384Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libc3_sha384final(archive_sha384_ctx *ctx, void *md)
{
  SHA384Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA384_LIBSYSTEM)

static int
__archive_libsystem_sha384init(archive_sha384_ctx *ctx)
{
  CC_SHA384_Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libsystem_sha384update(archive_sha384_ctx *ctx, const void *indata,
    size_t insize)
{
  CC_SHA384_Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libsystem_sha384final(archive_sha384_ctx *ctx, void *md)
{
  CC_SHA384_Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA384_OPENSSL)

static int
__archive_openssl_sha384init(archive_sha384_ctx *ctx)
{
  EVP_DigestInit(ctx, EVP_sha384());
  return (ARCHIVE_OK);
}

static int
__archive_openssl_sha384update(archive_sha384_ctx *ctx, const void *indata,
    size_t insize)
{
  EVP_DigestUpdate(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_openssl_sha384final(archive_sha384_ctx *ctx, void *md)
{
  EVP_DigestFinal(ctx, md, NULL);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA384_WIN)

static int
__archive_windowsapi_sha384init(archive_sha384_ctx *ctx)
{
  __la_hash_Init(ctx, CALG_SHA384);
  return (ARCHIVE_OK);
}

static int
__archive_windowsapi_sha384update(archive_sha384_ctx *ctx, const void *indata,
    size_t insize)
{
  __la_hash_Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_windowsapi_sha384final(archive_sha384_ctx *ctx, void *md)
{
  __la_hash_Final(md, 48, ctx);
  return (ARCHIVE_OK);
}

#endif

/* SHA512 implementations */
#if defined(ARCHIVE_CRYPTO_SHA512_LIBC)

static int
__archive_libc_sha512init(archive_sha512_ctx *ctx)
{
  SHA512_Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libc_sha512update(archive_sha512_ctx *ctx, const void *indata,
    size_t insize)
{
  SHA512_Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libc_sha512final(archive_sha512_ctx *ctx, void *md)
{
  SHA512_Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA512_LIBC2)

static int
__archive_libc2_sha512init(archive_sha512_ctx *ctx)
{
  SHA512Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libc2_sha512update(archive_sha512_ctx *ctx, const void *indata,
    size_t insize)
{
  SHA512Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libc2_sha512final(archive_sha512_ctx *ctx, void *md)
{
  SHA512Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA512_LIBC3)

static int
__archive_libc3_sha512init(archive_sha512_ctx *ctx)
{
  SHA512Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libc3_sha512update(archive_sha512_ctx *ctx, const void *indata,
    size_t insize)
{
  SHA512Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libc3_sha512final(archive_sha512_ctx *ctx, void *md)
{
  SHA512Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA512_LIBSYSTEM)

static int
__archive_libsystem_sha512init(archive_sha512_ctx *ctx)
{
  CC_SHA512_Init(ctx);
  return (ARCHIVE_OK);
}

static int
__archive_libsystem_sha512update(archive_sha512_ctx *ctx, const void *indata,
    size_t insize)
{
  CC_SHA512_Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_libsystem_sha512final(archive_sha512_ctx *ctx, void *md)
{
  CC_SHA512_Final(md, ctx);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA512_OPENSSL)

static int
__archive_openssl_sha512init(archive_sha512_ctx *ctx)
{
  EVP_DigestInit(ctx, EVP_sha512());
  return (ARCHIVE_OK);
}

static int
__archive_openssl_sha512update(archive_sha512_ctx *ctx, const void *indata,
    size_t insize)
{
  EVP_DigestUpdate(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_openssl_sha512final(archive_sha512_ctx *ctx, void *md)
{
  EVP_DigestFinal(ctx, md, NULL);
  return (ARCHIVE_OK);
}

#elif defined(ARCHIVE_CRYPTO_SHA512_WIN)

static int
__archive_windowsapi_sha512init(archive_sha512_ctx *ctx)
{
  __la_hash_Init(ctx, CALG_SHA512);
  return (ARCHIVE_OK);
}

static int
__archive_windowsapi_sha512update(archive_sha512_ctx *ctx, const void *indata,
    size_t insize)
{
  __la_hash_Update(ctx, indata, insize);
  return (ARCHIVE_OK);
}

static int
__archive_windowsapi_sha512final(archive_sha512_ctx *ctx, void *md)
{
  __la_hash_Final(md, 64, ctx);
  return (ARCHIVE_OK);
}

#endif

/* NOTE: Crypto functions are set based on availability and by the following
 * order of preference.
 * 1. libc
 * 2. libc2
 * 3. libc3
 * 4. libSystem
 * 5. OpenSSL
 * 6. Windows API
 */
const struct archive_crypto __archive_crypto =
{
/* MD5 */
#if defined(ARCHIVE_CRYPTO_MD5_LIBC)
  &__archive_libc_md5init,
  &__archive_libc_md5update,
  &__archive_libc_md5final,
#elif defined(ARCHIVE_CRYPTO_MD5_LIBSYSTEM)
  &__archive_libsystem_md5init,
  &__archive_libsystem_md5update,
  &__archive_libsystem_md5final,
#elif defined(ARCHIVE_CRYPTO_MD5_OPENSSL)
  &__archive_openssl_md5init,
  &__archive_openssl_md5update,
  &__archive_openssl_md5final,
#elif defined(ARCHIVE_CRYPTO_MD5_WIN)
  &__archive_windowsapi_md5init,
  &__archive_windowsapi_md5update,
  &__archive_windowsapi_md5final,
#elif !defined(ARCHIVE_MD5_COMPILE_TEST)
  NULL,
  NULL,
  NULL,
#endif

/* RIPEMD160 */
#if defined(ARCHIVE_CRYPTO_RMD160_LIBC)
  &__archive_libc_ripemd160init,
  &__archive_libc_ripemd160update,
  &__archive_libc_ripemd160final,
#elif defined(ARCHIVE_CRYPTO_RMD160_OPENSSL)
  &__archive_openssl_ripemd160init,
  &__archive_openssl_ripemd160update,
  &__archive_openssl_ripemd160final,
#elif !defined(ARCHIVE_RMD160_COMPILE_TEST)
  NULL,
  NULL,
  NULL,
#endif

/* SHA1 */
#if defined(ARCHIVE_CRYPTO_SHA1_LIBC)
  &__archive_libc_sha1init,
  &__archive_libc_sha1update,
  &__archive_libc_sha1final,
#elif defined(ARCHIVE_CRYPTO_SHA1_LIBSYSTEM)
  &__archive_libsystem_sha1init,
  &__archive_libsystem_sha1update,
  &__archive_libsystem_sha1final,
#elif defined(ARCHIVE_CRYPTO_SHA1_OPENSSL)
  &__archive_openssl_sha1init,
  &__archive_openssl_sha1update,
  &__archive_openssl_sha1final,
#elif defined(ARCHIVE_CRYPTO_SHA1_WIN)
  &__archive_windowsapi_sha1init,
  &__archive_windowsapi_sha1update,
  &__archive_windowsapi_sha1final,
#elif !defined(ARCHIVE_SHA1_COMPILE_TEST)
  NULL,
  NULL,
  NULL,
#endif

/* SHA256 */
#if defined(ARCHIVE_CRYPTO_SHA256_LIBC)
  &__archive_libc_sha256init,
  &__archive_libc_sha256update,
  &__archive_libc_sha256final,
#elif defined(ARCHIVE_CRYPTO_SHA256_LIBC2)
  &__archive_libc2_sha256init,
  &__archive_libc2_sha256update,
  &__archive_libc2_sha256final,
#elif defined(ARCHIVE_CRYPTO_SHA256_LIBC3)
  &__archive_libc3_sha256init,
  &__archive_libc3_sha256update,
  &__archive_libc3_sha256final,
#elif defined(ARCHIVE_CRYPTO_SHA256_LIBSYSTEM)
  &__archive_libsystem_sha256init,
  &__archive_libsystem_sha256update,
  &__archive_libsystem_sha256final,
#elif defined(ARCHIVE_CRYPTO_SHA256_OPENSSL)
  &__archive_openssl_sha256init,
  &__archive_openssl_sha256update,
  &__archive_openssl_sha256final,
#elif defined(ARCHIVE_CRYPTO_SHA256_WIN)
  &__archive_windowsapi_sha256init,
  &__archive_windowsapi_sha256update,
  &__archive_windowsapi_sha256final,
#elif !defined(ARCHIVE_SHA256_COMPILE_TEST)
  NULL,
  NULL,
  NULL,
#endif

/* SHA384 */
#if defined(ARCHIVE_CRYPTO_SHA384_LIBC)
  &__archive_libc_sha384init,
  &__archive_libc_sha384update,
  &__archive_libc_sha384final,
#elif defined(ARCHIVE_CRYPTO_SHA384_LIBC2)
  &__archive_libc2_sha384init,
  &__archive_libc2_sha384update,
  &__archive_libc2_sha384final,
#elif defined(ARCHIVE_CRYPTO_SHA384_LIBC3)
  &__archive_libc3_sha384init,
  &__archive_libc3_sha384update,
  &__archive_libc3_sha384final,
#elif defined(ARCHIVE_CRYPTO_SHA384_LIBSYSTEM)
  &__archive_libsystem_sha384init,
  &__archive_libsystem_sha384update,
  &__archive_libsystem_sha384final,
#elif defined(ARCHIVE_CRYPTO_SHA384_OPENSSL)
  &__archive_openssl_sha384init,
  &__archive_openssl_sha384update,
  &__archive_openssl_sha384final,
#elif defined(ARCHIVE_CRYPTO_SHA384_WIN)
  &__archive_windowsapi_sha384init,
  &__archive_windowsapi_sha384update,
  &__archive_windowsapi_sha384final,
#elif !defined(ARCHIVE_SHA384_COMPILE_TEST)
  NULL,
  NULL,
  NULL,
#endif

/* SHA512 */
#if defined(ARCHIVE_CRYPTO_SHA512_LIBC)
  &__archive_libc_sha512init,
  &__archive_libc_sha512update,
  &__archive_libc_sha512final
#elif defined(ARCHIVE_CRYPTO_SHA512_LIBC2)
  &__archive_libc2_sha512init,
  &__archive_libc2_sha512update,
  &__archive_libc2_sha512final
#elif defined(ARCHIVE_CRYPTO_SHA512_LIBC3)
  &__archive_libc3_sha512init,
  &__archive_libc3_sha512update,
  &__archive_libc3_sha512final
#elif defined(ARCHIVE_CRYPTO_SHA512_LIBSYSTEM)
  &__archive_libsystem_sha512init,
  &__archive_libsystem_sha512update,
  &__archive_libsystem_sha512final
#elif defined(ARCHIVE_CRYPTO_SHA512_OPENSSL)
  &__archive_openssl_sha512init,
  &__archive_openssl_sha512update,
  &__archive_openssl_sha512final
#elif defined(ARCHIVE_CRYPTO_SHA512_WIN)
  &__archive_windowsapi_sha512init,
  &__archive_windowsapi_sha512update,
  &__archive_windowsapi_sha512final
#elif !defined(ARCHIVE_SHA512_COMPILE_TEST)
  NULL,
  NULL,
  NULL
#endif
};
