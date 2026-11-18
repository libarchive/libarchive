#!/bin/sh
set -eu

ID=$(docker build -q -f build/release/Dockerfile .)
if [ -z "$ID" ]; then
	echo "Failed to build docker image"
	exit 1
fi

release_archive=$(mktemp "${TMPDIR:-/tmp}/libarchive-release.XXXXXXXX")
trap 'rm -f "$release_archive"' 0

# Wait for the producer to succeed before extracting its output.
docker run "$ID" sh -c "tar -c -f - libarchive-*" > "$release_archive"
tar -x -f "$release_archive"
