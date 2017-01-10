/*-
 * Copyright (c) 2003-2010 Tim Kientzle
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

struct myacl_t {
	int type;
	int permset;
	int tag;
	int qual; /* GID or UID of user/group, depending on tag. */
	const char *name; /* Name of user/group, depending on tag. */
};

static struct myacl_t acls_reg[] = {
	/* For this test, we need the file owner to be able to read and write the ACL. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_READ_ACL | ARCHIVE_ENTRY_ACL_WRITE_ACL | ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS | ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, ""},

	/* An entry for each type. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 108, "user108" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DENY, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 109, "user109" },

	/* An entry for each permission. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 112, "user112" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 113, "user113" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 115, "user115" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_APPEND_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 117, "user117" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS,
	  ARCHIVE_ENTRY_ACL_USER, 119, "user119" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS,
	  ARCHIVE_ENTRY_ACL_USER, 120, "user120" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES,
	  ARCHIVE_ENTRY_ACL_USER, 122, "user122" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES,
	  ARCHIVE_ENTRY_ACL_USER, 123, "user123" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_DELETE,
	  ARCHIVE_ENTRY_ACL_USER, 124, "user124" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_ACL,
	  ARCHIVE_ENTRY_ACL_USER, 125, "user125" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_ACL,
	  ARCHIVE_ENTRY_ACL_USER, 126, "user126" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_OWNER,
	  ARCHIVE_ENTRY_ACL_USER, 127, "user127" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_SYNCHRONIZE,
	  ARCHIVE_ENTRY_ACL_USER, 128, "user128" },

	/* One entry for each qualifier. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 135, "user135" },
//	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
//	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_GROUP, 136, "group136" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" }
};


static struct myacl_t acls_dir[] = {
	/* For this test, we need to be able to read and write the ACL. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_READ_ACL,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, ""},

	/* An entry for each type. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_USER, 101, "user101" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DENY, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_USER, 102, "user102" },

	/* An entry for each permission. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_USER, 201, "user201" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_ADD_FILE,
	  ARCHIVE_ENTRY_ACL_USER, 202, "user202" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_ADD_SUBDIRECTORY,
	  ARCHIVE_ENTRY_ACL_USER, 203, "user203" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS,
	  ARCHIVE_ENTRY_ACL_USER, 204, "user204" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS,
	  ARCHIVE_ENTRY_ACL_USER, 205, "user205" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_DELETE_CHILD,
	  ARCHIVE_ENTRY_ACL_USER, 206, "user206" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES,
	  ARCHIVE_ENTRY_ACL_USER, 207, "user207" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES,
	  ARCHIVE_ENTRY_ACL_USER, 208, "user208" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_DELETE,
	  ARCHIVE_ENTRY_ACL_USER, 209, "user209" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_ACL,
	  ARCHIVE_ENTRY_ACL_USER, 210, "user210" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_ACL,
	  ARCHIVE_ENTRY_ACL_USER, 211, "user211" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_OWNER,
	  ARCHIVE_ENTRY_ACL_USER, 212, "user212" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_SYNCHRONIZE,
	  ARCHIVE_ENTRY_ACL_USER, 213, "user213" },

	/* One entry with each inheritance value. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_FILE_INHERIT,
	  ARCHIVE_ENTRY_ACL_USER, 301, "user301" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_DIRECTORY_INHERIT,
	  ARCHIVE_ENTRY_ACL_USER, 302, "user302" },
#if 0
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_NO_PROPAGATE_INHERIT,
	  ARCHIVE_ENTRY_ACL_USER, 303, "user303" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_INHERIT_ONLY,
	  ARCHIVE_ENTRY_ACL_USER, 304, "user304" },
#endif

#if 0
	/* FreeBSD does not support audit entries. */
	{ ARCHIVE_ENTRY_ACL_TYPE_AUDIT,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_SUCCESSFUL_ACCESS,
	  ARCHIVE_ENTRY_ACL_USER, 401, "user401" },
	{ ARCHIVE_ENTRY_ACL_TYPE_AUDIT,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_FAILED_ACCESS,
	  ARCHIVE_ENTRY_ACL_USER, 402, "user402" },
#endif

	/* One entry for each qualifier. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_USER, 501, "user501" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_GROUP, 502, "group502" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" }
};

static void
set_acls(struct archive_entry *ae, struct myacl_t *acls, int start, int end)
{
	int i;

	archive_entry_acl_clear(ae);
	if (start > 0) {
		assertEqualInt(ARCHIVE_OK,
			archive_entry_acl_add_entry(ae,
			    acls[0].type, acls[0].permset, acls[0].tag,
			    acls[0].qual, acls[0].name));
	}
	for (i = start; i < end; i++) {
		assertEqualInt(ARCHIVE_OK,
		    archive_entry_acl_add_entry(ae,
			acls[i].type, acls[i].permset, acls[i].tag,
			acls[i].qual, acls[i].name));
	}
}

static int
acl_permset_to_bitmap(uint32_t a_access_mask)
{
	static struct { int machine; int portable; } perms[] = {
		{ACE_EXECUTE, ARCHIVE_ENTRY_ACL_EXECUTE},
		{ACE_READ_DATA, ARCHIVE_ENTRY_ACL_READ_DATA},
		{ACE_LIST_DIRECTORY, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY},
		{ACE_WRITE_DATA, ARCHIVE_ENTRY_ACL_WRITE_DATA},
		{ACE_ADD_FILE, ARCHIVE_ENTRY_ACL_ADD_FILE},
		{ACE_APPEND_DATA, ARCHIVE_ENTRY_ACL_APPEND_DATA},
		{ACE_ADD_SUBDIRECTORY, ARCHIVE_ENTRY_ACL_ADD_SUBDIRECTORY},
		{ACE_READ_NAMED_ATTRS, ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS},
		{ACE_WRITE_NAMED_ATTRS, ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS},
		{ACE_DELETE_CHILD, ARCHIVE_ENTRY_ACL_DELETE_CHILD},
		{ACE_READ_ATTRIBUTES, ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES},
		{ACE_WRITE_ATTRIBUTES, ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES},
		{ACE_DELETE, ARCHIVE_ENTRY_ACL_DELETE},
		{ACE_READ_ACL, ARCHIVE_ENTRY_ACL_READ_ACL},
		{ACE_WRITE_ACL, ARCHIVE_ENTRY_ACL_WRITE_ACL},
		{ACE_WRITE_OWNER, ARCHIVE_ENTRY_ACL_WRITE_OWNER},
		{ACE_SYNCHRONIZE, ARCHIVE_ENTRY_ACL_SYNCHRONIZE}
	};
	int i, permset = 0;

	for (i = 0; i < (int)(sizeof(perms)/sizeof(perms[0])); ++i)
		if (a_access_mask & perms[i].machine)
			permset |= perms[i].portable;
	return permset;
}

static int
acl_flagset_to_bitmap(uint16_t a_flags)
{
	static struct { int machine; int portable; } flags[] = {
		{ACE_FILE_INHERIT_ACE, ARCHIVE_ENTRY_ACL_ENTRY_FILE_INHERIT},
		{ACE_DIRECTORY_INHERIT_ACE, ARCHIVE_ENTRY_ACL_ENTRY_DIRECTORY_INHERIT},
		{ACE_NO_PROPAGATE_INHERIT_ACE, ARCHIVE_ENTRY_ACL_ENTRY_NO_PROPAGATE_INHERIT},
		{ACE_INHERIT_ONLY_ACE, ARCHIVE_ENTRY_ACL_ENTRY_INHERIT_ONLY},
		{ACE_SUCCESSFUL_ACCESS_ACE_FLAG, ARCHIVE_ENTRY_ACL_ENTRY_SUCCESSFUL_ACCESS},
		{ACE_FAILED_ACCESS_ACE_FLAG, ARCHIVE_ENTRY_ACL_ENTRY_FAILED_ACCESS},
		{ACE_INHERITED_ACE, ARCHIVE_ENTRY_ACL_ENTRY_INHERITED}
	};
	int i, flagset = 0;

	for (i = 0; i < (int)(sizeof(flags)/sizeof(flags[0])); ++i)
		if (a_flags & flags[i].machine)
			flagset |= flags[i].portable;
	return flagset;
}

static int
acl_match(ace_t *ace, struct myacl_t *myacl)
{
	int perms;

	/* translate the silly opaque permset to a bitmap */
	perms = acl_permset_to_bitmap(ace->a_access_mask) | acl_flagset_to_bitmap(ace->a_flags);
	if (perms != myacl->permset)
		return (0);

	if (ace->a_flags & ACE_OWNER) {
		if (myacl->tag != ARCHIVE_ENTRY_ACL_USER_OBJ)
			return (0);
	} else if (ace->a_flags & ACE_GROUP) {
		if (myacl->tag != ARCHIVE_ENTRY_ACL_GROUP_OBJ)
			return (0);
	} else if (ace->a_flags & ACE_EVERYONE) {
		if (myacl->tag != ARCHIVE_ENTRY_ACL_EVERYONE)
			return (0);
	} else if (ace->a_flags & ACE_IDENTIFIER_GROUP) {
		if (myacl->tag != ARCHIVE_ENTRY_ACL_GROUP)
			return (0);
		if ((gid_t)myacl->qual != ace->a_who)
			return (0);
	} else {
		if (myacl->tag != ARCHIVE_ENTRY_ACL_USER)
			return (0);
		if ((uid_t)myacl->qual != ace->a_who)
			return (0);
	}
	return (1);
}

static void
compare_acls(acl_t *acl, struct myacl_t *myacls, const char *filename, int start, int end)
{
	int *marker;
	int matched;
	int e, i, n;
	ace_t *ace;

	n = end - start;
	marker = malloc(sizeof(marker[0]) * (n + 1));
	for (i = 0; i < n; i++)
		marker[i] = i + start;
	/* Always include the first ACE. */
	if (start > 0) {
	  marker[n] = 0;
	  ++n;
	}

	/*
	 * Iterate over acls in system acl object, try to match each
	 * one with an item in the myacls array.
	 */
	for (e = 0; e < acl->acl_cnt; e++) {
		ace = &((ace_t *)acl->acl_aclp)[e];
		/* Search for a matching entry (tag and qualifier) */
		for (i = 0, matched = 0; i < n && !matched; i++) {
			if (acl_match(ace, &myacls[marker[i]])) {
				/* We found a match; remove it. */
				marker[i] = marker[n - 1];
				n--;
				matched = 1;
			}
		}

		failure("ACL entry on file %s that shouldn't be there", filename);
		assert(matched == 1);
	}

	/* Dump entries in the myacls array that weren't in the system acl. */
	for (i = 0; i < n; ++i) {
		failure(" ACL entry %d missing from %s: "
		    "type=%#010x,permset=%#010x,tag=%d,qual=%d,name=``%s''\n",
		    marker[i], filename,
		    myacls[marker[i]].type, myacls[marker[i]].permset,
		    myacls[marker[i]].tag, myacls[marker[i]].qual,
		    myacls[marker[i]].name);
		assert(0); /* Record this as a failure. */
	}
	free(marker);
}

static void
compare_entry_acls(struct archive_entry *ae, struct myacl_t *myacls, const char *filename, int start, int end)
{
	int *marker;
	int matched;
	int i, n;
	int type, permset, tag, qual;
	const char *name;

	/* Count ACL entries in myacls array and allocate an indirect array. */
	n = end - start;
	marker = malloc(sizeof(marker[0]) * (n + 1));
	for (i = 0; i < n; i++)
		marker[i] = i + start;
	/* Always include the first ACE. */
	if (start > 0) {
	  marker[n] = 0;
	  ++n;
	}

	/*
	 * Iterate over acls in entry, try to match each
	 * one with an item in the myacls array.
	 */
	assertEqualInt(n, archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	while (ARCHIVE_OK == archive_entry_acl_next(ae,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4, &type, &permset, &tag, &qual, &name)) {

		/* Search for a matching entry (tag and qualifier) */
		for (i = 0, matched = 0; i < n && !matched; i++) {
			if (tag == myacls[marker[i]].tag
			    && qual == myacls[marker[i]].qual
			    && permset == myacls[marker[i]].permset
			    && type == myacls[marker[i]].type) {
				/* We found a match; remove it. */
				marker[i] = marker[n - 1];
				n--;
				matched = 1;
			}
		}

		failure("ACL entry on file that shouldn't be there: "
			"type=%#010x,permset=%#010x,tag=%d,qual=%d",
			type,permset,tag,qual);
		assert(matched == 1);
	}

	/* Dump entries in the myacls array that weren't in the system acl. */
	for (i = 0; i < n; ++i) {
		failure(" ACL entry %d missing from %s: "
		    "type=%#010x,permset=%#010x,tag=%d,qual=%d,name=``%s''\n",
		    marker[i], filename,
		    myacls[marker[i]].type, myacls[marker[i]].permset,
		    myacls[marker[i]].tag, myacls[marker[i]].qual,
		    myacls[marker[i]].name);
		assert(0); /* Record this as a failure. */
	}
	free(marker);
}
#endif

/*
 * Verify ACL restore-to-disk.  This test is FreeBSD-specific.
 */

DEFINE_TEST(test_acl_solaris_nfs4)
{
#if !defined(__sun__)
	skipping("Solaris-specific NFS4 ACL restore test");
#else
	char buff[64];
	struct stat st;
	struct archive *a;
	struct archive_entry *ae;
	int i, n;
	acl_t *acl;

	/* Create a test dir and try to get trivial ACL from it. */
	if (!assertMakeDir("pretest", 0755))
		return;

	n = acl_get("pretest", 0, &acl);
	if (n != 0 && errno == ENOSYS) {
		skipping("ACL tests require ACL support on the filesystem");
		return;
	}
	failure("acl_get(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);

	if (acl->acl_type != ACE_T) {
		acl_free(acl);
		skipping("Filesystem does not use NFSv4 ACLs");
		return;
	}

	acl_free(acl);

	/* Create a write-to-disk object. */
	assert(NULL != (a = archive_write_disk_new()));
	archive_write_disk_set_options(a,
	    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL);

	/* Populate an archive entry with some metadata, including ACL info */
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "testall");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_perm(ae, 0654);
	archive_entry_set_mtime(ae, 123456, 7890);
	archive_entry_set_size(ae, 0);
	set_acls(ae, acls_reg, 0, (int)(sizeof(acls_reg)/sizeof(acls_reg[0])));

	/* Write the entry to disk, including ACLs. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));

	/* Likewise for a dir. */
	archive_entry_set_pathname(ae, "dirall");
	archive_entry_set_filetype(ae, AE_IFDIR);
	archive_entry_set_perm(ae, 0654);
	archive_entry_set_mtime(ae, 123456, 7890);
	set_acls(ae, acls_dir, 0, (int)(sizeof(acls_dir)/sizeof(acls_dir[0])));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));

	for (i = 0; i < (int)(sizeof(acls_dir)/sizeof(acls_dir[0])); ++i) {
	  sprintf(buff, "dir%d", i);
	  archive_entry_set_pathname(ae, buff);
	  archive_entry_set_filetype(ae, AE_IFDIR);
	  archive_entry_set_perm(ae, 0654);
	  archive_entry_set_mtime(ae, 123456 + i, 7891 + i);
	  set_acls(ae, acls_dir, i, i + 1);
	  assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	}

	archive_entry_free(ae);

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Verify the data on disk. */
	assertEqualInt(0, stat("testall", &st));
	assertEqualInt(st.st_mtime, 123456);
	n = acl_get("testall", 0, &acl);
	failure("acl_get(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);
	compare_acls(acl, acls_reg, "testall", 0, (int)(sizeof(acls_reg)/sizeof(acls_reg[0])));
	acl_free(acl);

	/* Verify single-permission dirs on disk. */
	for (i = 0; i < (int)(sizeof(acls_dir)/sizeof(acls_dir[0])); ++i) {
	  sprintf(buff, "dir%d", i);
	  assertEqualInt(0, stat(buff, &st));
	  assertEqualInt(st.st_mtime, 123456 + i);
	  n = acl_get(buff, 0, &acl);
	  failure("acl_get(): errno = %d (%s)", errno, strerror(errno));
	  assertEqualInt(0, n);
	  compare_acls(acl, acls_dir, buff, i, i + 1);
	  acl_free(acl);
	}

	/* Verify "dirall" on disk. */
	assertEqualInt(0, stat("dirall", &st));
	assertEqualInt(st.st_mtime, 123456);
	n = acl_get("dirall", 0, &acl);
	failure("acl_get(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);
	compare_acls(acl, acls_dir, "dirall", 0,  (int)(sizeof(acls_dir)/sizeof(acls_dir[0])));
	acl_free(acl);

	/* Read and compare ACL via archive_read_disk */
	a = archive_read_disk_new();
	assert(a != NULL);
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "testall");
	assertEqualInt(ARCHIVE_OK,
		       archive_read_disk_entry_from_file(a, ae, -1, NULL));
	compare_entry_acls(ae, acls_reg, "testall", 0, (int)(sizeof(acls_reg)/sizeof(acls_reg[0])));
	archive_entry_free(ae);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Read and compare ACL via archive_read_disk */
	a = archive_read_disk_new();
	assert(a != NULL);
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "dirall");
	assertEqualInt(ARCHIVE_OK,
		       archive_read_disk_entry_from_file(a, ae, -1, NULL));
	compare_entry_acls(ae, acls_dir, "dirall", 0,  (int)(sizeof(acls_dir)/sizeof(acls_dir[0])));
	archive_entry_free(ae);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
#endif
}
