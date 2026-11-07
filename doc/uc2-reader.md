# Optional UltraCompressor II reader

This reader adds listing and extraction of historical UC2 archives through an
external `libuc2read` shared library. The adapter is BSD-2-Clause. The dependency
retains its existing LGPL-3.0-only license; no decoder is copied into libarchive.
The GPL compressor and rANS implementation are excluded from that dependency.
The build checks reject common explicit static-link overrides, but packagers
must ensure the selected binary is the shared reader. These checks do not audit
a replacement library or arbitrary toolchain flags. Support defaults to OFF and requires an explicit build option.

The reader handles historical Huffman/delta and Turbo/method-80 data, custom
master dictionaries, CP850 names and stored versions. Repeated paths appear in
stored order. It does not handle encryption, SAS/UC2SEA volume resolution, PCP,
or the revival's rANS and shared-chunk extensions. Historical checksums detect
some damage; they do not authenticate archives.

## Build and test

From this libarchive checkout, build the separately licensed dependency from
this public UC2 source commit:

```sh
git clone https://github.com/evvaletov/uc2.git uc2-source
git -C uc2-source checkout --detach 07bb648e348990e60ab1b6be721946a2cfa0c69a
cmake -S uc2-source/contrib/libuc2read -B uc2-reader \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PWD/uc2-prefix"
cmake --build uc2-reader
cmake --install uc2-reader
cmake -S . -B build-uc2 -DENABLE_UC2READ=ON -DENABLE_TEST=ON \
  -DCMAKE_PREFIX_PATH="$PWD/uc2-prefix"
cmake --build build-uc2 --target bsdtar libarchive_test
build-uc2/bin/libarchive_test -r "$PWD/libarchive/test" test_read_format_uc2
```

CMake rejects a missing dependency or headers that cannot link its reader API.
The Autotools switch is `--with-uc2read`. For a private Linux installation, set
`PKG_CONFIG_PATH` to the prefix's `lib/pkgconfig` or `lib64/pkgconfig` directory
and `LD_LIBRARY_PATH` to the corresponding library directory during configure
and execution. Autotools runs linked configure probes. An ordinary build without
the option acquires no UC2 dependency; explicit registration returns ARCHIVE_WARN.

## Limits and metadata

| Option | Default | Unit |
| --- | ---: | --- |
| `memory-limit` | 67108864 | bytes |
| `input-limit` | 4294967295 | bytes |
| `entries-limit` | 100000 | entries |
| `entry-limit` | 1073741824 | decoded bytes per file |
| `total-limit` | 8589934592 | cumulative decoded bytes |

Use positive decimal values, without signs or unit suffixes. For example:

```sh
bsdtar --options uc2:memory-limit=134217728,uc2:entry-limit=2147483648 -xf input.uc2
```

The memory budget covers decoder allocations, cached dictionaries and adapter
metadata/paths. Fixed adapter state (about 65 KiB), stdio and libarchive's own
allocations are outside it. Input and the current decoded entry use anonymous
temporary files; disk use can reach the input limit plus one entry limit. The
entry spool is released between files. Listing and skipping do not decode file
payloads. Limits fail explicitly, without silently truncating output; the UC2
format's 32-bit input and entry limits still apply.

DOS timestamps have no timezone. This adapter maps them to UTC using Gregorian
arithmetic without changing process-wide timezone state. Invalid timestamps map
to zero. The proposed format number is `0x110000`; its allocation and the optional
LGPL dependency remain subjects for maintainer review.

The reader spools the complete input stream, including trailing bytes; concatenated
archives are not supported. Limits apply independently: a collection with long
paths may reach the memory limit before the entry-count ceiling.

Composed paths are limited to 2,047 bytes and 64 directory components. An
invalid or over-limit hierarchy rejects the archive before exposing entries.
