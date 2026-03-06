import CArchive
import Foundation

/// Reads entries and data from an archive.
public final class ArchiveReader {
    private let archive: OpaquePointer
    /// Using NSData for a stable .bytes pointer that outlives withUnsafeBytes.
    private let retainedData: NSData?

    /// Opens an archive from a file path.
    public init(path: String) throws {
        guard let a = archive_read_new() else {
            throw ArchiveError(message: "Failed to create archive reader")
        }
        self.archive = a
        self.retainedData = nil
        archive_read_support_filter_all(a)
        archive_read_support_format_all(a)
        let r = archive_read_open_filename(a, path, 10240)
        if r != ARCHIVE_OK {
            throw ArchiveError(archive: a)
            // deinit will call archive_read_free
        }
    }

    /// Opens an archive from in-memory data.
    public init(data: Data) throws {
        guard let a = archive_read_new() else {
            throw ArchiveError(message: "Failed to create archive reader")
        }
        self.archive = a
        let nsData = data as NSData
        self.retainedData = nsData
        archive_read_support_filter_all(a)
        archive_read_support_format_all(a)
        let r = archive_read_open_memory(a, nsData.bytes, nsData.length)
        if r != ARCHIVE_OK {
            throw ArchiveError(archive: a)
        }
    }

    deinit {
        archive_read_free(archive)
    }

    /// The detected archive format, available after reading the first header.
    public var format: ArchiveFormat? {
        ArchiveFormat(rawFormat: archive_format(archive))
    }

    private func nextHeader() throws -> OpaquePointer? {
        var entryPtr: OpaquePointer?
        let r = archive_read_next_header(archive, &entryPtr)
        if r == ARCHIVE_EOF { return nil }
        if r != ARCHIVE_OK && r != ARCHIVE_WARN {
            throw ArchiveError(archive: archive)
        }
        return entryPtr
    }

    /// Iterates over all entries in the archive.
    ///
    /// Call `readData()` within the closure to get the entry's contents.
    public func forEachEntry(_ body: (ArchiveEntry, ArchiveReader) throws -> Void) throws {
        while let ep = try nextHeader() {
            guard let entry = ArchiveEntry(entry: ep) else { continue }
            try body(entry, self)
        }
    }

    /// Reads the data for the current entry.
    public func readData() throws -> Data {
        var result = Data()
        var buffer: UnsafeRawPointer?
        var size: Int = 0
        var offset: Int64 = 0
        while true {
            let r = archive_read_data_block(archive, &buffer, &size, &offset)
            if r == ARCHIVE_EOF { break }
            if r != ARCHIVE_OK {
                throw ArchiveError(archive: archive)
            }
            if let buffer = buffer, size > 0 {
                result.append(UnsafeBufferPointer(
                    start: buffer.assumingMemoryBound(to: UInt8.self),
                    count: size
                ))
            }
        }
        return result
    }

    /// Extracts the entire archive to the specified directory.
    public func extractAll(to directory: String, options: ExtractOptions = .default) throws {
        guard let disk = archive_write_disk_new() else {
            throw ArchiveError(message: "Failed to create disk writer")
        }
        defer { archive_write_free(disk) }
        // Remove security flags that conflict with our path rewriting:
        // - SECURE_NOABSOLUTEPATHS: we prepend the directory ourselves
        // - SECURE_SYMLINKS: macOS /var is a symlink to /private/var
        let conflictFlags = ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS | ARCHIVE_EXTRACT_SECURE_SYMLINKS
        archive_write_disk_set_options(disk, options.rawValue & ~conflictFlags)
        archive_write_disk_set_standard_lookup(disk)

        let fm = FileManager.default
        var isDir: ObjCBool = false
        if !fm.fileExists(atPath: directory, isDirectory: &isDir) {
            try fm.createDirectory(atPath: directory, withIntermediateDirectories: true)
        }

        while let ep = try nextHeader() {
            if let origPath = archive_entry_pathname(ep) {
                let fullPath = (directory as NSString).appendingPathComponent(String(cString: origPath))
                archive_entry_set_pathname(ep, fullPath)
            }
            let wh = archive_write_header(disk, ep)
            if wh != ARCHIVE_OK && wh != ARCHIVE_WARN {
                throw ArchiveError(archive: disk)
            }
            var buf: UnsafeRawPointer?
            var size: Int = 0
            var offset: Int64 = 0
            while true {
                let dr = archive_read_data_block(archive, &buf, &size, &offset)
                if dr == ARCHIVE_EOF { break }
                if dr != ARCHIVE_OK {
                    throw ArchiveError(archive: archive)
                }
                let wr = archive_write_data_block(disk, buf, size, offset)
                if wr != ARCHIVE_OK {
                    throw ArchiveError(archive: disk)
                }
            }
            let fe = archive_write_finish_entry(disk)
            if fe != ARCHIVE_OK && fe != ARCHIVE_WARN {
                throw ArchiveError(archive: disk)
            }
        }
    }

    /// Lists all entries in the archive without reading data.
    public func listEntries() throws -> [ArchiveEntry] {
        var entries: [ArchiveEntry] = []
        while let ep = try nextHeader() {
            if let entry = ArchiveEntry(entry: ep) {
                entries.append(entry)
            }
        }
        return entries
    }
}
