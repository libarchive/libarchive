/* archive_zip_legacy.h */

#ifndef ARCHIVE_ZIP_LEGACY_H_INCLUDED
#define ARCHIVE_ZIP_LEGACY_H_INCLUDED

// #ifdef HAVE_LEGACY

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

// #endif /* HAVE_LEGACY */

#endif /* !ARCHIVE_ZIP_LEGACY_H_INCLUDED */
