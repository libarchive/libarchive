@_exported import CArchive

/// Namespace for libarchive Swift wrapper.
public enum Archive {
    /// The version of the underlying libarchive C library.
    public static var version: String {
        String(cString: archive_version_string())
    }

    /// The detailed version of the underlying libarchive C library.
    public static var versionDetails: String {
        String(cString: archive_version_details())
    }
}
