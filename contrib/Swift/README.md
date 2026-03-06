# Swift libarchive Wrapper

A Swift Package that compiles [libarchive](https://libarchive.org) from source and provides an idiomatic Swift API for reading and writing archive files.

## Installation

Add to your `Package.swift`:

```swift
dependencies: [
    .package(url: "https://github.com/libarchive/libarchive", from: "3.8.5")
]
```

Then add `"Archive"` to your target's dependencies.

## Quick Start

### Reading an archive

```swift
import Archive

// From a file
let reader = try ArchiveReader(path: "/path/to/archive.tar.gz")
try reader.forEachEntry { entry, reader in
    print(entry.pathname)
    let data = try reader.readData()
}

// From in-memory data
let reader = try ArchiveReader(data: archiveData)
let entries = try reader.listEntries()
```

### Writing an archive

```swift
import Archive

// In-memory tar.gz
let writer = try ArchiveWriter(format: .tar, filters: [.gzip])
try writer.writeEntry(
    ArchiveEntry(pathname: "hello.txt", size: Int64(data.count)),
    data: data
)
let archiveData = try writer.finish()

// Write to file
let writer = try ArchiveWriter(path: "/tmp/out.zip", format: .zip)
try writer.writeEntry(
    ArchiveEntry(pathname: "file.txt", size: Int64(data.count)),
    data: data
)
try writer.close()
```

### Extracting to disk

```swift
let reader = try ArchiveReader(data: archiveData)
try reader.extractAll(to: "/tmp/output", options: .default)
```

### Data extensions

```swift
// Compress data into an archive
let compressed = try myData.compress(as: "file.txt", format: .tar, filters: [.gzip])

// Decompress archive data
let files = try compressed.decompressArchive()  // [String: Data]

// List entries
let entries = try archiveData.archiveEntries()
```

## Supported Formats

| Format | Read | Write |
|--------|------|-------|
| tar (pax, gnutar, ustar, v7) | Yes | Yes |
| zip | Yes | Yes |
| 7-Zip | Yes | Yes |
| cpio | Yes | Yes |
| ar | Yes | Yes |
| xar | Yes | Yes |
| ISO 9660 | Yes | Yes |
| shar | No | Yes |
| mtree | Yes | Yes |
| WARC | Yes | Yes |
| RAW | Yes | Yes |
| RAR, LHA, CAB | Yes | No |

## Compression Filters

| Filter | Read | Write | Trait |
|--------|------|-------|-------|
| gzip | Yes | Yes | `GzipSupport` (default) |
| bzip2 | Yes | Yes | `Bzip2Support` (default) |
| compress | Yes | Yes | Always available |
| xz/lzma | Yes | Yes | `LZMASupport` (requires liblzma) |
| zstd | Yes | Yes | `ZstdSupport` (requires libzstd) |
| lz4 | Yes | Yes | Always available |

### Filter composition

Combine format and filters for common archive types:

```swift
// .tar.gz
try ArchiveWriter(format: .tar, filters: [.gzip])

// .tar.bz2
try ArchiveWriter(format: .tar, filters: [.bzip2])

// .tar.xz (requires LZMASupport trait)
try ArchiveWriter(format: .tar, filters: [.xz])
```

## Traits Configuration

The package uses Swift Package Manager traits to control optional compression library support:

| Trait | Default | Description |
|-------|---------|-------------|
| `GzipSupport` | Enabled | zlib (available in macOS SDK) |
| `Bzip2Support` | Enabled | bzip2 (available in macOS SDK) |
| `LZMASupport` | Disabled | Requires liblzma (e.g., `brew install xz`) |
| `ZstdSupport` | Disabled | Requires libzstd (e.g., `brew install zstd`) |

Enable additional traits:

```bash
swift build --traits LZMASupport,ZstdSupport
```

## Error Handling

All operations throw `ArchiveError` on failure:

```swift
do {
    let reader = try ArchiveReader(data: corruptData)
    _ = try reader.listEntries()
} catch let error as ArchiveError {
    print(error.code)     // libarchive errno
    print(error.message)  // Human-readable description
}
```

## Extract Options

Control extraction behavior with `ExtractOptions`:

```swift
let reader = try ArchiveReader(data: archiveData)
try reader.extractAll(to: "/tmp/out", options: [.permissions, .time, .secure])
```

Available options: `.permissions`, `.time`, `.owner`, `.acl`, `.xattr`, `.fflags`, `.noOverwrite`, `.noOverwriteNewer`, `.secure`.

## Concurrency

- `ArchiveEntry`, `ArchiveError`, `ArchiveFormat`, `ArchiveFilter`, `FileType`, and `ExtractOptions` are all `Sendable`
- `ArchiveReader` and `ArchiveWriter` are reference types (classes) that should not be shared across threads

## Architecture

The package has three targets:

1. **CArchive** — C target that compiles libarchive sources directly
2. **Archive** — Swift wrapper with idiomatic API
3. **ArchiveTests** — Tests using Swift Testing framework
