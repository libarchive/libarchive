import CArchive

/// Supported archive compression filters.
public enum ArchiveFilter: Sendable {
    case none
	case compress
	case lz4
    #if GzipSupport
    case gzip
    #endif
    #if Bzip2Support
    case bzip2
    #endif
    #if LZMASupport
    case lzma
    case xz
    #endif
    #if ZstdSupport
    case zstd
    #endif

    /// Adds this filter to an archive write handle.
    internal func addWriteFilter(_ a: OpaquePointer) -> Int32 {
        switch self {
        case .none: return archive_write_add_filter_none(a)
		case .compress: return archive_write_add_filter_compress(a)
		case .lz4: return archive_write_add_filter_lz4(a)
        #if GzipSupport
        case .gzip: return archive_write_add_filter_gzip(a)
        #endif
        #if Bzip2Support
        case .bzip2: return archive_write_add_filter_bzip2(a)
        #endif
        #if LZMASupport
        case .lzma: return archive_write_add_filter_lzma(a)
        case .xz: return archive_write_add_filter_xz(a)
        #endif
        #if ZstdSupport
        case .zstd: return archive_write_add_filter_zstd(a)
        #endif
        }
    }
}
