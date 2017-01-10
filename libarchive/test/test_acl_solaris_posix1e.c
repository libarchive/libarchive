/*-
 * Copyright (c) 2003-2008 Tim Kientzle
 * Copyright (c) 2017 Martin Matuska
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

#if defined(__sun__)
#include <sys/acl.h>

static struct archive_test_acl_t acls2[] = {
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_EXECUTE | ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_USER, 77, "user77" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, 0,
	  ARCHIVE_ENTRY_ACL_USER, 78, "user78" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, 0007,
	  ARCHIVE_ENTRY_ACL_GROUP, 78, "group78" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
	  ARCHIVE_ENTRY_ACL_WRITE | ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_OTHER, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
	  ARCHIVE_ENTRY_ACL_WRITE | ARCHIVE_ENTRY_ACL_READ | ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_MASK, -1, "" },
};

static int
acl_entry_get_perm(aclent_t *aclent) {
	int permset = 0;

	if (aclent->a_perm & 1)
		permset |= ARCHIVE_ENTRY_ACL_EXECUTE;
	if (aclent->a_perm & 2)
		permset |= ARCHIVE_ENTRY_ACL_WRITE;
	if (aclent->a_perm & 4)
		permset |= ARCHIVE_ENTRY_ACL_READ;

	return permset;
}

static int
acl_match(aclent_t *aclent, struct archive_test_acl_t *myacl)
{
	if (myacl->permset != acl_entry_get_perm(aclent))
		return (0);

	switch (aclent->a_type) {
	case DEF_USER_OBJ:
	case USER_OBJ:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_USER_OBJ) return (0);
		break;
	case DEF_USER:
	case USER:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_USER)
			return (0);
		if ((uid_t)myacl->qual != aclent->a_id)
			return (0);
		break;
	case DEF_GROUP_OBJ:
	case GROUP_OBJ:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_GROUP_OBJ) return (0);
		break;
	case DEF_GROUP:
	case GROUP:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_GROUP)
			return (0);
		if ((gid_t)myacl->qual != aclent->a_id)
			return (0);
		break;
	case DEF_CLASS_OBJ:
	case CLASS_OBJ:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_MASK) return (0);
		break;
	case DEF_OTHER_OBJ:
	case OTHER_OBJ:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_OTHER) return (0);
		break;
	}
	return (1);
}

static void
compare_acls(acl_t *acl, struct archive_test_acl_t *myacls, int n)
{
	int *marker;
	int matched;
	int e, i;
	aclent_t *aclent;

	/* Count ACL entries in myacls array and allocate an indirect array. */
	marker = malloc(sizeof(marker[0]) * n);
	if (marker == NULL)
		return;
	for (i = 0; i < n; i++)
		marker[i] = i;

	/*
	 * Iterate over acls in system acl object, try to match each
	 * one with an item in the myacls array.
	 */
	for(e = 0; e < acl->acl_cnt; e++) {
		aclent = &((aclent_t *)acl->acl_aclp)[e];

		/* Search for a matching entry (tag and qualifier) */
		for (i = 0, matched = 0; i < n && !matched; i++) {
			if (acl_match(aclent, &myacls[marker[i]])) {
				/* We found a match; remove it. */
				marker[i] = marker[n - 1];
				n--;
				matched = 1;
			}
		}

		/* TODO: Print out more details in this case. */
		failure("ACL entry on file that shouldn't be there");
		assert(matched == 1);
	}

	/* Dump entries in the myacls array that weren't in the system acl. */
	for (i = 0; i < n; ++i) {
		failure(" ACL entry missing from file: "
		    "type=%#010x,permset=%#010x,tag=%d,qual=%d,name=``%s''\n",
		    myacls[marker[i]].type, myacls[marker[i]].permset,
		    myacls[marker[i]].tag, myacls[marker[i]].qual,
		    myacls[marker[i]].name);
		assert(0); /* Record this as a failure. */
	}
	free(marker);
}

#endif


/*
 * Verify ACL restore-to-disk.  This test is Solaris-specific.
 */

DEFINE_TEST(test_acl_solaris_posix1e_restore)
{
#if !defined(__sun__)
	skipping("Solaris-specific ACL restore test");
#else
	struct stat st;
	struct archive *a;
	struct archive_entry *ae;
	int n, fd;
	acl_t *acl;

	/*
	 * First, do a quick manual set/read of ACL data to
	 * verify that the local filesystem does support ACLs.
	 * If it doesn't, we'll simply skip the remaining tests.
	 */
	acl = NULL;

	/* Create a test file and try to set an ACL on it. */
	fd = open("pretest", O_WRONLY | O_CREAT | O_EXCL, 0777);
	failure("Could not create test file?!");
	if (!assert(fd >= 0))
		return;
	n = facl_get(fd, 0, &acl);
	if (n != 0)
		close(fd);
	if (n != 0 && errno == ENOSYS) {
		skipping("ACL tests require ACL support on the filesystem");
		return;
	}
	failure("facl_get(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);

	if (acl->acl_type != ACLENT_T) {
		acl_free(acl);
		close(fd);
		skipping("Filesystem does not use POSIX.1e ACLs");
		return;
	}

	acl_free(acl);

	n = acl_fromtext("user::rwx,user:1:rw-,group::rwx,group:15:r-x,other:rwx,mask:rwx", &acl);
	if (n != 0)
		close(fd);
	failure("acl_fromtext(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);

	n = facl_set(fd, acl);
	acl_free(acl);
	if (n != 0 && errno == ENOSYS) {
		close(fd);
		skipping("ACL tests require ACL support on the filesystem");
		return;
	}
	failure("facl_set(): errno = %d (%s)",
	    errno, strerror(errno));
	assertEqualInt(0, n);
	close(fd);

	/* Create a write-to-disk object. */
	assert(NULL != (a = archive_write_disk_new()));
	archive_write_disk_set_options(a,
	    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL);

	/* Populate an archive entry with some metadata, including ACL info */
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "test0");
	archive_entry_set_mtime(ae, 123456, 7890);
	archive_entry_set_size(ae, 0);
	archive_test_set_acls(ae, acls2, sizeof(acls2)/sizeof(acls2[0]));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Verify the data on disk. */
	assertEqualInt(0, stat("test0", &st));
	assertEqualInt(st.st_mtime, 123456);
	n = acl_get("test0", 0, &acl);
	failure("acl_get(): errno = %d (%s)",
	    errno, strerror(errno));
	assert(n == 0);
	if (acl->acl_type != ACLENT_T) {
		acl_free(acl);
		skipping("Filesystem uses other than POSIX.1e ACLs");
		return;
	}
	assertEqualInt(ACLENT_T, acl->acl_type);
	compare_acls(acl, acls2, sizeof(acls2)/sizeof(acls2[0]));
	acl_free(acl);
#endif
}

/*
 * Verify ACL read-from-disk.  This test is FreeBSD-specific.
 */
DEFINE_TEST(test_acl_solaris_posix1e_read)
{
#if !defined(__sun__)
	skipping("Solaris-specific ACL read test");
#else
	struct archive *a;
	struct archive_entry *ae;
	int n, fd;
	const char *acl1_text, *acl2_text, *acl3_text;
	acl_t *acl;

	/*
	 * Manually construct a directory and two files with
	 * different ACLs.  This also serves to verify that ACLs
	 * are supported on the local filesystem.
	 */

	/* Create a test file f1 with acl1_text */
	fd = open("f1", O_WRONLY | O_CREAT | O_EXCL, 0777);
	failure("Could not create test file?!");
	if (!assert(fd >= 0))
		return;
	n = facl_get(fd, 0, &acl);
	if (n != 0)
		close(fd);
	if (n != 0 && errno == ENOSYS) {
		skipping("ACL tests require ACL support on the filesystem");
		return;
	}
	failure("facl_get(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);

	if (acl->acl_type != ACLENT_T) {
		acl_free(acl);
		close(fd);
		skipping("Filesystem does not use POSIX.1e ACLs");
		return;
	}

	acl_free(acl);

	acl1_text = "user::rwx,"
	    "group::rwx,"
	    "other:rwx,"
	    "user:1:rw-,"
	    "group:15:r-x,"
	    "mask:rwx";
	n = acl_fromtext(acl1_text, &acl);
	failure("acl_fromtext(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);

	n = facl_set(fd, acl);
	acl_free(acl);
	if (n != 0 && errno == ENOSYS) {
		close(fd);
		skipping("ACL tests require ACL support on the filesystem");
		return;
	}
	failure("facl_set(): errno = %d (%s)",
	    errno, strerror(errno));
	assertEqualInt(0, n);
	close(fd);

	assertMakeDir("d1", 0700);

	/*
	 * Create file d/f1 with acl2_text
	 *
	 * This differs from acl1_text in the u:1: and g:15: permissions.
	 *
	 * This file deliberately has the same name but a different ACL.
	 * Github Issue #777 explains how libarchive's directory traversal
	 * did not always correctly enter directories before attempting
	 * to read ACLs, resulting in reading the ACL from a like-named
	 * file in the wrong directory.
	 */
	acl2_text = "user::rwx,"
	    "group::rwx,"
	    "other:---,"
	    "user:1:r--,"
	    "group:15:r--,"
	    "mask:rwx";
	n = acl_fromtext(acl2_text, &acl);
	failure("acl_fromtext(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);

	fd = open("d1/f1", O_WRONLY | O_CREAT | O_EXCL, 0777);
	failure("Could not create test file?!");
	if (!assert(fd >= 0)) {
		acl_free(acl);
		return;
	}
	n = facl_set(fd, acl);
	acl_free(acl);
	if (n != 0 && errno == ENOSYS) {
		close(fd);
		skipping("ACL tests require ACL support on the filesystem");
		return;
	}
	failure("facl_set(): errno = %d (%s)",
	    errno, strerror(errno));
	assertEqualInt(0, n);
	close(fd);

	/* Create directory d2 with access and default ACLs */
	assertMakeDir("d2", 0755);

	acl3_text = "user::rwx,"
	    "group::r-x,"
	    "other:r-x,"
	    "user:2:r--,"
	    "group:16:-w-,"
	    "mask:rwx,"
	    "default:user::rwx,"
            "default:user:1:r--,"
	    "default:group::r-x,"
	    "default:group:15:r--,"
	    "default:mask:rwx,"
	    "default:other:r-x";

	n = acl_fromtext(acl3_text, &acl);
	failure("acl_fromtext(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);

	n = acl_set("d2", acl);
	acl_free(acl);
	if (n != 0 && errno == ENOSYS) {
		close(fd);
		skipping("ACL tests require ACL support on the filesystem");
		return;
	}
	failure("acl_set(): errno = %d (%s)",
	    errno, strerror(errno));
	assertEqualInt(0, n);

	/* Create a read-from-disk object. */
	assert(NULL != (a = archive_read_disk_new()));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "."));
	assert(NULL != (ae = archive_entry_new()));

	/* Walk the dir until we see both of the files */
	while (ARCHIVE_OK == archive_read_next_header2(a, ae)) {
		archive_read_disk_descend(a);
		if (strcmp(archive_entry_pathname(ae), "./f1") == 0) {
			assertEqualString(archive_entry_acl_to_text(ae, NULL, ARCHIVE_ENTRY_ACL_TYPE_POSIX1E | ARCHIVE_ENTRY_ACL_STYLE_SEPARATOR_COMMA | ARCHIVE_ENTRY_ACL_STYLE_SOLARIS), acl1_text);
		} else if (strcmp(archive_entry_pathname(ae), "./d1/f1") == 0) {
			assertEqualString(archive_entry_acl_to_text(ae, NULL, ARCHIVE_ENTRY_ACL_TYPE_POSIX1E | ARCHIVE_ENTRY_ACL_STYLE_SEPARATOR_COMMA | ARCHIVE_ENTRY_ACL_STYLE_SOLARIS), acl2_text);
		} else if (strcmp(archive_entry_pathname(ae), "./d2") == 0) {
			assertEqualString(archive_entry_acl_to_text(ae, NULL, ARCHIVE_ENTRY_ACL_TYPE_POSIX1E | ARCHIVE_ENTRY_ACL_STYLE_SEPARATOR_COMMA | ARCHIVE_ENTRY_ACL_STYLE_SOLARIS), acl3_text);
		}
	}

	archive_free(a);
#endif
}
