/*
 * Archive entry link resolver fuzzer for libarchive
 * Targets archive_entry_linkify (complexity: 775, zero coverage)
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "archive.h"
#include "archive_entry.h"

static constexpr size_t kMaxInputSize = 64 * 1024;  // 64KB

// Simple data consumer
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

  // Create a link resolver
  struct archive_entry_linkresolver *resolver = archive_entry_linkresolver_new();
  if (resolver == NULL) {
    return 0;
  }

  // Set the format strategy based on input
  uint8_t strategy = consumer.consume_byte() % 5;
  int format;
  switch (strategy) {
    case 0: format = ARCHIVE_FORMAT_TAR_GNUTAR; break;
    case 1: format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE; break;
    case 2: format = ARCHIVE_FORMAT_CPIO_POSIX; break;
    case 3: format = ARCHIVE_FORMAT_CPIO_SVR4_NOCRC; break;
    default: format = ARCHIVE_FORMAT_TAR_USTAR; break;
  }
  archive_entry_linkresolver_set_strategy(resolver, format);

  // Create multiple entries to test linkify with hardlinks
  struct archive_entry *entries[32];
  int num_entries = 0;

  while (!consumer.empty() && num_entries < 32 && consumer.remaining() > 20) {
    struct archive_entry *entry = archive_entry_new();
    if (entry == NULL) break;

    // Set pathname
    archive_entry_set_pathname(entry, consumer.consume_string(64));

    // Set inode and device for hardlink detection
    archive_entry_set_ino(entry, consumer.consume_int64());
    archive_entry_set_dev(entry, consumer.consume_uint32());
    archive_entry_set_nlink(entry, (consumer.consume_byte() % 5) + 1);

    // Set mode (regular file or directory)
    uint8_t ftype = consumer.consume_byte() % 2;
    mode_t mode = ftype ? (S_IFDIR | 0755) : (S_IFREG | 0644);
    archive_entry_set_mode(entry, mode);

    archive_entry_set_size(entry, consumer.consume_int64() & 0xFFFF);
    archive_entry_set_uid(entry, consumer.consume_uint32() & 0xFFFF);
    archive_entry_set_gid(entry, consumer.consume_uint32() & 0xFFFF);

    entries[num_entries++] = entry;
  }

  // Now run all entries through the linkresolver
  for (int i = 0; i < num_entries; i++) {
    struct archive_entry *entry = entries[i];
    struct archive_entry *spare = NULL;

    // This is the main function we want to fuzz (zero coverage)
    archive_entry_linkify(resolver, &entry, &spare);

    // entry and spare may be modified by linkify
    // We still need to free the original entries we allocated
    if (spare != NULL) {
      archive_entry_free(spare);
    }
  }

  // Free remaining entries from the resolver
  struct archive_entry *entry = NULL;
  struct archive_entry *spare = NULL;
  while (1) {
    archive_entry_linkify(resolver, &entry, &spare);
    if (entry == NULL)
      break;
    archive_entry_free(entry);
    entry = NULL;
    if (spare != NULL) {
      archive_entry_free(spare);
      spare = NULL;
    }
  }

  // Free all our created entries
  for (int i = 0; i < num_entries; i++) {
    if (entries[i] != NULL) {
      archive_entry_free(entries[i]);
    }
  }

  archive_entry_linkresolver_free(resolver);
  return 0;
}
