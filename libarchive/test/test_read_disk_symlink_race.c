#include "test.h"

struct swap_paths {
	const char *entry;
	const char *original;
	const char *saved;
	const char *replacement;
	int done;
};

static int
swap_path(struct archive *archive, void *data, struct archive_entry *entry)
{
	struct swap_paths *paths = data;

	(void)archive;
	if (!paths->done && strcmp(paths->entry,
	    archive_entry_pathname(entry)) == 0) {
		assertEqualInt(0, rename(paths->original, paths->saved));
		assertEqualInt(0, rename(paths->replacement, paths->original));
		paths->done = 1;
	}
	return (1);
}

DEFINE_TEST(test_read_disk_symlink_race)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	skipping("Symlink race test requires POSIX filesystem semantics");
#else
	struct archive *archive;
	struct archive_entry *entry;
	struct swap_paths paths;
	char buffer[32];
	int flags, result;

	flags = ARCHIVE_READDISK_NO_ACL | ARCHIVE_READDISK_NO_XATTR |
	    ARCHIVE_READDISK_NO_FFLAGS | ARCHIVE_READDISK_NO_SPARSE;
	assertMakeDir("file-tree", 0755);
	assertMakeFile("outside-file", 0600, "secret");
	assertMakeFile("file-tree/file", 0600, "safe");
	assertMakeSymlink("file-tree/replacement", "../outside-file", 0);
	paths.entry = "file-tree/file";
	paths.original = "file-tree/file";
	paths.saved = "file-tree/saved";
	paths.replacement = "file-tree/replacement";
	paths.done = 0;
	archive = archive_read_disk_new();
	assert(archive != NULL);
	assertEqualIntA(archive, ARCHIVE_OK,
	    archive_read_disk_set_symlink_physical(archive));
	assertEqualIntA(archive, ARCHIVE_OK,
	    archive_read_disk_set_behavior(archive, flags));
	assertEqualIntA(archive, ARCHIVE_OK,
	    archive_read_disk_set_metadata_filter_callback(archive,
	    swap_path, &paths));
	assertEqualIntA(archive, ARCHIVE_OK,
	    archive_read_disk_open(archive, "file-tree/file"));
	assertEqualIntA(archive, ARCHIVE_OK,
	    archive_read_next_header(archive, &entry));
	assertEqualInt(1, paths.done);
	assert(archive_read_data(archive, buffer, sizeof(buffer)) < 0);
	assertEqualInt(ARCHIVE_OK, archive_read_free(archive));

	assertMakeDir("dir-tree", 0755);
	assertMakeDir("dir-tree/dir", 0755);
	assertMakeFile("dir-tree/dir/safe", 0600, "safe");
	assertMakeDir("outside-dir", 0755);
	assertMakeFile("outside-dir/secret", 0600, "secret");
	assertMakeSymlink("dir-tree/replacement", "../outside-dir", 1);
	paths.entry = "dir-tree/dir";
	paths.original = "dir-tree/dir";
	paths.saved = "dir-tree/saved";
	paths.replacement = "dir-tree/replacement";
	paths.done = 0;
	archive = archive_read_disk_new();
	assert(archive != NULL);
	assertEqualIntA(archive, ARCHIVE_OK,
	    archive_read_disk_set_symlink_physical(archive));
	assertEqualIntA(archive, ARCHIVE_OK,
	    archive_read_disk_set_behavior(archive, flags));
	assertEqualIntA(archive, ARCHIVE_OK,
	    archive_read_disk_set_metadata_filter_callback(archive,
	    swap_path, &paths));
	assertEqualIntA(archive, ARCHIVE_OK,
	    archive_read_disk_open(archive, "dir-tree/dir"));
	assertEqualIntA(archive, ARCHIVE_OK,
	    archive_read_next_header(archive, &entry));
	assertEqualInt(1, paths.done);
	assertEqualIntA(archive, 1, archive_read_disk_can_descend(archive));
	assertEqualIntA(archive, ARCHIVE_OK,
	    archive_read_disk_descend(archive));
	result = archive_read_next_header(archive, &entry);
	assertEqualIntA(archive, ARCHIVE_FAILED, result);
	assertEqualInt(ARCHIVE_OK, archive_read_free(archive));
#endif
}
