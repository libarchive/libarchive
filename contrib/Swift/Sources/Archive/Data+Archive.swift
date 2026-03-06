import Foundation

extension Data {
    /// Lists the entries in this archive data.
    public func archiveEntries() throws -> [ArchiveEntry] {
        let reader = try ArchiveReader(data: self)
        return try reader.listEntries()
    }

    /// Decompresses/extracts this archive data, returning a dictionary of
    /// pathname to file data for each regular file entry.
    public func decompressArchive() throws -> [String: Data] {
        let reader = try ArchiveReader(data: self)
        var result: [String: Data] = [:]
        try reader.forEachEntry { entry, reader in
            if entry.fileType == .regular {
                let data = try reader.readData()
                result[entry.pathname] = data
            }
        }
        return result
    }

    /// Creates an archive from this data with the given filename.
    public func compress(as filename: String, format: ArchiveFormat, filters: [ArchiveFilter] = []) throws -> Data {
        let writer = try ArchiveWriter(format: format, filters: filters)
        let entry = ArchiveEntry(pathname: filename, size: Int64(self.count))
        try writer.writeEntry(entry, data: self)
        return try writer.finish()
    }
}
