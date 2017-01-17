/*-
 * Copyright (c) 2003-2010 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_disk.c 201159 2009-12-29 05:35:40Z kientzle $");

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_ACL_H
#define _ACL_PRIVATE /* For debugging */
#include <sys/acl.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_acl_private.h"
#include "archive_write_disk_private.h"

#if !HAVE_POSIX_ACL && !HAVE_SUN_ACL
/* Default empty function body to satisfy mainline code. */
int
archive_write_disk_set_acls(struct archive *a, int fd, const char *name,
	 struct archive_acl *abstract_acl)
{
	(void)a; /* UNUSED */
	(void)fd; /* UNUSED */
	(void)name; /* UNUSED */
	(void)abstract_acl; /* UNUSED */
	return (ARCHIVE_OK);
}

#else /* HAVE_POSIX_ACL || HAVE_SUN_ACL */

static int	set_acl(struct archive *, int fd, const char *,
			struct archive_acl *,
			acl_type_t, int archive_entry_acl_type, const char *tn);

/*
 * XXX TODO: What about ACL types other than ACCESS and DEFAULT?
 */
int
archive_write_disk_set_acls(struct archive *a, int fd, const char *name,
	 struct archive_acl *abstract_acl)
{
	int		 ret;

	if (archive_acl_count(abstract_acl,
	    ARCHIVE_ENTRY_ACL_TYPE_POSIX1E) > 0) {
#if HAVE_SUN_ACL
		/* Solaris writes POSIX.1e access and default ACLs together */
		ret = set_acl(a, fd, name, abstract_acl, ACLENT_T,
		    ARCHIVE_ENTRY_ACL_TYPE_POSIX1E, "posix1e");
#else
		ret = set_acl(a, fd, name, abstract_acl, ACL_TYPE_ACCESS,
		    ARCHIVE_ENTRY_ACL_TYPE_ACCESS, "access");
		if (ret != ARCHIVE_OK)
			return (ret);
		ret = set_acl(a, fd, name, abstract_acl, ACL_TYPE_DEFAULT,
		    ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, "default");
#endif
		return (ret);
	}
#if HAVE_ACL_TYPE_NFS4 || HAVE_SUN_ACL
	else if (archive_acl_count(abstract_acl,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4) > 0) {
#if HAVE_SUN_ACL
		ret = set_acl(a, fd, name, abstract_acl, ACE_T,
		    ARCHIVE_ENTRY_ACL_TYPE_NFS4, "nfs4");
#else	/* !HAVE_SUN_ACL */
		ret = set_acl(a, fd, name, abstract_acl, ACL_TYPE_NFS4,
		    ARCHIVE_ENTRY_ACL_TYPE_NFS4, "nfs4");
#endif	/* !HAVE_SUN_ACL */
		return (ret);
	}
#endif	/* HAVE_ACL_TYPE_NFS4 && HAVE_SUN_ACL */
	else {
		/* No ACLs found */
		return ARCHIVE_OK;
	}
}

/*
 * Translate system ACL permissions into libarchive internal structure
 */
static struct {
	int archive_perm;
	int platform_perm;
} acl_perm_map[] = {
#if HAVE_SUN_ACL	/* Solaris NFSv4 ACL permissions */
	{ARCHIVE_ENTRY_ACL_EXECUTE, ACE_EXECUTE},
	{ARCHIVE_ENTRY_ACL_READ_DATA, ACE_READ_DATA},
	{ARCHIVE_ENTRY_ACL_LIST_DIRECTORY, ACE_LIST_DIRECTORY},
	{ARCHIVE_ENTRY_ACL_WRITE_DATA, ACE_WRITE_DATA},
	{ARCHIVE_ENTRY_ACL_ADD_FILE, ACE_ADD_FILE},
	{ARCHIVE_ENTRY_ACL_APPEND_DATA, ACE_APPEND_DATA},
	{ARCHIVE_ENTRY_ACL_ADD_SUBDIRECTORY, ACE_ADD_SUBDIRECTORY},
	{ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS, ACE_READ_NAMED_ATTRS},
	{ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS, ACE_WRITE_NAMED_ATTRS},
	{ARCHIVE_ENTRY_ACL_DELETE_CHILD, ACE_DELETE_CHILD},
	{ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES, ACE_READ_ATTRIBUTES},
	{ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES, ACE_WRITE_ATTRIBUTES},
	{ARCHIVE_ENTRY_ACL_DELETE, ACE_DELETE},
	{ARCHIVE_ENTRY_ACL_READ_ACL, ACE_READ_ACL},
	{ARCHIVE_ENTRY_ACL_WRITE_ACL, ACE_WRITE_ACL},
	{ARCHIVE_ENTRY_ACL_WRITE_OWNER, ACE_WRITE_OWNER},
	{ARCHIVE_ENTRY_ACL_SYNCHRONIZE, ACE_SYNCHRONIZE}
#else	/* POSIX.1e ACL permissions */
	{ARCHIVE_ENTRY_ACL_EXECUTE, ACL_EXECUTE},
	{ARCHIVE_ENTRY_ACL_WRITE, ACL_WRITE},
	{ARCHIVE_ENTRY_ACL_READ, ACL_READ},
#if HAVE_ACL_TYPE_NFS4	/* FreeBSD NFSv4 ACL permissions */
	{ARCHIVE_ENTRY_ACL_READ_DATA, ACL_READ_DATA},
	{ARCHIVE_ENTRY_ACL_LIST_DIRECTORY, ACL_LIST_DIRECTORY},
	{ARCHIVE_ENTRY_ACL_WRITE_DATA, ACL_WRITE_DATA},
	{ARCHIVE_ENTRY_ACL_ADD_FILE, ACL_ADD_FILE},
	{ARCHIVE_ENTRY_ACL_APPEND_DATA, ACL_APPEND_DATA},
	{ARCHIVE_ENTRY_ACL_ADD_SUBDIRECTORY, ACL_ADD_SUBDIRECTORY},
	{ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS, ACL_READ_NAMED_ATTRS},
	{ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS, ACL_WRITE_NAMED_ATTRS},
	{ARCHIVE_ENTRY_ACL_DELETE_CHILD, ACL_DELETE_CHILD},
	{ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES, ACL_READ_ATTRIBUTES},
	{ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES, ACL_WRITE_ATTRIBUTES},
	{ARCHIVE_ENTRY_ACL_DELETE, ACL_DELETE},
	{ARCHIVE_ENTRY_ACL_READ_ACL, ACL_READ_ACL},
	{ARCHIVE_ENTRY_ACL_WRITE_ACL, ACL_WRITE_ACL},
	{ARCHIVE_ENTRY_ACL_WRITE_OWNER, ACL_WRITE_OWNER},
	{ARCHIVE_ENTRY_ACL_SYNCHRONIZE, ACL_SYNCHRONIZE}
#endif
#endif	/* !HAVE_SUN_ACL */
};

#if HAVE_ACL_TYPE_NFS4 || HAVE_SUN_ACL
/*
 * Translate system NFSv4 inheritance flags into libarchive internal structure
 */
static struct {
	int archive_inherit;
	int platform_inherit;
} acl_inherit_map[] = {
#if HAVE_SUN_ACL
	{ARCHIVE_ENTRY_ACL_ENTRY_FILE_INHERIT, ACE_FILE_INHERIT_ACE},
	{ARCHIVE_ENTRY_ACL_ENTRY_DIRECTORY_INHERIT, ACE_DIRECTORY_INHERIT_ACE},
	{ARCHIVE_ENTRY_ACL_ENTRY_NO_PROPAGATE_INHERIT, ACE_NO_PROPAGATE_INHERIT_ACE},
	{ARCHIVE_ENTRY_ACL_ENTRY_INHERIT_ONLY, ACE_INHERIT_ONLY_ACE},
	{ARCHIVE_ENTRY_ACL_ENTRY_SUCCESSFUL_ACCESS, ACE_SUCCESSFUL_ACCESS_ACE_FLAG},
	{ARCHIVE_ENTRY_ACL_ENTRY_FAILED_ACCESS, ACE_FAILED_ACCESS_ACE_FLAG},
	{ARCHIVE_ENTRY_ACL_ENTRY_INHERITED, ACE_INHERITED_ACE}
#else	/* !HAVE_SUN_ACL */
	{ARCHIVE_ENTRY_ACL_ENTRY_FILE_INHERIT, ACL_ENTRY_FILE_INHERIT},
	{ARCHIVE_ENTRY_ACL_ENTRY_DIRECTORY_INHERIT, ACL_ENTRY_DIRECTORY_INHERIT},
	{ARCHIVE_ENTRY_ACL_ENTRY_NO_PROPAGATE_INHERIT, ACL_ENTRY_NO_PROPAGATE_INHERIT},
	{ARCHIVE_ENTRY_ACL_ENTRY_INHERIT_ONLY, ACL_ENTRY_INHERIT_ONLY},
	{ARCHIVE_ENTRY_ACL_ENTRY_SUCCESSFUL_ACCESS, ACL_ENTRY_SUCCESSFUL_ACCESS},
	{ARCHIVE_ENTRY_ACL_ENTRY_FAILED_ACCESS, ACL_ENTRY_FAILED_ACCESS},
	{ARCHIVE_ENTRY_ACL_ENTRY_INHERITED, ACL_ENTRY_INHERITED}
#endif	/* !HAVE_SUN_ACL */
};
#endif	/* HAVE_ACL_TYPE_NFS4 || HAVE_SUN_ACL */

static int
set_acl(struct archive *a, int fd, const char *name,
    struct archive_acl *abstract_acl,
    acl_type_t acl_type, int ae_requested_type, const char *tname)
{
#if HAVE_SUN_ACL
	aclent_t	 *aclent;
	ace_t		 *ace;
	int		 e, r;
	acl_t		 *acl;
#else
	acl_t		 acl;
	acl_entry_t	 acl_entry;
	acl_permset_t	 acl_permset;
#endif
#if HAVE_ACL_TYPE_NFS4
	acl_flagset_t	 acl_flagset;
	int		 r;
#endif
	int		 ret;
	int		 ae_type, ae_permset, ae_tag, ae_id;
	uid_t		 ae_uid;
	gid_t		 ae_gid;
	const char	*ae_name;
	int		 entries;
	int		 i;

	ret = ARCHIVE_OK;
	entries = archive_acl_reset(abstract_acl, ae_requested_type);
	if (entries == 0)
		return (ARCHIVE_OK);

#if HAVE_SUN_ACL
	acl = NULL;
	acl = malloc(sizeof(acl_t));
	if (acl == NULL) {
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
			"Invalid ACL type");
		return (ARCHIVE_FAILED);
	}
	if (acl_type == ACE_T)
		acl->acl_entry_size = sizeof(ace_t);
	else if (acl_type == ACLENT_T)
		acl->acl_entry_size = sizeof(aclent_t);
	else {
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
			"Invalid ACL type");
		acl_free(acl);
		return (ARCHIVE_FAILED);
	}
	acl->acl_type = acl_type;
	acl->acl_cnt = entries;

	acl->acl_aclp = malloc(entries * acl->acl_entry_size);
	if (acl->acl_aclp == NULL) {
		archive_set_error(a, errno,
		    "Can't allocate memory for acl buffer");
		acl_free(acl);
		return (ARCHIVE_FAILED);
	}
#else	/* !HAVE_SUN_ACL */
	acl = acl_init(entries);
	if (acl == (acl_t)NULL) {
		archive_set_error(a, errno,
		    "Failed to initialize ACL working storage");
		return (ARCHIVE_FAILED);
	}
#endif	/* !HAVE_SUN_ACL */
#if HAVE_SUN_ACL
	e = 0;
#endif
	while (archive_acl_next(a, abstract_acl, ae_requested_type, &ae_type,
		   &ae_permset, &ae_tag, &ae_id, &ae_name) == ARCHIVE_OK) {
#if HAVE_SUN_ACL
		ace = NULL;
		aclent = NULL;
		if (acl->acl_type == ACE_T)  {
			ace = &((ace_t *)acl->acl_aclp)[e];
			ace->a_who = -1;
			ace->a_access_mask = 0;
			ace->a_flags = 0;
		} else {
			aclent = &((aclent_t *)acl->acl_aclp)[e];
			aclent->a_id = -1;
			aclent->a_type = 0;
			aclent->a_perm = 0;
		}
#else	/* !HAVE_SUN_ACL */
		if (acl_create_entry(&acl, &acl_entry) != 0) {
			archive_set_error(a, errno,
			    "Failed to create a new ACL entry");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}
#endif	/* !HAVE_SUN_ACL */
		switch (ae_tag) {
#if HAVE_SUN_ACL
		case ARCHIVE_ENTRY_ACL_USER:
			ae_uid = archive_write_disk_uid(a, ae_name, ae_id);
			if (acl->acl_type == ACE_T)
				ace->a_who = ae_uid;
			else {
				aclent->a_id = ae_uid;
				aclent->a_type |= USER;
			}
			break;
		case ARCHIVE_ENTRY_ACL_GROUP:
			ae_gid = archive_write_disk_gid(a, ae_name, ae_id);
			if (acl->acl_type == ACE_T) {
				ace->a_who = ae_gid;
				ace->a_flags |= ACE_IDENTIFIER_GROUP;
			} else {
				aclent->a_id = ae_gid;
				aclent->a_type |= GROUP;
			}
			break;
		case ARCHIVE_ENTRY_ACL_USER_OBJ:
			if (acl->acl_type == ACE_T)
				ace->a_flags |= ACE_OWNER;
			else
				aclent->a_type |= USER_OBJ;
			break;
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			if (acl->acl_type == ACE_T) {
				ace->a_flags |= ACE_GROUP;
				ace->a_flags |= ACE_IDENTIFIER_GROUP;
			} else
				aclent->a_type |= GROUP_OBJ;
			break;
		case ARCHIVE_ENTRY_ACL_MASK:
			aclent->a_type |= CLASS_OBJ;
			break;
		case ARCHIVE_ENTRY_ACL_OTHER:
			aclent->a_type |= OTHER_OBJ;
			break;
		case ARCHIVE_ENTRY_ACL_EVERYONE:
			ace->a_flags |= ACE_EVERYONE;
			break;
#else	/* !HAVE_SUN_ACL */
		case ARCHIVE_ENTRY_ACL_USER:
			acl_set_tag_type(acl_entry, ACL_USER);
			ae_uid = archive_write_disk_uid(a, ae_name, ae_id);
			acl_set_qualifier(acl_entry, &ae_uid);
			break;
		case ARCHIVE_ENTRY_ACL_GROUP:
			acl_set_tag_type(acl_entry, ACL_GROUP);
			ae_gid = archive_write_disk_gid(a, ae_name, ae_id);
			acl_set_qualifier(acl_entry, &ae_gid);
			break;
		case ARCHIVE_ENTRY_ACL_USER_OBJ:
			acl_set_tag_type(acl_entry, ACL_USER_OBJ);
			break;
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			acl_set_tag_type(acl_entry, ACL_GROUP_OBJ);
			break;
		case ARCHIVE_ENTRY_ACL_MASK:
			acl_set_tag_type(acl_entry, ACL_MASK);
			break;
		case ARCHIVE_ENTRY_ACL_OTHER:
			acl_set_tag_type(acl_entry, ACL_OTHER);
			break;
#if HAVE_ACL_TYPE_NFS4
		case ARCHIVE_ENTRY_ACL_EVERYONE:
			acl_set_tag_type(acl_entry, ACL_EVERYONE);
			break;
#endif
#endif	/* !HAVE_SUN_ACL */
		default:
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Unknown ACL tag");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}

#if HAVE_ACL_TYPE_NFS4 || HAVE_SUN_ACL
		r = 0;
		switch (ae_type) {
#if HAVE_SUN_ACL
		case ARCHIVE_ENTRY_ACL_TYPE_ALLOW:
			if (ace != NULL)
				ace->a_type = ACE_ACCESS_ALLOWED_ACE_TYPE;
			else
				r = -1;
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_DENY:
			if (ace != NULL)
				ace->a_type = ACE_ACCESS_DENIED_ACE_TYPE;
			else
				r = -1;
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_AUDIT:
			if (ace != NULL)
				ace->a_type = ACE_SYSTEM_AUDIT_ACE_TYPE;
			else
				r = -1;
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_ALARM:
			if (ace != NULL)
				ace->a_type = ACE_SYSTEM_ALARM_ACE_TYPE;
			else
				r = -1;
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_ACCESS:
			if (aclent == NULL)
				r = -1;
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_DEFAULT:
			if (aclent != NULL)
				aclent->a_type |= ACL_DEFAULT;
			else
				r = -1;
			break;
#else	/* !HAVE_SUN_ACL */
		case ARCHIVE_ENTRY_ACL_TYPE_ALLOW:
			r = acl_set_entry_type_np(acl_entry, ACL_ENTRY_TYPE_ALLOW);
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_DENY:
			r = acl_set_entry_type_np(acl_entry, ACL_ENTRY_TYPE_DENY);
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_AUDIT:
			r = acl_set_entry_type_np(acl_entry, ACL_ENTRY_TYPE_AUDIT);
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_ALARM:
			r = acl_set_entry_type_np(acl_entry, ACL_ENTRY_TYPE_ALARM);
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_ACCESS:
		case ARCHIVE_ENTRY_ACL_TYPE_DEFAULT:
			// These don't translate directly into the system ACL.
			break;
#endif	/* !HAVE_SUN_ACL */
		default:
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Unknown ACL entry type");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}

		if (r != 0) {
#if HAVE_SUN_ACL
			errno = EINVAL;
#endif
			archive_set_error(a, errno,
			    "Failed to set ACL entry type");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}
#endif	/* HAVE_ACL_TYPE_NFS4 || HAVE_SUN_ACL */

#if HAVE_SUN_ACL
		if (acl->acl_type == ACLENT_T) {
			if (ae_permset & ARCHIVE_ENTRY_ACL_EXECUTE)
				aclent->a_perm |= 1;
			if (ae_permset & ARCHIVE_ENTRY_ACL_WRITE)
				aclent->a_perm |= 2;
			if (ae_permset & ARCHIVE_ENTRY_ACL_READ)
				aclent->a_perm |= 4;
		} else
#else
		if (acl_get_permset(acl_entry, &acl_permset) != 0) {
			archive_set_error(a, errno,
			    "Failed to get ACL permission set");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}
		if (acl_clear_perms(acl_permset) != 0) {
			archive_set_error(a, errno,
			    "Failed to clear ACL permissions");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}
#endif	/* !HAVE_SUN_ACL */
		for (i = 0; i < (int)(sizeof(acl_perm_map) / sizeof(acl_perm_map[0])); ++i) {
			if (ae_permset & acl_perm_map[i].archive_perm) {
#if HAVE_SUN_ACL
				ace->a_access_mask |=
				    acl_perm_map[i].platform_perm;
#else
				if (acl_add_perm(acl_permset,
				    acl_perm_map[i].platform_perm) != 0) {
					archive_set_error(a, errno,
					    "Failed to add ACL permission");
					ret = ARCHIVE_FAILED;
					goto exit_free;
				}
#endif
			}
		}

#if HAVE_ACL_TYPE_NFS4 || HAVE_SUN_ACL
#if HAVE_SUN_ACL
		if (acl_type == ACE_T)
#else
		if (acl_type == ACL_TYPE_NFS4)
#endif
		{
#ifdef HAVE_POSIX_ACL
			/*
			 * acl_get_flagset_np() fails with non-NFSv4 ACLs
			 */
			if (acl_get_flagset_np(acl_entry, &acl_flagset) != 0) {
				archive_set_error(a, errno,
				    "Failed to get flagset from an NFSv4 ACL entry");
				ret = ARCHIVE_FAILED;
				goto exit_free;
			}
			if (acl_clear_flags_np(acl_flagset) != 0) {
				archive_set_error(a, errno,
				    "Failed to clear flags from an NFSv4 ACL flagset");
				ret = ARCHIVE_FAILED;
				goto exit_free;
			}
#endif
			for (i = 0; i < (int)(sizeof(acl_inherit_map) /sizeof(acl_inherit_map[0])); ++i) {
				if (ae_permset & acl_inherit_map[i].archive_inherit) {
#if HAVE_SUN_ACL
					ace->a_flags |=
					    acl_inherit_map[i].platform_inherit;
#else	/* !HAVE_SUN_ACL */
					if (acl_add_flag_np(acl_flagset,
							acl_inherit_map[i].platform_inherit) != 0) {
						archive_set_error(a, errno,
						    "Failed to add flag to NFSv4 ACL flagset");
						ret = ARCHIVE_FAILED;
						goto exit_free;
					}
#endif	/* HAVE_SUN_ACLi */
				}
			}
		}
#endif
#if HAVE_SUN_ACL
	e++;
#endif
	}

#if HAVE_ACL_SET_FD_NP || HAVE_ACL_SET_FD || HAVE_SUN_ACL
	/* Try restoring the ACL through 'fd' if we can. */
#if HAVE_SUN_ACL || HAVE_ACL_SET_FD_NP
	if (fd >= 0)
#else	/* !HAVE_SUN_ACL && !HAVE_ACL_SET_FD_NP */
	if (fd >= 0 && acl_type == ACL_TYPE_ACCESS)
#endif
	{
#if HAVE_SUN_ACL
		if (facl_set(fd, acl) == 0)
#elif HAVE_ACL_SET_FD_NP
		if (acl_set_fd_np(fd, acl, acl_type) == 0)
#else	/* !HAVE_SUN_ACL && !HAVE_ACL_SET_FD_NP */
		if (acl_set_fd(fd, acl) == 0)
#endif
			ret = ARCHIVE_OK;
		else {
			if (errno == EOPNOTSUPP) {
				/* Filesystem doesn't support ACLs */
				ret = ARCHIVE_OK;
			} else {
				archive_set_error(a, errno,
				    "Failed to set %s acl on fd", tname);
			}
		}
	} else
#endif	/* HAVE_ACL_SET_FD_NP || HAVE_ACL_SET_FD || HAVE_SUN_ACL */
#if HAVE_SUN_ACL
	if (acl_set(name, acl) != 0)
#elif HAVE_ACL_SET_LINK_NP
	if (acl_set_link_np(name, acl_type, acl) != 0)
#else
	/* TODO: Skip this if 'name' is a symlink. */
	if (acl_set_file(name, acl_type, acl) != 0)
#endif
	{
		if (errno == EOPNOTSUPP) {
			/* Filesystem doesn't support ACLs */
			ret = ARCHIVE_OK;
		} else {
			archive_set_error(a, errno, "Failed to set %s acl",
			    tname);
			ret = ARCHIVE_WARN;
		}
	}
exit_free:
	acl_free(acl);
	return (ret);
}
#endif	/* HAVE_POSIX_ACL || HAVE_SUN_ACL */
