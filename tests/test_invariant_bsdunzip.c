#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

static int file_exists_outside_extraction_dir(const char *extraction_dir, const char *target_path) {
    char real_extract[PATH_MAX];
    char real_target[PATH_MAX];
    
    if (!realpath(extraction_dir, real_extract)) return 0;
    if (!realpath(target_path, real_target)) return 0;
    
    return strncmp(real_target, real_extract, strlen(real_extract)) != 0;
}

START_TEST(test_zip_slip_boundary_enforcement)
{
    // Invariant: Extracted files must never escape the extraction directory boundary
    const char *test_archives[] = {
        "test_symlink_escape.zip",      // Symlink pointing to /tmp followed by file write
        "test_dotdot_escape.zip",       // Path with ../../etc/passwd
        "test_valid_archive.zip"        // Valid archive with normal paths
    };
    int num_archives = sizeof(test_archives) / sizeof(test_archives[0]);

    for (int i = 0; i < num_archives; i++) {
        char extraction_dir[256];
        snprintf(extraction_dir, sizeof(extraction_dir), "/tmp/unzip_test_%d", getpid());
        mkdir(extraction_dir, 0755);
        
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "./unzip/bsdunzip -d %s %s 2>/dev/null", 
                 extraction_dir, test_archives[i]);
        system(cmd);
        
        // Check no files escaped extraction directory
        char sensitive_path[256];
        snprintf(sensitive_path, sizeof(sensitive_path), "/tmp/escaped_file_%d", i);
        ck_assert_msg(!file_exists_outside_extraction_dir(extraction_dir, sensitive_path),
                      "File escaped extraction directory for archive: %s", test_archives[i]);
        
        // Cleanup
        snprintf(cmd, sizeof(cmd), "rm -rf %s", extraction_dir);
        system(cmd);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_zip_slip_boundary_enforcement);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}