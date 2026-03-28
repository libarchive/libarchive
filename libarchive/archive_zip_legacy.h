/*-
 * Copyright (c) 2026 Ray Chason
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

#ifndef ARCHIVE_ZIP_LEGACY_H_INCLUDED
#define ARCHIVE_ZIP_LEGACY_H_INCLUDED

#ifdef HAVE_LEGACY

#include "archive_read_private.h"

/* Implode */
struct implode_desc;

int implode_init(struct implode_desc **desc, struct archive_read *a,
	uint64_t cmp_size, unsigned zip_flags, size_t *cmp_bytes_read);
void implode_free(struct implode_desc **desc);
int implode_read(struct implode_desc *desc, uint8_t bytes[], size_t num_bytes,
	size_t *bytes_read, size_t *cmp_bytes_read);
const char *implode_error(int err);

/* Shrink */
struct shrink_desc;

int shrink_init(struct shrink_desc **desc, struct archive_read *a,
	uint64_t cmp_size, size_t *cmp_bytes_read);
void shrink_free(struct shrink_desc **desc);
int shrink_read(struct shrink_desc *desc, uint8_t bytes[], size_t num_bytes,
	size_t *bytes_read, size_t *cmp_bytes_read);
const char *shrink_error(int err);

/* Reduce */
struct reduce_desc;

int reduce_init(struct reduce_desc **desc, struct archive_read *a,
	uint64_t cmp_size, unsigned level, size_t *cmp_bytes_read);
void reduce_free(struct reduce_desc **desc);
int reduce_read(struct reduce_desc *desc, uint8_t bytes[], size_t num_bytes,
	size_t *bytes_read, size_t *cmp_bytes_read);
const char *reduce_error(int err);

#endif /* HAVE_LEGACY */

#endif /* !ARCHIVE_ZIP_LEGACY_H_INCLUDED */
