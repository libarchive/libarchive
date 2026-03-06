import CArchive
import Foundation

/// Writes entries and data to an archive.
public final class ArchiveWriter {
    private let archive: OpaquePointer
    private let entry: OpaquePointer
    private let tempPath: String?
    private var closed = false

    /// Creates a writer that writes to a temporary file (for in-memory use).
    ///
    /// Call `finish()` to get the resulting `Data`.
    public init(format: ArchiveFormat, filters: [ArchiveFilter] = [.none]) throws {
        guard let a = archive_write_new() else {
            throw ArchiveError(message: "Failed to create archive writer")
        }
        guard let e = archive_entry_new() else {
            archive_write_free(a)
            throw ArchiveError(message: "Failed to create archive entry")
        }
        self.archive = a
        self.entry = e
        let tmp = NSTemporaryDirectory() + "archive_\(ProcessInfo.processInfo.globallyUniqueString)"
        self.tempPath = tmp

        // After all stored properties are set, deinit handles cleanup on throw.
        let fr = format.setWriteFormat(a)
        if fr != ARCHIVE_OK && fr != ARCHIVE_WARN {
            throw ArchiveError(archive: a)
        }
        for filter in filters {
            let flr = filter.addWriteFilter(a)
            if flr != ARCHIVE_OK && flr != ARCHIVE_WARN {
                throw ArchiveError(archive: a)
            }
        }
        let r = archive_write_open_filename(a, tmp)
        if r != ARCHIVE_OK {
            throw ArchiveError(archive: a)
        }
    }

    /// Creates a writer that writes directly to a file path.
    public init(path: String, format: ArchiveFormat, filters: [ArchiveFilter] = [.none]) throws {
        guard let a = archive_write_new() else {
            throw ArchiveError(message: "Failed to create archive writer")
        }
        guard let e = archive_entry_new() else {
            archive_write_free(a)
            throw ArchiveError(message: "Failed to create archive entry")
        }
        self.archive = a
        self.entry = e
        self.tempPath = nil

        let fr = format.setWriteFormat(a)
        if fr != ARCHIVE_OK && fr != ARCHIVE_WARN {
            throw ArchiveError(archive: a)
        }
        for filter in filters {
            let flr = filter.addWriteFilter(a)
            if flr != ARCHIVE_OK && flr != ARCHIVE_WARN {
                throw ArchiveError(archive: a)
            }
        }
        let r = archive_write_open_filename(a, path)
        if r != ARCHIVE_OK {
            throw ArchiveError(archive: a)
        }
    }

    deinit {
        if !closed {
            archive_write_close(archive)
        }
        archive_entry_free(entry)
        archive_write_free(archive)
        if let tmp = tempPath {
            try? FileManager.default.removeItem(atPath: tmp)
        }
    }

    /// Writes an entry with optional data to the archive.
    public func writeEntry(_ archiveEntry: ArchiveEntry, data: Data? = nil) throws {
        archive_entry_clear(entry)
        archiveEntry.apply(to: entry)
        if let data = data {
            archive_entry_set_size(entry, Int64(data.count))
        }
        let r = archive_write_header(archive, entry)
        if r != ARCHIVE_OK && r != ARCHIVE_WARN {
            throw ArchiveError(archive: archive)
        }
        if let data = data, !data.isEmpty {
            let written = data.withUnsafeBytes { buf in
                archive_write_data(archive, buf.baseAddress, buf.count)
            }
            if written < 0 {
                throw ArchiveError(archive: archive)
            }
        }
    }

    /// Finishes writing and returns the archive data (for in-memory writers).
    public func finish() throws -> Data {
        let r = archive_write_close(archive)
        closed = true
        if r != ARCHIVE_OK && r != ARCHIVE_WARN {
            throw ArchiveError(archive: archive)
        }
        guard let tmp = tempPath else {
            throw ArchiveError(message: "finish() called on file-based writer; use close() instead")
        }
        return try Data(contentsOf: URL(fileURLWithPath: tmp))
    }

    /// Closes a file-based archive writer.
    public func close() throws {
        let r = archive_write_close(archive)
        closed = true
        if r != ARCHIVE_OK && r != ARCHIVE_WARN {
            throw ArchiveError(archive: archive)
        }
    }
}
