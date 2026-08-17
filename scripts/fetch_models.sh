#!/usr/bin/env bash
# Copyright 2026 Zhang
# SPDX-License-Identifier: BSL-1.0

set -Eeuo pipefail

readonly MODEL_REPOSITORY="https://gitlab.freedesktop.org/monado/utilities/hand-tracking-models.git"
readonly MODEL_COMMIT="37a8a81bc8f433ac6cbdf2471909d2bac74beca1"
readonly DETECTION_MODEL="grayscale_detection_160x160.onnx"
readonly DETECTION_SHA256="1f1a039a266e13dc186bb884430ebd9c8216bdda680ab08a533d4c671f27ed36"
readonly KEYPOINT_MODEL="grayscale_keypoint_jan18.onnx"
readonly KEYPOINT_SHA256="40c0daa598cedb993b54fff17685231b7465d6db342656c401b01c2029efd1d5"

usage() {
	printf 'Usage: %s [destination-directory]\n' "$0"
	printf 'Default destination: <project>/models\n'
}

die() {
	printf 'error: %s\n' "$*" >&2
	exit 1
}

if (($# > 1)); then
	usage >&2
	exit 2
fi

for command_name in git mktemp awk; do
	command -v "$command_name" >/dev/null 2>&1 || die "required command not found: $command_name"
done
git lfs version >/dev/null 2>&1 || die "Git LFS is required (install git-lfs and try again)"

if command -v sha256sum >/dev/null 2>&1; then
	sha256_file() { sha256sum -- "$1" | awk '{print $1}'; }
elif command -v shasum >/dev/null 2>&1; then
	sha256_file() { shasum -a 256 -- "$1" | awk '{print $1}'; }
else
	die "neither sha256sum nor shasum is available"
fi

script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
project_directory="$(cd -- "$script_directory/.." && pwd -P)"
destination="${1:-$project_directory/models}"
mkdir -p -- "$destination"
destination="$(cd -- "$destination" && pwd -P)"

temporary_parent="${TMPDIR:-/tmp}"
temporary_directory="$(mktemp -d "${temporary_parent%/}/mercury-handpose-models.XXXXXX")"
cleanup() {
	# mktemp supplied this path; the additional checks prevent a broad removal if
	# the variable is ever changed by a future edit.
	if [[ -n "${temporary_directory:-}" && "$temporary_directory" != "/" &&
	      -d "$temporary_directory" &&
	      "$(basename -- "$temporary_directory")" == mercury-handpose-models.* ]]; then
		rm -rf -- "$temporary_directory"
	fi
}
trap cleanup EXIT HUP INT TERM

checkout="$temporary_directory/repository"
git init -q "$checkout"
git -C "$checkout" remote add origin "$MODEL_REPOSITORY"
git -C "$checkout" lfs install --local --skip-smudge >/dev/null
GIT_LFS_SKIP_SMUDGE=1 git -C "$checkout" fetch --quiet --depth=1 origin "$MODEL_COMMIT"
GIT_LFS_SKIP_SMUDGE=1 git -C "$checkout" checkout --quiet --detach FETCH_HEAD
git -C "$checkout" lfs pull \
	--include="$DETECTION_MODEL,$KEYPOINT_MODEL" \
	--exclude=""

verify_model() {
	local filename="$1"
	local expected="$2"
	local source_path="$checkout/$filename"
	[[ -f "$source_path" ]] || die "model download is missing $filename"
	local actual
	actual="$(sha256_file "$source_path")"
	[[ "$actual" == "$expected" ]] || \
		die "SHA256 mismatch for $filename (expected $expected, got $actual)"
}

verify_model "$DETECTION_MODEL" "$DETECTION_SHA256"
verify_model "$KEYPOINT_MODEL" "$KEYPOINT_SHA256"

# The source files are verified first; install then verify the final copies too.
install -m 0644 "$checkout/$DETECTION_MODEL" "$destination/$DETECTION_MODEL"
install -m 0644 "$checkout/$KEYPOINT_MODEL" "$destination/$KEYPOINT_MODEL"
[[ "$(sha256_file "$destination/$DETECTION_MODEL")" == "$DETECTION_SHA256" ]] || \
	die "final verification failed for $DETECTION_MODEL"
[[ "$(sha256_file "$destination/$KEYPOINT_MODEL")" == "$KEYPOINT_SHA256" ]] || \
	die "final verification failed for $KEYPOINT_MODEL"

printf '\nDownloaded and verified model weights from commit %s:\n' "$MODEL_COMMIT"
printf '  %s\n  %s\n' "$destination/$DETECTION_MODEL" "$destination/$KEYPOINT_MODEL"
printf '\nNOTICE: the upstream model repository declares no explicit license.\n'
printf 'Review models/README.md and obtain any permissions your use requires.\n'
