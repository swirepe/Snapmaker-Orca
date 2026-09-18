#!/usr/bin/env bash

# Build a macOS release using only Apple Command Line Tools. This selects the
# Unix Makefiles CMake generator, so xcodebuild and the Xcode application are
# not required. All build_release_macos.sh flags except -x are accepted.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for arg in "$@"; do
    if [[ "$arg" == "-x" ]]; then
        echo "Error: -x selects Ninja Multi-Config and is not supported by the no-Xcode wrapper." >&2
        exit 2
    fi
done

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "Error: this script must be run on macOS." >&2
    exit 1
fi

for tool in cmake make clang++ codesign install_name_tool dsymutil lipo msgfmt; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Error: required tool '$tool' was not found in PATH." >&2
        if [[ "$tool" == "msgfmt" ]]; then
            echo "Install gettext (for example: brew install gettext)." >&2
        else
            echo "Install Apple Command Line Tools with: xcode-select --install" >&2
        fi
        exit 1
    fi
done

if ! xcrun --find clang >/dev/null 2>&1; then
    echo "Error: Apple Command Line Tools are not configured." >&2
    echo "Install them with: xcode-select --install" >&2
    exit 1
fi

export SLICER_CMAKE_GENERATOR="Unix Makefiles"
export SLICER_BUILD_TARGET="all"
export DEPS_CMAKE_GENERATOR="Unix Makefiles"

echo "Building macOS release without Xcode (Unix Makefiles)."
exec "$PROJECT_DIR/build_release_macos.sh" "$@"
