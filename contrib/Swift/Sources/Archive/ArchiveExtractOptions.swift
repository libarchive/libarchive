import CArchive

/// Options controlling archive extraction behavior.
public struct ExtractOptions: OptionSet, Sendable {
    public let rawValue: Int32

    public init(rawValue: Int32) {
        self.rawValue = rawValue
    }

    /// Restore file permissions.
    public static let permissions = ExtractOptions(rawValue: ARCHIVE_EXTRACT_PERM)
    /// Restore file modification time.
    public static let time = ExtractOptions(rawValue: ARCHIVE_EXTRACT_TIME)
    /// Restore file ownership.
    public static let owner = ExtractOptions(rawValue: ARCHIVE_EXTRACT_OWNER)
    /// Restore ACLs.
    public static let acl = ExtractOptions(rawValue: ARCHIVE_EXTRACT_ACL)
    /// Restore extended attributes.
    public static let xattr = ExtractOptions(rawValue: ARCHIVE_EXTRACT_XATTR)
    /// Restore file flags (macOS/BSD).
    public static let fflags = ExtractOptions(rawValue: ARCHIVE_EXTRACT_FFLAGS)
    /// Do not overwrite existing files.
    public static let noOverwrite = ExtractOptions(rawValue: ARCHIVE_EXTRACT_NO_OVERWRITE)
    /// Do not overwrite newer files.
    public static let noOverwriteNewer = ExtractOptions(rawValue: ARCHIVE_EXTRACT_NO_OVERWRITE_NEWER)
    /// Refuse to extract absolute paths or paths with `..` components.
    public static let secure = ExtractOptions(rawValue: ARCHIVE_EXTRACT_SECURE_NODOTDOT | ARCHIVE_EXTRACT_SECURE_SYMLINKS | ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS)

    /// A reasonable default: permissions + time + security.
    public static let `default`: ExtractOptions = [.permissions, .time, .secure]
}
