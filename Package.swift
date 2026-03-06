// swift-tools-version: 6.1

import PackageDescription

let package = Package(
    name: "Archive",
    platforms: [.macOS(.v13), .iOS(.v15), .tvOS(.v15), .watchOS(.v10)],
    products: [
        .library(name: "Archive", targets: ["Archive"]),
    ],
    traits: [
        // On macOS, zlib and bzip2 are in the SDK. On Linux, dev packages
        // (liblzma-dev, libbz2-dev) must be installed separately, so compression
        // traits are not enabled by default.
        .trait(name: "GzipSupport", description: "Enable gzip compression (zlib)"),
        .trait(name: "Bzip2Support", description: "Enable bzip2 compression"),
        .trait(name: "LZMASupport", description: "Enable lzma compression (requires liblzma)"),
        .trait(name: "ZstdSupport", description: "Enable Zstandard compression (requires libzstd)"),
        .default(enabledTraits: [/*"GzipSupport"*/]),
    ],
    targets: [
        .target(
            name: "CArchive",
            dependencies: [
                .target(name: "Cliblzma", condition: .when(platforms: [.macOS, .linux, .android], traits: ["LZMASupport"])),
                .target(name: "Clibzstd", condition: .when(platforms: [.macOS, .linux, .android], traits: ["ZstdSupport"])),
            ],
            path: "libarchive",
            exclude: [ "test" ],
            publicHeadersPath: ".",
            cSettings: [
                .headerSearchPath("../contrib/android/include", .when(platforms: [.android])),
                .define("PLATFORM_CONFIG_H", to: "\"config_spm.h\""),
                .define("HAVE_ZLIB_H", .when(traits: ["GzipSupport"])),
                .define("HAVE_LIBZ", .when(traits: ["GzipSupport"])),
                .define("HAVE_BZLIB_H", .when(traits: ["Bzip2Support"])),
                .define("HAVE_LIBBZ2", .when(traits: ["Bzip2Support"])),
                .define("HAVE_LZMA_H", .when(traits: ["LZMASupport"])),
                .define("HAVE_LIBLZMA", .when(traits: ["LZMASupport"])),
                .define("HAVE_LZMA_STREAM_ENCODER_MT", .when(traits: ["LZMASupport"])),
                .define("HAVE_ZSTD_H", .when(traits: ["ZstdSupport"])),
                .define("HAVE_LIBZSTD", .when(traits: ["ZstdSupport"])),
                .define("HAVE_ZSTD_compressStream", .when(traits: ["ZstdSupport"])),
            ],
            linkerSettings: [
                .linkedLibrary("z", .when(traits: ["GzipSupport"])),
                .linkedLibrary("bz2", .when(traits: ["Bzip2Support"])),
                .linkedLibrary("lzma", .when(platforms: [.macOS, .linux, .android], traits: ["LZMASupport"])),
                .linkedLibrary("zstd", .when(platforms: [.macOS, .linux, .android], traits: ["ZstdSupport"])),
                .linkedLibrary("iconv", .when(platforms: [.macOS, .iOS, .tvOS, .watchOS, .visionOS])),
                .linkedLibrary("crypto", .when(platforms: [.linux])),
            ]
        ),
        .systemLibrary(
            name: "Cliblzma",
            path: "contrib/Swift/Sources/Cliblzma",
            pkgConfig: "liblzma",
            providers: [
                .brew(["xz"]),
                .apt(["liblzma-dev"])
            ]
        ),
        .systemLibrary(
            name: "Clibzstd",
            path: "contrib/Swift/Sources/Clibzstd",
            pkgConfig: "libzstd",
            providers: [
                .brew(["zstd"]),
                .apt(["libzstd-dev"])
            ]
        ),
        .target(
            name: "Archive",
            dependencies: ["CArchive"],
            path: "contrib/Swift/Sources/Archive"
        ),
        .testTarget(
            name: "ArchiveTests",
            dependencies: ["Archive"],
            path: "contrib/Swift/Tests/ArchiveTests"
        ),
    ]
)
