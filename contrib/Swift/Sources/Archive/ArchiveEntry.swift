import CArchive
import Foundation

// AE_IF* macros use C type casts that Swift can't import directly.
// Define them as Swift constants.
private let AE_IFREG: UInt32  = 0o100000
private let AE_IFDIR: UInt32  = 0o040000
private let AE_IFLNK: UInt32  = 0o120000
private let AE_IFBLK: UInt32  = 0o060000
private let AE_IFCHR: UInt32  = 0o020000
private let AE_IFIFO: UInt32  = 0o010000
private let AE_IFSOCK: UInt32 = 0o140000

/// The type of an archive entry.
public enum FileType: Sendable {
    case regular
    case directory
    case symbolicLink
    case hardLink
    case blockDevice
    case characterDevice
    case fifo
    case socket

    internal var cValue: UInt32 {
        switch self {
        case .regular: return AE_IFREG
        case .directory: return AE_IFDIR
        case .symbolicLink: return AE_IFLNK
        case .hardLink: return 0
        case .blockDevice: return AE_IFBLK
        case .characterDevice: return AE_IFCHR
        case .fifo: return AE_IFIFO
        case .socket: return AE_IFSOCK
        }
    }

    internal init?(cValue: UInt32) {
        switch cValue {
        case AE_IFREG: self = .regular
        case AE_IFDIR: self = .directory
        case AE_IFLNK: self = .symbolicLink
        case AE_IFBLK: self = .blockDevice
        case AE_IFCHR: self = .characterDevice
        case AE_IFIFO: self = .fifo
        case AE_IFSOCK: self = .socket
        default: return nil
        }
    }
}

/// A value type representing an entry (file/directory) in an archive.
public struct ArchiveEntry: Sendable {
    public var pathname: String
    public var size: Int64
    public var fileType: FileType
    /// POSIX permissions (e.g. 0o644).
    public var permissions: UInt16
    public var modificationDate: Date
    public var uid: UInt32
    public var gid: UInt32
    public var symlinkTarget: String?
    public var hardlinkTarget: String?

    public init(
        pathname: String,
        size: Int64 = 0,
        fileType: FileType = .regular,
        permissions: UInt16 = 0o644,
        modificationDate: Date = Date(),
        uid: UInt32 = 0,
        gid: UInt32 = 0,
        symlinkTarget: String? = nil,
        hardlinkTarget: String? = nil
    ) {
        self.pathname = pathname
        self.size = size
        self.fileType = fileType
        self.permissions = permissions
        self.modificationDate = modificationDate
        self.uid = uid
        self.gid = gid
        self.symlinkTarget = symlinkTarget
        self.hardlinkTarget = hardlinkTarget
    }

    /// Creates from a libarchive entry pointer.
    internal init?(entry: OpaquePointer?) {
        guard let entry = entry else { return nil }
        guard let pathPtr = archive_entry_pathname(entry) else { return nil }
        self.pathname = String(cString: pathPtr)
        self.size = archive_entry_size(entry)
        let ft = UInt32(archive_entry_filetype(entry))
        if let symTarget = archive_entry_symlink(entry), ft == AE_IFLNK {
            self.fileType = .symbolicLink
            self.symlinkTarget = String(cString: symTarget)
        } else if let hlTarget = archive_entry_hardlink(entry) {
            self.fileType = .hardLink
            self.hardlinkTarget = String(cString: hlTarget)
        } else {
            self.fileType = FileType(cValue: ft) ?? .regular
            self.symlinkTarget = nil
            self.hardlinkTarget = nil
        }
        self.permissions = UInt16(archive_entry_perm(entry))
        let mtime = archive_entry_mtime(entry)
        self.modificationDate = Date(timeIntervalSince1970: TimeInterval(mtime))
        self.uid = UInt32(archive_entry_uid(entry))
        self.gid = UInt32(archive_entry_gid(entry))
    }

    /// Applies this entry's metadata to a libarchive entry pointer.
    internal func apply(to entry: OpaquePointer) {
        archive_entry_set_pathname(entry, pathname)
        archive_entry_set_size(entry, size)
        archive_entry_set_filetype(entry, UInt32(fileType.cValue))
        #if os(Windows)
        archive_entry_set_perm(entry, UInt16(permissions))
        #else
        archive_entry_set_perm(entry, mode_t(permissions))
        #endif
        archive_entry_set_mtime(entry, time_t(modificationDate.timeIntervalSince1970), 0)
        archive_entry_set_uid(entry, Int64(uid))
        archive_entry_set_gid(entry, Int64(gid))
        if let target = symlinkTarget {
            archive_entry_set_symlink(entry, target)
        }
        if let target = hardlinkTarget {
            archive_entry_set_hardlink(entry, target)
        }
    }
}
