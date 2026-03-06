import CArchive

/// Supported archive formats.
public enum ArchiveFormat: Sendable {
    case tar
    case zip
    case sevenZip
    case cpio
    case ar
    case iso9660
    case xar
    case raw
    case shar
    case mtree
    case warc

    /// The libarchive format code for writing.
    internal var writeFormatCode: Int32 {
        switch self {
        case .tar: return ARCHIVE_FORMAT_TAR
        case .zip: return ARCHIVE_FORMAT_ZIP
        case .sevenZip: return ARCHIVE_FORMAT_7ZIP
        case .cpio: return ARCHIVE_FORMAT_CPIO
        case .ar: return ARCHIVE_FORMAT_AR
        case .iso9660: return ARCHIVE_FORMAT_ISO9660
        case .xar: return ARCHIVE_FORMAT_XAR
        case .raw: return ARCHIVE_FORMAT_RAW
        case .shar: return ARCHIVE_FORMAT_SHAR
        case .mtree: return ARCHIVE_FORMAT_MTREE
        case .warc: return ARCHIVE_FORMAT_WARC
        }
    }

    /// Creates from a libarchive format code (masking sub-format bits).
    internal init?(rawFormat: Int32) {
        switch rawFormat & ARCHIVE_FORMAT_BASE_MASK {
        case ARCHIVE_FORMAT_TAR: self = .tar
        case ARCHIVE_FORMAT_ZIP: self = .zip
        case ARCHIVE_FORMAT_7ZIP: self = .sevenZip
        case ARCHIVE_FORMAT_CPIO: self = .cpio
        case ARCHIVE_FORMAT_AR: self = .ar
        case ARCHIVE_FORMAT_ISO9660: self = .iso9660
        case ARCHIVE_FORMAT_XAR: self = .xar
        case ARCHIVE_FORMAT_RAW: self = .raw
        case ARCHIVE_FORMAT_SHAR: self = .shar
        case ARCHIVE_FORMAT_MTREE: self = .mtree
        case ARCHIVE_FORMAT_WARC: self = .warc
        default: return nil
        }
    }

    /// Sets up the write format on an archive pointer.
    internal func setWriteFormat(_ a: OpaquePointer) -> Int32 {
        switch self {
        case .tar: return archive_write_set_format_pax_restricted(a)
        case .zip: return archive_write_set_format_zip(a)
        case .sevenZip: return archive_write_set_format_7zip(a)
        case .cpio: return archive_write_set_format_cpio_newc(a)
        case .ar: return archive_write_set_format_ar_svr4(a)
        case .iso9660: return archive_write_set_format_iso9660(a)
        case .xar: return archive_write_set_format_xar(a)
        case .raw: return archive_write_set_format_raw(a)
        case .shar: return archive_write_set_format_shar_dump(a)
        case .mtree: return archive_write_set_format_mtree(a)
        case .warc: return archive_write_set_format_warc(a)
        }
    }
}
