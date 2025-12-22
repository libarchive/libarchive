/*
 * Archive write disk fuzzer for libarchive
 * Tests extraction to filesystem
 * Security-critical: path traversal, permission handling, symlink attacks
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <vector>

#include "archive.h"
#include "archive_entry.h"

static constexpr size_t kMaxInputSize = 64 * 1024;

static char g_temp_dir[256] = {0};

class DataConsumer {
public:
  DataConsumer(const uint8_t *data, size_t size) : data_(data), size_(size), pos_(0) {
    memset(string_buf_, 0, sizeof(string_buf_));
  }

  bool empty() const { return pos_ >= size_; }

  uint8_t consume_byte() {
    if (pos_ >= size_) return 0;
    return data_[pos_++];
  }

  uint32_t consume_uint32() {
    uint32_t val = 0;
    for (int i = 0; i < 4 && pos_ < size_; i++) {
      val |= static_cast<uint32_t>(data_[pos_++]) << (i * 8);
    }
    return val;
  }

  int64_t consume_int64() {
    int64_t val = 0;
    for (int i = 0; i < 8 && pos_ < size_; i++) {
      val |= static_cast<int64_t>(data_[pos_++]) << (i * 8);
    }
    return val;
  }

  const char* consume_path(size_t max_len) {
    if (max_len > sizeof(string_buf_) - 1) max_len = sizeof(string_buf_) - 1;
    size_t avail = size_ - pos_;
    size_t len = (avail < max_len) ? avail : max_len;

    size_t actual_len = 0;
    while (actual_len < len && pos_ < size_) {
      char c = static_cast<char>(data_[pos_++]);
      if (c == '\0') break;
      string_buf_[actual_len++] = c;
    }
    string_buf_[actual_len] = '\0';
    return string_buf_;
  }

  const uint8_t* consume_bytes(size_t *out_len, size_t max_len) {
    size_t avail = size_ - pos_;
    size_t len = (avail < max_len) ? avail : max_len;
    const uint8_t *ptr = data_ + pos_;
    pos_ += len;
    *out_len = len;
    return ptr;
  }

  size_t remaining() const { return size_ - pos_; }

private:
  const uint8_t *data_;
  size_t size_;
  size_t pos_;
  char string_buf_[256];
};

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
  (void)argc;
  (void)argv;
  // Create a temporary directory for extraction
  snprintf(g_temp_dir, sizeof(g_temp_dir), "/tmp/fuzz_extract_XXXXXX");
  if (mkdtemp(g_temp_dir) == NULL) {
    g_temp_dir[0] = '\0';
  }
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len == 0 || len > kMaxInputSize) {
    return 0;
  }

  if (g_temp_dir[0] == '\0') {
    return 0;
  }

  DataConsumer consumer(buf, len);

  struct archive *disk = archive_write_disk_new();
  if (disk == NULL) {
    return 0;
  }

  // Configure write disk options
  uint8_t opt_flags = consumer.consume_byte();
  int flags = 0;
  if (opt_flags & 0x01) flags |= ARCHIVE_EXTRACT_TIME;
  if (opt_flags & 0x02) flags |= ARCHIVE_EXTRACT_PERM;
  if (opt_flags & 0x04) flags |= ARCHIVE_EXTRACT_ACL;
  if (opt_flags & 0x08) flags |= ARCHIVE_EXTRACT_FFLAGS;
  if (opt_flags & 0x10) flags |= ARCHIVE_EXTRACT_OWNER;
  if (opt_flags & 0x20) flags |= ARCHIVE_EXTRACT_XATTR;
  if (opt_flags & 0x40) flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS;
  if (opt_flags & 0x80) flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;

  archive_write_disk_set_options(disk, flags);
  archive_write_disk_set_standard_lookup(disk);

  // Create entries to extract
  int entry_count = 0;
  while (!consumer.empty() && entry_count < 5 && consumer.remaining() > 20) {
    struct archive_entry *entry = archive_entry_new();
    if (entry == NULL) break;

    // Build a safe path within our temp directory
    char safe_path[512];
    const char *name = consumer.consume_path(32);
    snprintf(safe_path, sizeof(safe_path), "%s/%s", g_temp_dir, name);

    // Sanitize path to prevent traversal
    char *p = safe_path;
    while (*p) {
      if (p[0] == '.' && p[1] == '.') {
        p[0] = '_';
        p[1] = '_';
      }
      p++;
    }

    archive_entry_set_pathname(entry, safe_path);

    uint8_t ftype = consumer.consume_byte() % 3;
    mode_t mode;
    switch (ftype) {
      case 0: mode = S_IFREG | 0644; break;
      case 1: mode = S_IFDIR | 0755; break;
      default: mode = S_IFREG | 0644; break;
    }
    archive_entry_set_mode(entry, mode);

    archive_entry_set_uid(entry, 1000);
    archive_entry_set_gid(entry, 1000);
    archive_entry_set_mtime(entry, consumer.consume_int64(), 0);

    // Write the entry header
    if (archive_write_header(disk, entry) == ARCHIVE_OK) {
      if (S_ISREG(mode)) {
        size_t data_len;
        const uint8_t *data = consumer.consume_bytes(&data_len, 256);
        archive_entry_set_size(entry, data_len);
        if (data_len > 0) {
          archive_write_data(disk, data, data_len);
        }
      }
      archive_write_finish_entry(disk);
    }

    archive_entry_free(entry);
    entry_count++;
  }

  archive_write_close(disk);
  archive_write_free(disk);

  // Clean up extracted files
  char cmd[600];
  snprintf(cmd, sizeof(cmd), "rm -rf %s/* 2>/dev/null", g_temp_dir);
  (void)system(cmd);

  return 0;
}
