/*-
 * Copyright (c) 2013 Priit Järv
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
#include "test.h"

#if defined(__FreeBSD__) && __FreeBSD__ > 5
#include <sys/mac.h>

struct mac_testdata {
	const char *polname;
	const char *trylabel;
};

static int
create_labeled_file(char *filename, const char *labeltext)
{
	mac_t mac = NULL;
	int fd = -1, err;
	int ret = 0;

	fd = open(filename, O_RDWR | O_CREAT, 0777);
	failure("Could not create test file?!");
	if (!assert(fd >= 0))
		goto done;

	failure("Couldn't convert MAC label");
	if (!assert(mac_from_text(&mac, labeltext) == 0))
		goto done;

	errno = 0;
	err = mac_set_fd(fd, mac);
	failure("mac_set_fd(): errno=%d (%s)", errno, strerror(errno));
	if (assert(err == 0))
		ret = 1; /* success */

done:
	if (mac) free(mac);
	if (fd >= 0) close(fd);
	return ret;
}

static int
get_file_maclabel(char *filename, char *buf, size_t buflen)
{
	mac_t mac = NULL;
	char *labeltext = NULL;
	int ret = 0;

	if (mac_prepare_file_label(&mac))
		return 0;
	if (mac_get_file(filename, mac))
		goto done;

	if (mac_to_text(mac, &labeltext) == 0) {
		strncpy(buf, labeltext, buflen - 1);
		buf[buflen - 1] = '\0';
		ret = 1;
	}
done:
	if (mac) free(mac);
	if (labeltext) free(labeltext);
	return ret;
}

static void
test_macpolicy(const char *polname, const char *trylabel)
{
	char fn[64];
	char mac1[512], mac2[512];
	struct archive *ar, *aw;
	struct archive_entry *ae;
	int i, found;

	/* Create a file with the given label */
	strcpy(fn, "test_");
	strncat(fn, polname, 54);
	fn[59] = '\0';
	if (!create_labeled_file(fn, trylabel))
		return;

	/* Get the actual label. Note that it may be different than
	 * our original argument if multiple policies are loaded.
	 */
	failure("Cannot get MAC label from file");
	if (!assert(get_file_maclabel(fn, mac1, 512)))
		return;

	/* Read the file data into archive */
	assert((ar = archive_read_disk_new()) != NULL);
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_copy_pathname(ae, fn);

	failure("Reading disk entry failed.");
	assertEqualIntA(ar, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(ar, ae, -1, NULL));

	/* Check that there is a matching xattr */
	i = archive_entry_xattr_reset(ae);
	found = 0;
	while (i--) {
		const char *name;
		const void *value;
		size_t size;
		archive_entry_xattr_next(ae, &name, &value, &size);
		if (name != NULL) {
			if (strncmp(name, "system.mac", 10) == 0) {
				if (strncmp(value, mac1,
				    (size > 512 ? 512 : size)) == 0) {
					found = 1;
					break;
				}
			}
		}
	}
	failure("No matching label in archive");
	assert(found);

	/* Write the file back to disk. Note that the file contained
	 * no data so we're only concerned with the header.
	 */
	assert((aw = archive_write_disk_new()) != NULL);
	archive_write_disk_set_options(aw, ARCHIVE_EXTRACT_XATTR);

	strncat(fn, "_out", 4); /* New name for extraction */
	fn[63] = '\0';
	archive_entry_copy_pathname(ae, fn);
	assertEqualIntA(aw, ARCHIVE_OK, archive_write_header(aw, ae));
	assertEqualIntA(aw, ARCHIVE_OK, archive_write_finish_entry(aw));

	/* Clean up */
	assertEqualInt(ARCHIVE_OK, archive_write_free(aw));
	assertEqualInt(ARCHIVE_OK, archive_read_free(ar));
	archive_entry_free(ae);

	/* Finally, verify the label of the created file */
	failure("Cannot get MAC label from file");
	if (!assert(get_file_maclabel(fn, mac2, 512)))
		return;
	failure("Original and extracted MAC labels do not match");
	assert(strncmp(mac1, mac2, 512) == 0);
}
#endif /* defined(__FreeBSD__) && __FreeBSD__ > 5 */


/*
 * Test reading and writing MAC labels. This test is FreeBSD-specific.
 * NOTE: to be able to test anything at all, these conditions should be met:
 * - at least one labeling policy module is loaded
 * - the filesystem is configured as multilabel
 * - the user process itself has a permissive enough label (the specifics
 *   depend on the policy being tested).
 * Initial version; covers only the bare minimum.
 */

DEFINE_TEST(test_maclabel_freebsd)
{
#if !defined(__FreeBSD__)
	skipping("FreeBSD-specific MAC label test");
#elif __FreeBSD__ < 5
	skipping("MAC labels supported only on FreeBSD 5.0 and later");
#else
	/* Use these labels for testing. They are chosen so that
	 * they would be different from the default, however the
	 * correct way would be to get the label for the current process
	 * and use that to choose the testing value. This would require
	 * a large amount of text-processing, as the API does not
	 * provide a way to get numeric values for grades.
	 */
	struct mac_testdata poltests[] = {
		{ "biba", "biba/10" },
		{ "lomac", "lomac/10" },
		{ "mls", "mls/10" },
		{ NULL, NULL }
	};
	int pretest_done;
	int i;
	int err, fd;

	pretest_done = 0;
	i = 0;
	while (poltests[i].polname) {
		if (!mac_is_present(poltests[i].polname)) {
			skipping("MAC policy `%s' is not present",
			    poltests[i].polname);
			i++;
			continue;
		}
		if(!pretest_done) {
			mac_t mac;
			/* Check if the filesystem supports MAC labels. */
			fd = open("pretest", O_RDWR | O_CREAT, 0777);
			failure("Could not create test file?!");
			if (!assert(fd >= 0))
				return;

			failure("Couldn't convert MAC label");
			if (!assert(mac_from_text(&mac,
			    poltests[i].trylabel) == 0))
				return;

			errno = 0;
			err = mac_set_fd(fd, mac);
			if (err && errno == EOPNOTSUPP) {
				close(fd);
				free(mac);
				skipping("MAC labels are not supported on"\
				    "this filesystem");
				return;
			}
			failure("mac_set_fd(): errno=%d (%s)",
			    errno, strerror(errno));
			assertEqualInt(0, err);
			free(mac);
			close(fd);

			pretest_done = 1;
		}
		test_macpolicy(poltests[i].polname, poltests[i].trylabel);
		i++;
	}
#endif
}
