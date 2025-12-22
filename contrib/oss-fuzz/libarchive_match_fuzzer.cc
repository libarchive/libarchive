/*
 * Archive match fuzzer for libarchive
 * Tests pattern matching, time matching, and owner matching
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "archive.h"
#include "archive_entry.h"

static constexpr size_t kMaxInputSize = 32 * 1024;

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

  int64_t consume_int64() {
    int64_t val = 0;
    for (int i = 0; i < 8 && pos_ < size_; i++) {
      val |= static_cast<int64_t>(data_[pos_++]) << (i * 8);
    }
    return val;
  }

  const char* consume_string(size_t max_len) {
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

  size_t remaining() const { return size_ - pos_; }

private:
  const uint8_t *data_;
  size_t size_;
  size_t pos_;
  char string_buf_[256];
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len == 0 || len > kMaxInputSize) {
    return 0;
  }

  DataConsumer consumer(buf, len);

  struct archive *match = archive_match_new();
  if (match == NULL) {
    return 0;
  }

  // Add various match patterns
  while (!consumer.empty() && consumer.remaining() > 5) {
    uint8_t match_type = consumer.consume_byte() % 6;

    switch (match_type) {
      case 0: {
        // Pattern exclusion
        const char *pattern = consumer.consume_string(64);
        archive_match_exclude_pattern(match, pattern);
        break;
      }
      case 1: {
        // Pattern inclusion
        const char *pattern = consumer.consume_string(64);
        archive_match_include_pattern(match, pattern);
        break;
      }
      case 2: {
        // Time comparison (newer than)
        int64_t sec = consumer.consume_int64();
        int64_t nsec = consumer.consume_int64() % 1000000000;
        archive_match_include_time(match, ARCHIVE_MATCH_MTIME | ARCHIVE_MATCH_NEWER,
                                   sec, nsec);
        break;
      }
      case 3: {
        // Time comparison (older than)
        int64_t sec = consumer.consume_int64();
        int64_t nsec = consumer.consume_int64() % 1000000000;
        archive_match_include_time(match, ARCHIVE_MATCH_MTIME | ARCHIVE_MATCH_OLDER,
                                   sec, nsec);
        break;
      }
      case 4: {
        // UID inclusion
        int64_t uid = consumer.consume_int64() & 0xFFFF;
        archive_match_include_uid(match, uid);
        break;
      }
      case 5: {
        // GID inclusion
        int64_t gid = consumer.consume_int64() & 0xFFFF;
        archive_match_include_gid(match, gid);
        break;
      }
    }
  }

  // Create a test entry and check if it matches
  struct archive_entry *entry = archive_entry_new();
  if (entry) {
    archive_entry_set_pathname(entry, "test/file.txt");
    archive_entry_set_mtime(entry, 1234567890, 0);
    archive_entry_set_uid(entry, 1000);
    archive_entry_set_gid(entry, 1000);
    archive_entry_set_mode(entry, 0644 | 0100000);  // Regular file

    // Test matching
    archive_match_path_excluded(match, entry);
    archive_match_time_excluded(match, entry);
    archive_match_owner_excluded(match, entry);
    archive_match_excluded(match, entry);

    archive_entry_free(entry);
  }

  archive_match_free(match);
  return 0;
}
