/*
 * Archive entry fuzzer for libarchive
 * Targets archive_entry_* functions including ACL, linkify, and metadata
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "archive.h"
#include "archive_entry.h"

static constexpr size_t kMaxInputSize = 64 * 1024;  // 64KB

// FuzzedDataProvider-like helper for consuming bytes
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

  const char* consume_string(size_t max_len) {
    if (max_len > sizeof(string_buf_) - 1) max_len = sizeof(string_buf_) - 1;
    size_t avail = size_ - pos_;
    size_t len = (avail < max_len) ? avail : max_len;

    // Copy to internal buffer and null-terminate
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
  char string_buf_[512];
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len == 0 || len > kMaxInputSize) {
    return 0;
  }

  DataConsumer consumer(buf, len);

  struct archive_entry *entry = archive_entry_new();
  if (entry == NULL) {
    return 0;
  }

  // Set basic entry properties
  archive_entry_set_pathname(entry, consumer.consume_string(256));
  archive_entry_set_size(entry, consumer.consume_int64());
  archive_entry_set_mode(entry, consumer.consume_uint32());
  archive_entry_set_uid(entry, consumer.consume_uint32());
  archive_entry_set_gid(entry, consumer.consume_uint32());
  archive_entry_set_mtime(entry, consumer.consume_int64(), 0);
  archive_entry_set_atime(entry, consumer.consume_int64(), 0);
  archive_entry_set_ctime(entry, consumer.consume_int64(), 0);
  archive_entry_set_birthtime(entry, consumer.consume_int64(), 0);

  // Set various string fields
  archive_entry_set_uname(entry, consumer.consume_string(64));
  archive_entry_set_gname(entry, consumer.consume_string(64));
  archive_entry_set_symlink(entry, consumer.consume_string(256));
  archive_entry_set_hardlink(entry, consumer.consume_string(256));

  // Exercise ACL functions (low coverage targets)
  int acl_type = consumer.consume_byte() & 0x0F;
  int acl_permset = consumer.consume_uint32();
  int acl_tag = consumer.consume_byte() & 0x0F;
  int acl_qual = consumer.consume_uint32();
  const char *acl_name = consumer.consume_string(64);

  archive_entry_acl_add_entry(entry, acl_type, acl_permset, acl_tag, acl_qual, acl_name);

  // Add more ACL entries based on remaining data
  while (!consumer.empty() && consumer.remaining() > 10) {
    acl_type = consumer.consume_byte() & 0x0F;
    acl_permset = consumer.consume_uint32();
    acl_tag = consumer.consume_byte() & 0x0F;
    acl_qual = consumer.consume_uint32();
    acl_name = consumer.consume_string(32);
    archive_entry_acl_add_entry(entry, acl_type, acl_permset, acl_tag, acl_qual, acl_name);
  }

  // Exercise ACL text conversion functions (archive_acl_to_text_* are uncovered)
  ssize_t text_len;
  char *acl_text = archive_entry_acl_to_text(entry, &text_len, ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
  if (acl_text) {
    // Parse the text back
    archive_entry_acl_from_text(entry, acl_text, ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
    free(acl_text);
  }

  acl_text = archive_entry_acl_to_text(entry, &text_len, ARCHIVE_ENTRY_ACL_TYPE_DEFAULT);
  if (acl_text) {
    free(acl_text);
  }

  acl_text = archive_entry_acl_to_text(entry, &text_len, ARCHIVE_ENTRY_ACL_TYPE_NFS4);
  if (acl_text) {
    free(acl_text);
  }

  // Exercise wide character versions
  wchar_t *acl_text_w = archive_entry_acl_to_text_w(entry, &text_len, ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
  if (acl_text_w) {
    free(acl_text_w);
  }

  // Get pathname variants
  archive_entry_pathname(entry);
  archive_entry_pathname_w(entry);
  archive_entry_pathname_utf8(entry);

  // Clone the entry
  struct archive_entry *entry2 = archive_entry_clone(entry);
  if (entry2) {
    archive_entry_free(entry2);
  }

  // Clear and reuse
  archive_entry_clear(entry);

  archive_entry_free(entry);
  return 0;
}
