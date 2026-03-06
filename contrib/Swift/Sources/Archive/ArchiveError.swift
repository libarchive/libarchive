import CArchive

/// An error from libarchive operations.
public struct ArchiveError: Error, Sendable, CustomStringConvertible {
    /// The libarchive error code.
    public let code: Int32
    /// The human-readable error message.
    public let message: String

    public var description: String {
        "ArchiveError(\(code)): \(message)"
    }

    /// Creates an error from a libarchive archive pointer.
    internal init(archive: OpaquePointer) {
        self.code = Int32(archive_errno(archive))
        if let msg = archive_error_string(archive) {
            self.message = String(cString: msg)
        } else {
            self.message = "Unknown archive error"
        }
    }

    /// Creates an error with a custom message.
    internal init(code: Int32 = -1, message: String) {
        self.code = code
        self.message = message
    }
}
