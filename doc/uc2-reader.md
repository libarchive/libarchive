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
git -C uc2-source checkout --detach bc6f9042f3275e18d0a1d3051bd70b4480bc7464
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
allocations are outside it. The current decoded entry uses an anonymous
temporary file, bounded by the entry and total output limits. It is released
between files. Seekable input uses libarchive callbacks without an input spool.
Listing and skipping do not decode file payloads. Limits fail explicitly, without silently truncating output; the UC2
format's 32-bit input and entry limits still apply.

DOS timestamps have no timezone. This adapter maps them to UTC using Gregorian
arithmetic without changing process-wide timezone state. Invalid timestamps map
to zero. The proposed format number is `0x110000`; its allocation and the optional
LGPL dependency remain subjects for maintainer review.

The reader requires seekable input, including when using stdin. A pipe or a
non-seekable filter fails before returning entries; save its output to a file
first. The input limit applies to the complete source length, including trailing
bytes. Concatenated archives are not supported. Limits apply independently: a
collection with long paths may reach the memory limit before the entry-count ceiling.

Composed paths are limited to 2,047 bytes and 64 directory components. An
invalid or over-limit hierarchy rejects the archive before exposing entries.

## Format references and scope

UC2 is a DOS-era archive format with a compressed central directory, stored file
versions and shared compression dictionaries. The directory identifies payloads
and dictionaries by absolute locations. Reading it and then returning to those
locations requires random access in this decoder.

The original structures are documented in
[`SUPERMAN.H`](https://github.com/evvaletov/uc2/blob/main/original/UC2_source/src/SUPERMAN.H)
and the accompanying implementation, preserved from the original source release.
The revival's [format guide](https://github.com/evvaletov/uc2/blob/main/docs/format.rst)
is a partial implementation reference, not a complete independent specification.
[Bobrowski's unuc2](https://torinak.com/~jb/unuc2/) is the portable reader on which
this dependency is based. Its 0.8 CLI requires a seekable archive: `-p` selects
standard output, while `-` is not a standard-input archive operand. Redirecting a
pipe through `/dev/stdin` fails at `fseek` on Linux. A subprocess producing decoded
file bytes would still need a separate archive metadata protocol, unlike a gzip
filter that returns one decompressed byte stream.

Decoded entries still use a temporary file because libuc2 exposes a synchronous
push decoder and libarchive requests blocks through a pull interface. The adapter
verifies the complete decoded entry before returning its first block. Removing
that spool requires a resumable decoder API or another bounded push/pull bridge;
this change does not claim to provide one.
