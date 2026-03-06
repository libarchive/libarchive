import Testing
import Foundation
@testable import Archive

/// When the `GzipSupport` trait is enabled we will run the `.gzip` tests
let gzipSupport: ArchiveFilter? = {
    #if GzipSupport
    .gzip
    #else
    nil
    #endif
}()

/// When the `Bzip2Support` trait is enabled we will run the `.bzip2` tests
let bzip2Support: ArchiveFilter? = {
    #if Bzip2Support
    .bzip2
    #else
    nil
    #endif
}()

/// When the `LZMASupport` trait is enabled we will run the `.xz` tests
let xzSupport: ArchiveFilter? = {
    #if LZMASupport
    .xz
    #else
    nil
    #endif
}()

/// When the `LZMASupport` trait is enabled we will run the `.xz` tests
let lzmaSupport: ArchiveFilter? = {
    #if LZMASupport
    .lzma
    #else
    nil
    #endif
}()

/// When the `ZstdSupport` trait is enabled we will run the `.zstd` tests
let zstdSupport: ArchiveFilter? = {
    #if ZstdSupport
    .zstd
    #else
    nil
    #endif
}()

// MARK: - Version Tests

@Test func versionInfo() {
    let version = Archive.version
    #expect(version.contains("libarchive"))
}

// MARK: - Format Round-Trip Tests

@Suite("Format Round-Trips")
struct FormatRoundTripTests {

    @Test func tarRoundTrip() throws {
        try roundTrip(format: .tar)
    }

    @Test func zipRoundTrip() throws {
        try roundTrip(format: .zip)
    }

    @Test func sevenZipRoundTrip() throws {
        try roundTrip(format: .sevenZip)
    }

    @Test func cpioRoundTrip() throws {
        try roundTrip(format: .cpio)
    }

    @Test func arRoundTrip() throws {
        let data = Data("ar file content".utf8)
        let writer = try ArchiveWriter(format: .ar)
        let entry = ArchiveEntry(pathname: "test.txt", size: Int64(data.count))
        try writer.writeEntry(entry, data: data)
        let archiveData = try writer.finish()
        #expect(!archiveData.isEmpty)

        let reader = try ArchiveReader(data: archiveData)
        var found = false
        try reader.forEachEntry { entry, reader in
            let readData = try reader.readData()
            #expect(readData == data)
            found = true
        }
        #expect(found)
    }

    @Test(.disabled(if: true))
    func xarRoundTrip() throws {
        try roundTrip(format: .xar)
    }

    private func roundTrip(format: ArchiveFormat) throws {
        let file1Data = Data("Hello, World!".utf8)
        let file2Data = Data("Second file content with more data here.".utf8)

        let writer = try ArchiveWriter(format: format)
        try writer.writeEntry(
            ArchiveEntry(pathname: "file1.txt", size: Int64(file1Data.count)),
            data: file1Data
        )
        try writer.writeEntry(
            ArchiveEntry(pathname: "file2.txt", size: Int64(file2Data.count)),
            data: file2Data
        )
        let archiveData = try writer.finish()
        #expect(!archiveData.isEmpty)

        let reader = try ArchiveReader(data: archiveData)
        var files: [String: Data] = [:]
        try reader.forEachEntry { entry, reader in
            let data = try reader.readData()
            files[entry.pathname] = data
        }

        #expect(files["file1.txt"] == file1Data)
        #expect(files["file2.txt"] == file2Data)
    }
}

// MARK: - Filter Round-Trip Tests

@Suite("Filter Round-Trips")
struct FilterRoundTripTests {

    @Test
    func compressFilter() throws {
        try filterRoundTrip(filter: .compress)
    }

    @Test(.disabled(if: gzipSupport == nil))
    func gzipFilter() throws {
        try filterRoundTrip(filter: gzipSupport!)
    }

    @Test(.disabled(if: bzip2Support == nil))
    func bzip2Filter() throws {
        try filterRoundTrip(filter: bzip2Support!)
    }

    @Test(.disabled(if: xzSupport == nil))
    func xzFilter() throws {
        try filterRoundTrip(filter: xzSupport!)
    }

    @Test(.disabled(if: lzmaSupport == nil))
    func lzmaFilter() throws {
        try filterRoundTrip(filter: lzmaSupport!)
    }

    @Test(.disabled(if: zstdSupport == nil))
    func zstdFilter() throws {
        try filterRoundTrip(filter: zstdSupport!)
    }

    private func filterRoundTrip(filter: ArchiveFilter) throws {
        let fileData = Data("This is test data for filter round-trip testing. Repeating content for compression: abcabcabcabc.".utf8)

        let writer = try ArchiveWriter(format: .tar, filters: [filter])
        try writer.writeEntry(
            ArchiveEntry(pathname: "compressed.txt", size: Int64(fileData.count)),
            data: fileData
        )
        let archiveData = try writer.finish()
        #expect(!archiveData.isEmpty)

        let reader = try ArchiveReader(data: archiveData)
        var readBack: Data?
        try reader.forEachEntry { entry, reader in
            #expect(entry.pathname == "compressed.txt")
            readBack = try reader.readData()
        }
        #expect(readBack == fileData)
    }
}

// MARK: - Filter Composition Tests

@Suite("Filter Composition")
struct FilterCompositionTests {

    @Test(.disabled(if: gzipSupport == nil))
    func tarGzip() throws {
        try compositionRoundTrip(filters: [gzipSupport!])
    }

    @Test(.disabled(if: bzip2Support == nil))
    func tarBzip2() throws {
        try compositionRoundTrip(filters: [bzip2Support!])
    }

    @Test(.disabled(if: xzSupport == nil))
    func tarXz() throws {
        try compositionRoundTrip(filters: [xzSupport!])
    }

    @Test(.disabled(if: zstdSupport == nil))
    func tarZstd() throws {
        try compositionRoundTrip(filters: [zstdSupport!])
    }

    private func compositionRoundTrip(filters: [ArchiveFilter]) throws {
        let fileData = Data("Composition test data with repetition: xyzxyzxyzxyz.".utf8)

        let writer = try ArchiveWriter(format: .tar, filters: filters)
        try writer.writeEntry(
            ArchiveEntry(pathname: "composed.txt", size: Int64(fileData.count)),
            data: fileData
        )
        let archiveData = try writer.finish()
        #expect(!archiveData.isEmpty)

        let reader = try ArchiveReader(data: archiveData)
        try reader.forEachEntry { entry, reader in
            #expect(entry.pathname == "composed.txt")
            let data = try reader.readData()
            #expect(data == fileData)
        }
    }
}

// MARK: - Write/Read Tests

@Suite("Write and Read")
struct WriteReadTests {

    @Test func multipleEntries() throws {
        let writer = try ArchiveWriter(format: .tar)
        for i in 0..<10 {
            let data = Data("Content of file \(i)".utf8)
            try writer.writeEntry(
                ArchiveEntry(pathname: "dir/file\(i).txt", size: Int64(data.count)),
                data: data
            )
        }
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        let entries = try reader.listEntries()
        #expect(entries.count == 10)
    }

    @Test func directoryEntries() throws {
        let writer = try ArchiveWriter(format: .tar)
        try writer.writeEntry(
            ArchiveEntry(pathname: "mydir/", fileType: .directory, permissions: 0o755)
        )
        let fileData = Data("file in dir".utf8)
        try writer.writeEntry(
            ArchiveEntry(pathname: "mydir/file.txt", size: Int64(fileData.count)),
            data: fileData
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var entries: [ArchiveEntry] = []
        try reader.forEachEntry { entry, _ in
            entries.append(entry)
        }
        #expect(entries.count == 2)
        #expect(entries[0].fileType == .directory)
        #expect(entries[1].fileType == .regular)
    }

    @Test func unicodePathnames() throws {
        let writer = try ArchiveWriter(format: .tar)
        let data = Data("unicode content".utf8)
        let name = "日本語/ファイル.txt"
        try writer.writeEntry(
            ArchiveEntry(pathname: name, size: Int64(data.count)),
            data: data
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        try reader.forEachEntry { entry, _ in
            #expect(entry.pathname == name)
        }
    }

    @Test func emptyArchive() throws {
        let writer = try ArchiveWriter(format: .tar)
        let archiveData = try writer.finish()
        #expect(!archiveData.isEmpty)

        let reader = try ArchiveReader(data: archiveData)
        let entries = try reader.listEntries()
        #expect(entries.isEmpty)
    }

    @Test func largeEntry() throws {
        let size = 1024 * 1024
        var largeData = Data(count: size)
        for i in 0..<size {
            largeData[i] = UInt8(i & 0xFF)
        }

        let writer = try ArchiveWriter(format: .tar)
        try writer.writeEntry(
            ArchiveEntry(pathname: "large.bin", size: Int64(size)),
            data: largeData
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        try reader.forEachEntry { entry, reader in
            let readData = try reader.readData()
            #expect(readData.count == size)
            #expect(readData == largeData)
        }
    }

    @Test func permissionsPreserved() throws {
        let writer = try ArchiveWriter(format: .tar)
        let data = Data("test".utf8)
        try writer.writeEntry(
            ArchiveEntry(pathname: "exec.sh", size: Int64(data.count), permissions: 0o755),
            data: data
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        try reader.forEachEntry { entry, _ in
            #expect(entry.permissions == 0o755)
        }
    }

    @Test func symlinkEntry() throws {
        let writer = try ArchiveWriter(format: .tar)
        let data = Data("target file".utf8)
        try writer.writeEntry(
            ArchiveEntry(pathname: "target.txt", size: Int64(data.count)),
            data: data
        )
        try writer.writeEntry(
            ArchiveEntry(pathname: "link.txt", fileType: .symbolicLink, symlinkTarget: "target.txt")
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var entries: [ArchiveEntry] = []
        try reader.forEachEntry { entry, _ in
            entries.append(entry)
        }
        #expect(entries.count == 2)
        #expect(entries[1].fileType == .symbolicLink)
        #expect(entries[1].symlinkTarget == "target.txt")
    }
}

// MARK: - Extract Tests

@Suite("Extract")
struct ExtractTests {

    @Test func extractToDirectory() throws {
        let fileData = Data("extracted content".utf8)
        let writer = try ArchiveWriter(format: .tar)
        try writer.writeEntry(
            ArchiveEntry(pathname: "extracted.txt", size: Int64(fileData.count)),
            data: fileData
        )
        let archiveData = try writer.finish()

        let tmpDir = NSTemporaryDirectory() + "archive_test_\(ProcessInfo.processInfo.globallyUniqueString)"
        defer { try? FileManager.default.removeItem(atPath: tmpDir) }

        let reader = try ArchiveReader(data: archiveData)
        try reader.extractAll(to: tmpDir)

        let extractedPath = (tmpDir as NSString).appendingPathComponent("extracted.txt")
        let extractedData = try Data(contentsOf: URL(fileURLWithPath: extractedPath))
        #expect(extractedData == fileData)
    }
}

// MARK: - Data Extension Tests

@Suite("Data Extensions")
struct DataExtensionTests {

    @Test(.disabled(if: gzipSupport == nil))
    func compressDecompressGzip() throws {
        let original = Data("Hello, gzip compression round-trip!".utf8)
        let compressed = try original.compress(as: "test.txt", format: .tar, filters: [gzipSupport!])
        #expect(!compressed.isEmpty)

        let decompressed = try compressed.decompressArchive()
        #expect(decompressed["test.txt"] == original)
    }

    @Test(.disabled(if: bzip2Support == nil))
    func compressDecompressBzip2() throws {
        let original = Data("Hello, bzip2 compression round-trip!".utf8)
        let compressed = try original.compress(as: "test.txt", format: .tar, filters: [bzip2Support!])
        #expect(!compressed.isEmpty)

        let decompressed = try compressed.decompressArchive()
        #expect(decompressed["test.txt"] == original)
    }

    @Test(.disabled(if: xzSupport == nil))
    func compressDecompressXz() throws {
        let original = Data("Hello, xz compression round-trip!".utf8)
        let compressed = try original.compress(as: "test.txt", format: .tar, filters: [xzSupport!])
        #expect(!compressed.isEmpty)

        let decompressed = try compressed.decompressArchive()
        #expect(decompressed["test.txt"] == original)
    }

    @Test(.disabled(if: zstdSupport == nil))
    func compressDecompressZstd() throws {
        let original = Data("Hello, zstd compression round-trip!".utf8)
        let compressed = try original.compress(as: "test.txt", format: .tar, filters: [zstdSupport!])
        #expect(!compressed.isEmpty)

        let decompressed = try compressed.decompressArchive()
        #expect(decompressed["test.txt"] == original)
    }

    @Test func compressDecompressNoFilter() throws {
        let original = Data("Hello, no-filter round-trip!".utf8)
        let compressed = try original.compress(as: "test.txt", format: .tar, filters: [.none])
        #expect(!compressed.isEmpty)

        let decompressed = try compressed.decompressArchive()
        #expect(decompressed["test.txt"] == original)
    }

    @Test func archiveEntries() throws {
        let writer = try ArchiveWriter(format: .tar)
        let data1 = Data("one".utf8)
        let data2 = Data("two".utf8)
        try writer.writeEntry(ArchiveEntry(pathname: "a.txt", size: Int64(data1.count)), data: data1)
        try writer.writeEntry(ArchiveEntry(pathname: "b.txt", size: Int64(data2.count)), data: data2)
        let archiveData = try writer.finish()

        let entries = try archiveData.archiveEntries()
        #expect(entries.count == 2)
        #expect(entries[0].pathname == "a.txt")
        #expect(entries[1].pathname == "b.txt")
    }
}

// MARK: - Error Handling Tests

@Suite("Error Handling")
struct ErrorHandlingTests {

    @Test func corruptData() {
        let garbage = Data([0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE])
        #expect(throws: ArchiveError.self) {
            let reader = try ArchiveReader(data: garbage)
            _ = try reader.listEntries()
        }
    }

    @Test func finishOnFileWriter() throws {
        let tmpPath = NSTemporaryDirectory() + "archive_test_\(ProcessInfo.processInfo.globallyUniqueString).tar"
        defer { try? FileManager.default.removeItem(atPath: tmpPath) }

        let writer = try ArchiveWriter(path: tmpPath, format: .tar)
        let data = Data("test".utf8)
        try writer.writeEntry(ArchiveEntry(pathname: "f.txt", size: Int64(data.count)), data: data)
        #expect(throws: ArchiveError.self) {
            _ = try writer.finish()
        }
    }
}

// MARK: - Concurrency Tests

@Suite("Concurrency")
struct ConcurrencyTests {

    @Test func entryIsSendable() async {
        let entry = ArchiveEntry(pathname: "test.txt", size: 42)
        let task = Task { @Sendable in
            return entry.pathname
        }
        let result = await task.value
        #expect(result == "test.txt")
    }
}

// MARK: - File-Based Writer Tests

@Suite("File-Based Writing")
struct FileBasedWriterTests {

    @Test(.disabled(if: gzipSupport == nil))
    func writeToFileAndReadBackGzip() throws {
        let tmpPath = NSTemporaryDirectory() + "archive_test_\(ProcessInfo.processInfo.globallyUniqueString).tar.gz"
        defer { try? FileManager.default.removeItem(atPath: tmpPath) }

        let fileData = Data("file-based gzip writing test".utf8)
        let writer = try ArchiveWriter(path: tmpPath, format: .tar, filters: [gzipSupport!])
        try writer.writeEntry(
            ArchiveEntry(pathname: "file.txt", size: Int64(fileData.count)),
            data: fileData
        )
        try writer.close()

        let reader = try ArchiveReader(path: tmpPath)
        try reader.forEachEntry { entry, reader in
            #expect(entry.pathname == "file.txt")
            let data = try reader.readData()
            #expect(data == fileData)
        }
    }

    @Test(.disabled(if: xzSupport == nil))
    func writeToFileAndReadBackXz() throws {
        let tmpPath = NSTemporaryDirectory() + "archive_test_\(ProcessInfo.processInfo.globallyUniqueString).tar.xz"
        defer { try? FileManager.default.removeItem(atPath: tmpPath) }

        let fileData = Data("file-based xz writing test".utf8)
        let writer = try ArchiveWriter(path: tmpPath, format: .tar, filters: [xzSupport!])
        try writer.writeEntry(
            ArchiveEntry(pathname: "file.txt", size: Int64(fileData.count)),
            data: fileData
        )
        try writer.close()

        let reader = try ArchiveReader(path: tmpPath)
        try reader.forEachEntry { entry, reader in
            #expect(entry.pathname == "file.txt")
            let data = try reader.readData()
            #expect(data == fileData)
        }
    }

    @Test(.disabled(if: zstdSupport == nil))
        func writeToFileAndReadBackZstd() throws {
        let tmpPath = NSTemporaryDirectory() + "archive_test_\(ProcessInfo.processInfo.globallyUniqueString).tar.zst"
        defer { try? FileManager.default.removeItem(atPath: tmpPath) }

        let fileData = Data("file-based zstd writing test".utf8)
        let writer = try ArchiveWriter(path: tmpPath, format: .tar, filters: [zstdSupport!])
        try writer.writeEntry(
            ArchiveEntry(pathname: "file.txt", size: Int64(fileData.count)),
            data: fileData
        )
        try writer.close()

        let reader = try ArchiveReader(path: tmpPath)
        try reader.forEachEntry { entry, reader in
            #expect(entry.pathname == "file.txt")
            let data = try reader.readData()
            #expect(data == fileData)
        }
    }

    @Test func writeToFileAndReadBackUncompressed() throws {
        let tmpPath = NSTemporaryDirectory() + "archive_test_\(ProcessInfo.processInfo.globallyUniqueString).tar"
        defer { try? FileManager.default.removeItem(atPath: tmpPath) }

        let fileData = Data("file-based uncompressed writing test".utf8)
        let writer = try ArchiveWriter(path: tmpPath, format: .tar)
        try writer.writeEntry(
            ArchiveEntry(pathname: "file.txt", size: Int64(fileData.count)),
            data: fileData
        )
        try writer.close()

        let reader = try ArchiveReader(path: tmpPath)
        try reader.forEachEntry { entry, reader in
            #expect(entry.pathname == "file.txt")
            let data = try reader.readData()
            #expect(data == fileData)
        }
    }
}

// MARK: - Format Detection Tests

@Suite("Format Detection")
struct FormatDetectionTests {

    @Test func detectTarFormat() throws {
        let writer = try ArchiveWriter(format: .tar)
        let data = Data("detect me".utf8)
        try writer.writeEntry(ArchiveEntry(pathname: "d.txt", size: Int64(data.count)), data: data)
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        _ = try reader.listEntries()
        #expect(reader.format == .tar)
    }

    @Test func detectZipFormat() throws {
        let writer = try ArchiveWriter(format: .zip)
        let data = Data("detect me".utf8)
        try writer.writeEntry(ArchiveEntry(pathname: "d.txt", size: Int64(data.count)), data: data)
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        _ = try reader.listEntries()
        #expect(reader.format == .zip)
    }
}

// MARK: - Zip Edge Cases

@Suite("Zip Edge Cases")
struct ZipEdgeCaseTests {

    @Test func zipEmptyFile() throws {
        let writer = try ArchiveWriter(format: .zip)
        try writer.writeEntry(
            ArchiveEntry(pathname: "empty.txt", size: 0),
            data: Data()
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        try reader.forEachEntry { entry, reader in
            #expect(entry.pathname == "empty.txt")
            #expect(entry.size == 0)
            let data = try reader.readData()
            #expect(data.isEmpty)
        }
    }

    @Test func zipEmptyArchive() throws {
        let writer = try ArchiveWriter(format: .zip)
        let archiveData = try writer.finish()
        #expect(!archiveData.isEmpty)

        let reader = try ArchiveReader(data: archiveData)
        let entries = try reader.listEntries()
        #expect(entries.isEmpty)
    }

    @Test func zipDirectoryHierarchy() throws {
        let writer = try ArchiveWriter(format: .zip)
        // Create nested directory structure
        try writer.writeEntry(
            ArchiveEntry(pathname: "a/", fileType: .directory, permissions: 0o755)
        )
        try writer.writeEntry(
            ArchiveEntry(pathname: "a/b/", fileType: .directory, permissions: 0o755)
        )
        try writer.writeEntry(
            ArchiveEntry(pathname: "a/b/c/", fileType: .directory, permissions: 0o755)
        )
        let data = Data("deep file".utf8)
        try writer.writeEntry(
            ArchiveEntry(pathname: "a/b/c/deep.txt", size: Int64(data.count)),
            data: data
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var entries: [ArchiveEntry] = []
        var fileData: Data?
        try reader.forEachEntry { entry, reader in
            entries.append(entry)
            if entry.fileType == .regular {
                fileData = try reader.readData()
            }
        }
        #expect(entries.count == 4)
        #expect(entries[0].fileType == .directory)
        #expect(entries[0].pathname == "a/")
        #expect(entries[1].pathname == "a/b/")
        #expect(entries[2].pathname == "a/b/c/")
        #expect(entries[3].pathname == "a/b/c/deep.txt")
        #expect(fileData == data)
    }

    @Test func zipSymlink() throws {
        let writer = try ArchiveWriter(format: .zip)
        let targetData = Data("symlink target content".utf8)
        try writer.writeEntry(
            ArchiveEntry(pathname: "target.txt", size: Int64(targetData.count)),
            data: targetData
        )
        try writer.writeEntry(
            ArchiveEntry(pathname: "link.txt", fileType: .symbolicLink, symlinkTarget: "target.txt")
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var entries: [ArchiveEntry] = []
        try reader.forEachEntry { entry, _ in
            entries.append(entry)
        }
        #expect(entries.count == 2)
        #expect(entries[0].pathname == "target.txt")
        #expect(entries[0].fileType == .regular)
        #expect(entries[1].pathname == "link.txt")
        #expect(entries[1].fileType == .symbolicLink)
        #expect(entries[1].symlinkTarget == "target.txt")
    }

    @Test func zipSymlinkToDirectory() throws {
        let writer = try ArchiveWriter(format: .zip)
        try writer.writeEntry(
            ArchiveEntry(pathname: "realdir/", fileType: .directory, permissions: 0o755)
        )
        let data = Data("in real dir".utf8)
        try writer.writeEntry(
            ArchiveEntry(pathname: "realdir/file.txt", size: Int64(data.count)),
            data: data
        )
        try writer.writeEntry(
            ArchiveEntry(pathname: "linkdir", fileType: .symbolicLink, symlinkTarget: "realdir")
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var entries: [ArchiveEntry] = []
        try reader.forEachEntry { entry, _ in
            entries.append(entry)
        }
        let linkEntry = entries.first { $0.pathname == "linkdir" }
        #expect(linkEntry != nil)
        #expect(linkEntry?.fileType == .symbolicLink)
        #expect(linkEntry?.symlinkTarget == "realdir")
    }

    @Test func zipUnicodeFilenames() throws {
        let names = [
            "café/résumé.txt",
            "日本語/テスト.txt",
            "Ünïcödé/fîlé.txt",
            "emoji_🎉/party.txt",
        ]
        let writer = try ArchiveWriter(format: .zip)
        for name in names {
            let data = Data("content of \(name)".utf8)
            try writer.writeEntry(
                ArchiveEntry(pathname: name, size: Int64(data.count)),
                data: data
            )
        }
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var readNames: [String] = []
        try reader.forEachEntry { entry, _ in
            readNames.append(entry.pathname)
        }
        #expect(readNames == names)
    }

    @Test func zipLongFilename() throws {
        // 255-char filename (typical max for many filesystems)
        let longName = String(repeating: "a", count: 200) + ".txt"
        let data = Data("long name content".utf8)

        let writer = try ArchiveWriter(format: .zip)
        try writer.writeEntry(
            ArchiveEntry(pathname: longName, size: Int64(data.count)),
            data: data
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        try reader.forEachEntry { entry, reader in
            #expect(entry.pathname == longName)
            let readData = try reader.readData()
            #expect(readData == data)
        }
    }

    @Test func zipDeeplyNestedPath() throws {
        // path with many directory components
        let components = (0..<20).map { "d\($0)" }
        let dirPath = components.joined(separator: "/") + "/"
        let filePath = components.joined(separator: "/") + "/file.txt"
        let data = Data("deeply nested".utf8)

        let writer = try ArchiveWriter(format: .zip)
        try writer.writeEntry(
            ArchiveEntry(pathname: dirPath, fileType: .directory, permissions: 0o755)
        )
        try writer.writeEntry(
            ArchiveEntry(pathname: filePath, size: Int64(data.count)),
            data: data
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var foundFile = false
        try reader.forEachEntry { entry, reader in
            if entry.fileType == .regular {
                #expect(entry.pathname == filePath)
                let readData = try reader.readData()
                #expect(readData == data)
                foundFile = true
            }
        }
        #expect(foundFile)
    }

    @Test func zipManySmallFiles() throws {
        let count = 100
        var expected: [String: Data] = [:]
        let writer = try ArchiveWriter(format: .zip)
        for i in 0..<count {
            let name = "file_\(String(format: "%04d", i)).txt"
            let data = Data("content #\(i)".utf8)
            expected[name] = data
            try writer.writeEntry(
                ArchiveEntry(pathname: name, size: Int64(data.count)),
                data: data
            )
        }
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var actual: [String: Data] = [:]
        try reader.forEachEntry { entry, reader in
            actual[entry.pathname] = try reader.readData()
        }
        #expect(actual.count == count)
        for (name, data) in expected {
            #expect(actual[name] == data)
        }
    }

    @Test func zipPermissionsPreserved() throws {
        let perms: [UInt16] = [0o644, 0o755, 0o600, 0o444, 0o700]
        let writer = try ArchiveWriter(format: .zip)
        for (i, perm) in perms.enumerated() {
            let data = Data("file \(i)".utf8)
            try writer.writeEntry(
                ArchiveEntry(pathname: "f\(i).txt", size: Int64(data.count), permissions: perm),
                data: data
            )
        }
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var readPerms: [UInt16] = []
        try reader.forEachEntry { entry, _ in
            readPerms.append(entry.permissions)
        }
        #expect(readPerms == perms)
    }

    @Test func zipMixedEntryTypes() throws {
        let writer = try ArchiveWriter(format: .zip)

        // Directory
        try writer.writeEntry(
            ArchiveEntry(pathname: "project/", fileType: .directory, permissions: 0o755)
        )
        // Regular file
        let srcData = Data("print('hello')".utf8)
        try writer.writeEntry(
            ArchiveEntry(pathname: "project/main.py", size: Int64(srcData.count), permissions: 0o644),
            data: srcData
        )
        // Executable
        let scriptData = Data("#!/bin/sh\necho hi".utf8)
        try writer.writeEntry(
            ArchiveEntry(pathname: "project/run.sh", size: Int64(scriptData.count), permissions: 0o755),
            data: scriptData
        )
        // Symlink
        try writer.writeEntry(
            ArchiveEntry(pathname: "project/link.py", fileType: .symbolicLink, symlinkTarget: "main.py")
        )
        // Empty file
        try writer.writeEntry(
            ArchiveEntry(pathname: "project/.gitkeep", size: 0),
            data: Data()
        )

        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var entries: [ArchiveEntry] = []
        var fileContents: [String: Data] = [:]
        try reader.forEachEntry { entry, reader in
            entries.append(entry)
            if entry.fileType == .regular {
                fileContents[entry.pathname] = try reader.readData()
            }
        }

        #expect(entries.count == 5)

        let dir = entries[0]
        #expect(dir.pathname == "project/")
        #expect(dir.fileType == .directory)

        let src = entries[1]
        #expect(src.pathname == "project/main.py")
        #expect(src.permissions == 0o644)
        #expect(fileContents["project/main.py"] == srcData)

        let script = entries[2]
        #expect(script.pathname == "project/run.sh")
        #expect(script.permissions == 0o755)
        #expect(fileContents["project/run.sh"] == scriptData)

        let link = entries[3]
        #expect(link.pathname == "project/link.py")
        #expect(link.fileType == .symbolicLink)
        #expect(link.symlinkTarget == "main.py")

        let empty = entries[4]
        #expect(empty.pathname == "project/.gitkeep")
        #expect(fileContents["project/.gitkeep"]?.isEmpty == true)
    }

    @Test func zipBinaryContent() throws {
        // File with all possible byte values
        var binaryData = Data(count: 256)
        for i in 0..<256 {
            binaryData[i] = UInt8(i)
        }

        let writer = try ArchiveWriter(format: .zip)
        try writer.writeEntry(
            ArchiveEntry(pathname: "binary.bin", size: Int64(binaryData.count)),
            data: binaryData
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        try reader.forEachEntry { entry, reader in
            let readData = try reader.readData()
            #expect(readData == binaryData)
        }
    }

    @Test func zipFilenameWithSpaces() throws {
        let name = "path with spaces/file name.txt"
        let data = Data("spaced content".utf8)

        let writer = try ArchiveWriter(format: .zip)
        try writer.writeEntry(
            ArchiveEntry(pathname: name, size: Int64(data.count)),
            data: data
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        try reader.forEachEntry { entry, reader in
            #expect(entry.pathname == name)
            let readData = try reader.readData()
            #expect(readData == data)
        }
    }

    @Test func zipFilenameWithSpecialCharacters() throws {
        let names = [
            "file (1).txt",
            "file [2].txt",
            "file {3}.txt",
            "file #4.txt",
            "file @5.txt",
            "file & 6.txt",
        ]
        let writer = try ArchiveWriter(format: .zip)
        for name in names {
            let data = Data(name.utf8)
            try writer.writeEntry(
                ArchiveEntry(pathname: name, size: Int64(data.count)),
                data: data
            )
        }
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var readNames: [String] = []
        try reader.forEachEntry { entry, _ in
            readNames.append(entry.pathname)
        }
        #expect(readNames == names)
    }

    @Test func zipExtractWithSymlinks() throws {
        let writer = try ArchiveWriter(format: .zip)

        let fileData = Data("real file content".utf8)
        try writer.writeEntry(
            ArchiveEntry(pathname: "original.txt", size: Int64(fileData.count)),
            data: fileData
        )
        try writer.writeEntry(
            ArchiveEntry(pathname: "symlink.txt", fileType: .symbolicLink, symlinkTarget: "original.txt")
        )
        let archiveData = try writer.finish()

        let tmpDir = NSTemporaryDirectory() + "archive_zip_symlink_\(ProcessInfo.processInfo.globallyUniqueString)"
        defer { try? FileManager.default.removeItem(atPath: tmpDir) }

        let reader = try ArchiveReader(data: archiveData)
        try reader.extractAll(to: tmpDir)

        let fm = FileManager.default
        let originalPath = (tmpDir as NSString).appendingPathComponent("original.txt")
        let symlinkPath = (tmpDir as NSString).appendingPathComponent("symlink.txt")

        // Original file should exist with correct content
        let readOriginal = try Data(contentsOf: URL(fileURLWithPath: originalPath))
        #expect(readOriginal == fileData)

        // Symlink should exist and resolve to the same content
        var isDir: ObjCBool = false
        #expect(fm.fileExists(atPath: symlinkPath, isDirectory: &isDir))

        let attrs = try fm.attributesOfItem(atPath: symlinkPath)
        let type = attrs[.type] as? FileAttributeType
        #expect(type == .typeSymbolicLink)

        let dest = try fm.destinationOfSymbolicLink(atPath: symlinkPath)
        #expect(dest == "original.txt")
    }

    @Test func zipLargeFile() throws {
        // 2MB file to test zip with larger data
        let size = 2 * 1024 * 1024
        var largeData = Data(count: size)
        // Fill with a repeating pattern for compressibility
        for i in 0..<size {
            largeData[i] = UInt8((i * 7 + 13) & 0xFF)
        }

        let writer = try ArchiveWriter(format: .zip)
        try writer.writeEntry(
            ArchiveEntry(pathname: "large.bin", size: Int64(size)),
            data: largeData
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        try reader.forEachEntry { entry, reader in
            #expect(entry.pathname == "large.bin")
            let readData = try reader.readData()
            #expect(readData.count == size)
            #expect(readData == largeData)
        }
    }

    @Test func zipMultipleSymlinksChain() throws {
        let writer = try ArchiveWriter(format: .zip)
        let data = Data("base content".utf8)

        // file -> link1 -> link2 (chain of symlinks)
        try writer.writeEntry(
            ArchiveEntry(pathname: "base.txt", size: Int64(data.count)),
            data: data
        )
        try writer.writeEntry(
            ArchiveEntry(pathname: "link1.txt", fileType: .symbolicLink, symlinkTarget: "base.txt")
        )
        try writer.writeEntry(
            ArchiveEntry(pathname: "link2.txt", fileType: .symbolicLink, symlinkTarget: "link1.txt")
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var entries: [ArchiveEntry] = []
        try reader.forEachEntry { entry, _ in
            entries.append(entry)
        }
        #expect(entries.count == 3)
        #expect(entries[0].fileType == .regular)
        #expect(entries[1].fileType == .symbolicLink)
        #expect(entries[1].symlinkTarget == "base.txt")
        #expect(entries[2].fileType == .symbolicLink)
        #expect(entries[2].symlinkTarget == "link1.txt")
    }

    @Test func zipRelativeSymlink() throws {
        let writer = try ArchiveWriter(format: .zip)
        let data = Data("nested content".utf8)

        try writer.writeEntry(
            ArchiveEntry(pathname: "dir/", fileType: .directory, permissions: 0o755)
        )
        try writer.writeEntry(
            ArchiveEntry(pathname: "dir/subdir/", fileType: .directory, permissions: 0o755)
        )
        try writer.writeEntry(
            ArchiveEntry(pathname: "dir/real.txt", size: Int64(data.count)),
            data: data
        )
        // Symlink from subdir back up to parent's file
        try writer.writeEntry(
            ArchiveEntry(pathname: "dir/subdir/uplink.txt", fileType: .symbolicLink, symlinkTarget: "../real.txt")
        )
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var entries: [ArchiveEntry] = []
        try reader.forEachEntry { entry, _ in
            entries.append(entry)
        }
        let uplink = entries.first { $0.pathname == "dir/subdir/uplink.txt" }
        #expect(uplink != nil)
        #expect(uplink?.fileType == .symbolicLink)
        #expect(uplink?.symlinkTarget == "../real.txt")
    }

    @Test func zipDotFiles() throws {
        let dotFiles = [".hidden", ".gitignore", ".DS_Store", "dir/.env"]
        let writer = try ArchiveWriter(format: .zip)
        for name in dotFiles {
            let data = Data("dotfile \(name)".utf8)
            try writer.writeEntry(
                ArchiveEntry(pathname: name, size: Int64(data.count)),
                data: data
            )
        }
        let archiveData = try writer.finish()

        let reader = try ArchiveReader(data: archiveData)
        var readNames: [String] = []
        try reader.forEachEntry { entry, _ in
            readNames.append(entry.pathname)
        }
        #expect(readNames == dotFiles)
    }
}
