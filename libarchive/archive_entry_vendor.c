#include "archive_platform.h"

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_entry_private.h"

/*
 * vendor attribute handling
 */

void
archive_entry_vendor_clear(struct archive_entry *entry)
{
	struct ae_vendor	*xp;

	while (entry->vendor_head != NULL) {
		xp = entry->vendor_head->next;
		free(entry->vendor_head->name);
		free(entry->vendor_head->value);
		free(entry->vendor_head);
		entry->vendor_head = xp;
	}

	entry->vendor_head = NULL;
}

void
archive_entry_vendor_add_entry(struct archive_entry *entry,
	const char *name, const void *value, size_t size)
{
	struct ae_vendor	*xp;

	if ((xp = (struct ae_vendor *)malloc(sizeof(struct ae_vendor))) == NULL)
		__archive_errx(1, "Out of memory");

	if ((xp->name = strdup(name)) == NULL)
		__archive_errx(1, "Out of memory");

	if ((xp->value = malloc(size)) != NULL) {
		memcpy(xp->value, value, size);
		xp->size = size;
	} else
		xp->size = 0;

	xp->next = entry->vendor_head;
	entry->vendor_head = xp;
}


/*
 * returns number of the extended attribute entries
 */
int
archive_entry_vendor_count(struct archive_entry *entry)
{
	struct ae_vendor *xp;
	int count = 0;

	for (xp = entry->vendor_head; xp != NULL; xp = xp->next)
		count++;

	return count;
}

int
archive_entry_vendor_reset(struct archive_entry * entry)
{
	entry->vendor_p = entry->vendor_head;

	return archive_entry_vendor_count(entry);
}

int
archive_entry_vendor_next(struct archive_entry * entry,
	const char **name, const void **value, size_t *size)
{
	if (entry->vendor_p) {
		*name = entry->vendor_p->name;
		*value = entry->vendor_p->value;
		*size = entry->vendor_p->size;

		entry->vendor_p = entry->vendor_p->next;

		return (ARCHIVE_OK);
	} else {
		*name = NULL;
		*value = NULL;
		*size = (size_t)0;
		return (ARCHIVE_WARN);
	}
}

int
archive_entry_vendor_valid_key(const char *name)
{
	/* Vendor specific PAX attributes are always in all caps up to the first
	 * period. This function will ignore any attributes that do not follow these
	 * rules
	 */
	char c;
	size_t s;
	for (c = name[0], s = 0; c != '\0'; c = name[++s]) {
		// Stop checking after the first period
		if (c == '.') {
			// It is an error if the first char is a period
			if (s == 0) {
				return 0;
			}
			break;
		}
		if (!(c >= 'A' && c <= 'Z')) {
			// found a non-upper case character
			return 0;
		}
	}
	return 1;
}
