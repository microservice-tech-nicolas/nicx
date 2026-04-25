#!/usr/bin/env sh
# nicx installer
# Usage: curl -fsSL https://raw.githubusercontent.com/microservice-tech-nicolas/nicx/main/scripts/install.sh | sh

set -e

REPO="microservice-tech-nicolas/nicx"
BINARY="nicx"
INSTALL_DIR="${NICX_INSTALL_DIR:-$HOME/.local/bin}"
BUILD_DIR="/tmp/nicx-build-$$"

# ── Colors ──────────────────────────────────────────────────────────────────
if [ -t 1 ]; then
    BOLD="\033[1m"; CYAN="\033[36m"; GREEN="\033[32m"
    YELLOW="\033[33m"; RED="\033[31m"; RESET="\033[0m"
else
    BOLD=""; CYAN=""; GREEN=""; YELLOW=""; RED=""; RESET=""
fi

info()    { printf "${CYAN}  →  ${RESET}%s\n" "$1"; }
success() { printf "${GREEN}  ✓  ${RESET}%s\n" "$1"; }
warn()    { printf "${YELLOW}  ⚠  ${RESET}%s\n" "$1"; }
die()     { printf "${RED}  ✗  ${RESET}%s\n" "$1" >&2; exit 1; }

printf "\n${BOLD}${CYAN}"
printf "  ███╗   ██╗██╗ ██████╗██╗  ██╗\n"
printf "  ████╗  ██║██║██╔════╝╚██╗██╔╝\n"
printf "  ██╔██╗ ██║██║██║      ╚███╔╝ \n"
printf "  ██║╚██╗██║██║██║      ██╔██╗ \n"
printf "  ██║ ╚████║██║╚██████╗██╔╝ ██╗\n"
printf "  ╚═╝  ╚═══╝╚═╝ ╚═════╝╚═╝  ╚═╝\n"
printf "${RESET}\n"
printf "  Installing nicx...\n\n"

# ── Detect OS & package manager ─────────────────────────────────────────────
detect_pkg_manager() {
    if command -v dnf  >/dev/null 2>&1; then echo "dnf";
    elif command -v apt >/dev/null 2>&1; then echo "apt";
    elif command -v pacman >/dev/null 2>&1; then echo "pacman";
    elif command -v zypper >/dev/null 2>&1; then echo "zypper";
    else echo "unknown"; fi
}

install_deps() {
    PM=$(detect_pkg_manager)
    info "Detected package manager: $PM"

    case "$PM" in
        dnf)    sudo dnf install -y gcc-c++ cmake ninja-build git ;;
        apt)    sudo apt-get install -y g++ cmake ninja-build git ;;
        pacman) sudo pacman -S --noconfirm gcc cmake ninja git ;;
        zypper) sudo zypper install -y gcc-c++ cmake ninja git ;;
        *)      warn "Unknown package manager — ensure g++, cmake, ninja, git are installed" ;;
    esac
}

# ── Check dependencies ───────────────────────────────────────────────────────
MISSING=""
for dep in git cmake ninja g++; do
    command -v $dep >/dev/null 2>&1 || MISSING="$MISSING $dep"
done

if [ -n "$MISSING" ]; then
    info "Installing missing dependencies:$MISSING"
    install_deps
fi

# ── Clone & build ────────────────────────────────────────────────────────────
info "Cloning repository..."
git clone --depth=1 "https://github.com/${REPO}.git" "$BUILD_DIR"

info "Building (Release)..."
cmake -S "$BUILD_DIR" -B "$BUILD_DIR/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
    -G Ninja -Wno-dev >/dev/null

cmake --build "$BUILD_DIR/build" --parallel >/dev/null

# ── Install ──────────────────────────────────────────────────────────────────
mkdir -p "$INSTALL_DIR"
cp "$BUILD_DIR/build/$BINARY" "$INSTALL_DIR/$BINARY"
chmod +x "$INSTALL_DIR/$BINARY"

rm -rf "$BUILD_DIR"
success "Installed to $INSTALL_DIR/$BINARY"

# ── PATH check ───────────────────────────────────────────────────────────────
case ":$PATH:" in
    *":$INSTALL_DIR:"*) ;;
    *)
        warn "$INSTALL_DIR is not in your PATH"
        printf "\n  Add this to your shell config (~/.zshrc or ~/.bashrc):\n"
        printf "\n    ${BOLD}export PATH=\"\$HOME/.local/bin:\$PATH\"${RESET}\n\n"
        ;;
esac

success "nicx installed successfully!"
printf "\n  Run: ${BOLD}nicx system info${RESET}\n\n"
