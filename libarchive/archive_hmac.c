/*-
* Copyright (c) 2014 Michihiro NAKAJIMA
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "archive.h"
#include "archive_hmac_private.h"

#ifdef __APPLE__

static int
__hmac_sha1_init(archive_hmac_sha1_ctx *ctx, const uint8_t *key, size_t key_len)
{
	CCHmacInit(ctx, kCCHmacAlgSHA1, key, key_len);
	return 0;
}

static void
__hmac_sha1_update(archive_hmac_sha1_ctx *ctx, const uint8_t *data,
    size_t data_len)
{
	CCHmacUpdate(ctx, data, data_len);
}

static void
__hmac_sha1_final(archive_hmac_sha1_ctx *ctx, uint8_t *out, size_t *out_len)
{
	CCHmacFinal(ctx, out);
	*out_len = 20;
}

static void
__hmac_sha1_cleanup(archive_hmac_sha1_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
}

#elif defined(HAVE_LIBNETTLE)

static int
__hmac_sha1_init(archive_hmac_sha1_ctx *ctx, const uint8_t *key, size_t key_len)
{
	hmac_sha1_set_key(ctx, key_len, key);
	return 0;
}

static void
__hmac_sha1_update(archive_hmac_sha1_ctx *ctx, const uint8_t *data,
    size_t data_len)
{
	hmac_sha1_update(ctx, data_len, data);
}

static void
__hmac_sha1_final(archive_hmac_sha1_ctx *ctx, uint8_t *out, size_t *out_len)
{
	hmac_sha1_digest(ctx, (unsigned)*out_len, out);
}

static void
__hmac_sha1_cleanup(archive_hmac_sha1_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
}

#elif defined(HAVE_LIBCRYPTO)

static int
__hmac_sha1_init(archive_hmac_sha1_ctx *ctx, const uint8_t *key, size_t key_len)
{
	HMAC_CTX_init(ctx);
	HMAC_Init(ctx, key, key_len, EVP_sha1());
	return 0;
}

static void
__hmac_sha1_update(archive_hmac_sha1_ctx *ctx, const uint8_t *data,
    size_t data_len)
{
	HMAC_Update(ctx, data, data_len);
}

static void
__hmac_sha1_final(archive_hmac_sha1_ctx *ctx, uint8_t *out, size_t *out_len)
{
	unsigned int len = (unsigned int)*out_len;
	HMAC_Final(ctx, out, &len);
	*out_len = len;
}

static void
__hmac_sha1_cleanup(archive_hmac_sha1_ctx *ctx)
{
	HMAC_CTX_cleanup(ctx);
	memset(ctx, 0, sizeof(*ctx));
}

#else

/* Stub */
static int
__hmac_sha1_init(archive_hmac_sha1_ctx *ctx, const uint8_t *key, size_t key_len)
{
	(void)ctx;/* UNUSED */
	(void)key;/* UNUSED */
	(void)key_len;/* UNUSED */
	return -1;
}

static void
__hmac_sha1_update(archive_hmac_sha1_ctx *ctx, const uint8_t *data,
    size_t data_len)
{
	(void)ctx;/* UNUSED */
	(void)data;/* UNUSED */
	(void)data_len;/* UNUSED */
}

static void
__hmac_sha1_final(archive_hmac_sha1_ctx *ctx, uint8_t *out)
{
	(void)ctx;/* UNUSED */
	(void)out;/* UNUSED */
}

static void
__hmac_sha1_cleanup(archive_hmac_sha1_ctx *ctx)
{
	(void)ctx;/* UNUSED */
}

#endif

const struct archive_hmac __archive_hmac = {
	&__hmac_sha1_init,
	&__hmac_sha1_update,
	&__hmac_sha1_final,
	&__hmac_sha1_cleanup,
};
