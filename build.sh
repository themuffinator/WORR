#!/bin/bash
# WORR build script for Linux
# Uses builddir-linux (Windows builds use builddir-win)
# Usage: ./build.sh [options]
#
# Options:
#   --deps          Install build dependencies (apt/dnf) before building
#   --clean         Wipe builddir-linux and reconfigure from scratch
#   --stage         Run refresh_install.py after build to populate .install/
#   -h, --help      Show this help

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/builddir-linux"
INSTALL_DIR="${SCRIPT_DIR}/.install"
BASE_GAME="baseq2"

install_deps_apt() {
    sudo apt-get install -y meson gcc libc6-dev libsdl3-dev libopenal-dev \
        libpng-dev libjpeg-dev zlib1g-dev mesa-common-dev \
        libcurl4-gnutls-dev libx11-dev libxi-dev \
        libwayland-dev wayland-protocols libdecor-0-dev \
        libavcodec-dev libavformat-dev libavutil-dev \
        libswresample-dev libswscale-dev
}

install_deps_dnf() {
    sudo dnf install -y meson gcc glibc-devel SDL3-devel openal-soft-devel \
        libpng-devel libjpeg-turbo-devel zlib-devel mesa-libGL-devel \
        libcurl-devel libX11-devel libXi-devel \
        wayland-devel wayland-protocols-devel libdecor-devel \
        ffmpeg-devel
}

install_deps() {
    if command -v dnf &>/dev/null; then
        echo "Detected Fedora/RHEL, installing dependencies..."
        install_deps_dnf
    elif command -v apt-get &>/dev/null; then
        echo "Detected Debian/Ubuntu, installing dependencies..."
        install_deps_apt
    else
        echo "Unknown package manager. Please install dependencies manually. See BUILDING.md"
        exit 1
    fi
}

setup_build() {
    cd "$SCRIPT_DIR"
    if [[ -n "$CLEAN" ]]; then
        echo "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
    fi
    meson setup "$BUILD_DIR"
}

compile() {
    cd "$SCRIPT_DIR"
    meson compile -C "$BUILD_DIR"
}

stage_install() {
    cd "$SCRIPT_DIR"
    python3 tools/refresh_install.py \
        --build-dir "$BUILD_DIR" \
        --install-dir "$INSTALL_DIR" \
        --base-game "$BASE_GAME"
}

show_help() {
    sed -n '2,15p' "$0" | sed 's/^# \?//'
}

DO_DEPS=
CLEAN=
DO_STAGE=

while [[ $# -gt 0 ]]; do
    case "$1" in
        --deps)  DO_DEPS=1 ;;
        --clean) CLEAN=1 ;;
        --stage) DO_STAGE=1 ;;
        -h|--help) show_help; exit 0 ;;
        *) echo "Unknown option: $1"; show_help; exit 1 ;;
    esac
    shift
done

echo "Building WORR in $SCRIPT_DIR"
echo "----------------------------"

[[ -n "$DO_DEPS" ]] && install_deps
setup_build
compile

if [[ -n "$DO_STAGE" ]]; then
    stage_install
    echo ""
    echo "Staged build in $INSTALL_DIR"
fi

echo ""
echo "Build complete. Binaries are in $BUILD_DIR"
