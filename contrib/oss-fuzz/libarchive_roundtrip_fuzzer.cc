/*
 * Archive roundtrip fuzzer for libarchive
 * Writes an archive then reads it back - tests write/read consistency
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <vector>

#include "archive.h"
#include "archive_entry.h"

static constexpr size_t kMaxInputSize = 64 * 1024;

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
  char string_buf_[128];
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len < 10 || len > kMaxInputSize) {
    return 0;
  }

  DataConsumer consumer(buf, len);
  std::vector<uint8_t> archive_data;
  archive_data.reserve(len * 2);

  // Phase 1: Write an archive
  struct archive *writer = archive_write_new();
  if (writer == NULL) {
    return 0;
  }

  // Select format
  uint8_t format = consumer.consume_byte() % 5;
  switch (format) {
    case 0: archive_write_set_format_pax_restricted(writer); break;
    case 1: archive_write_set_format_ustar(writer); break;
    case 2: archive_write_set_format_cpio_newc(writer); break;
    case 3: archive_write_set_format_zip(writer); break;
    default: archive_write_set_format_gnutar(writer); break;
  }

  archive_write_add_filter_none(writer);

  // Open to memory
  size_t used = 0;
  archive_data.resize(len * 4);
  if (archive_write_open_memory(writer, archive_data.data(), archive_data.size(), &used) != ARCHIVE_OK) {
    archive_write_free(writer);
    return 0;
  }

  // Write entries
  int entry_count = 0;
  while (!consumer.empty() && entry_count < 5 && consumer.remaining() > 10) {
    struct archive_entry *entry = archive_entry_new();
    if (entry == NULL) break;

    archive_entry_set_pathname(entry, consumer.consume_string(32));
    archive_entry_set_mode(entry, S_IFREG | 0644);
    archive_entry_set_uid(entry, consumer.consume_uint32() & 0xFFFF);
    archive_entry_set_gid(entry, consumer.consume_uint32() & 0xFFFF);

    size_t data_len;
    const uint8_t *data = consumer.consume_bytes(&data_len, 256);
    archive_entry_set_size(entry, data_len);

    if (archive_write_header(writer, entry) == ARCHIVE_OK && data_len > 0) {
      archive_write_data(writer, data, data_len);
    }

    archive_entry_free(entry);
    entry_count++;
  }

  archive_write_close(writer);
  archive_write_free(writer);

  if (used == 0) {
    return 0;
  }

  // Phase 2: Read the archive back
  struct archive *reader = archive_read_new();
  if (reader == NULL) {
    return 0;
  }

  archive_read_support_format_all(reader);
  archive_read_support_filter_all(reader);

  if (archive_read_open_memory(reader, archive_data.data(), used) != ARCHIVE_OK) {
    archive_read_free(reader);
    return 0;
  }

  std::vector<uint8_t> read_buffer(4096, 0);
  struct archive_entry *entry;
  while (archive_read_next_header(reader, &entry) == ARCHIVE_OK) {
    archive_entry_pathname(entry);
    archive_entry_size(entry);

    ssize_t r;
    while ((r = archive_read_data(reader, read_buffer.data(), read_buffer.size())) > 0)
      ;
  }

  archive_read_free(reader);
  return 0;
}
