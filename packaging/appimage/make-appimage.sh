#!/bin/sh
# Build an "Anylinux" AppImage of ncmail with sharun + uruntime
# (https://github.com/pkgforge-dev/Anylinux-AppImages). The bundle carries its
# own C library and dynamic linker, so it runs on any distribution, including
# musl-based and very old ones -- the previous linuxdeploy build only ran on
# systems whose glibc was at least as new as the build host's.
#
# It also bundles the terminfo database: sharun points TERMINFO at
# $SHARUN_DIR/share/terminfo when that directory exists, so the AppImage no
# longer depends on the host having terminfo installed.
#
# Run it from a checkout, on Arch (or in an Arch container) with the packages
# listed in ../../.github/workflows/appimage.yml installed.
set -eu

ARCH="$(uname -m)"
HERE="$(CDPATH= cd "$(dirname "$0")" && pwd)"
SOURCE_ROOT="${SOURCE_ROOT:-$(CDPATH= cd "$HERE/../.." && pwd)}"
test -f "$SOURCE_ROOT/interfac/main.cc"

VERSION="${VERSION:-$(sed -n "s/^ *version: *'\([^']*\)'.*/\1/p" "$SOURCE_ROOT/meson.build" | head -1)}"

BUILD="${BUILD:-/tmp/ncmail-appimage-build}"
APPDIR="$BUILD/AppDir"
OUTPATH="${OUTPATH:-$SOURCE_ROOT/out}"

# Pinned so a rebuild of a given commit produces the same bundler.
QUICK_SHARUN_REV=242932b61e72816c9b826e9cf18d9d94cab31047
QUICK_SHARUN_URL="https://raw.githubusercontent.com/pkgforge-dev/Anylinux-AppImages/$QUICK_SHARUN_REV/useful-tools/quick-sharun.sh"

rm -rf "$BUILD"
mkdir -p "$BUILD" "$OUTPATH"

wget --retry-connrefused --tries=30 "$QUICK_SHARUN_URL" -O "$BUILD/quick-sharun"
chmod +x "$BUILD/quick-sharun"

# Static libstdc++/libgcc: the linker pulls in only the objects ncmail uses,
# which is a small fraction of the 2.9 MB shared library, so the bundle drops
# ~0.85 MB compressed. Only for this build -- distro packages should keep the
# system library. The GCC Runtime Library Exception allows the static link.
meson setup "$BUILD/bd" "$SOURCE_ROOT" \
	--buildtype=release \
	-Dstrip=true \
	-Dtests=false \
	-Dprefix=/usr \
	-Dcpp_link_args="['-static-libstdc++','-static-libgcc']"
ninja -C "$BUILD/bd"
test -x "$BUILD/bd/ncmail"

mkdir -p "$APPDIR/share/applications" "$APPDIR/share/pixmaps"
cp "$HERE/ncmail.desktop" "$APPDIR/share/applications/ncmail.desktop"
cp "$SOURCE_ROOT/packaging/ncmail.png" "$APPDIR/share/pixmaps/ncmail.png"

export APPDIR
export ICON="$APPDIR/share/pixmaps/ncmail.png"
export DESKTOP="$APPDIR/share/applications/ncmail.desktop"
export OUTPATH
export OUTNAME="NoCarrierMail-$VERSION-$ARCH.AppImage"
export MAIN_BIN=ncmail

# ncurses is a plain NEEDED link and nothing here dlopens a library, so skip
# quick-sharun's run-it-and-watch pass. It would also need a tty to survive.
export STRACE_MODE=0

if [ "$VERSION" = snapshot ]; then
	TAG=snapshot
else
	TAG=latest
fi
GH_OWNER=${GITHUB_REPOSITORY_OWNER:-andy5995}
export UPINFO="${UPINFO:-gh-releases-zsync|$GH_OWNER|NoCarrierMail|$TAG|*$ARCH.AppImage.zsync}"

cd "$BUILD"
./quick-sharun "$BUILD/bd/ncmail"

# quick-sharun copies the whole hicolor theme, assuming it is nearly empty as
# it is in a clean container. On a desktop it holds every installed app's
# icons, which added 21 MB here. A terminal program needs no icon theme; the
# one icon the AppImage shows comes from $ICON above.
rm -rf "$APPDIR/share/icons"

# quick-sharun bundles glibc's NSS modules because, in its words, it is hard to
# tell which apps need them. ncmail does not: it never calls getpwuid or resolves
# a name -- $HOME comes from getenv. The systemd NSS modules drag in the whole
# Kerberos stack, ~0.9 MB of the compressed image, so remove them. Keep
# libnss_files and libnss_dns, which cost 16 KB each and add no dependencies.
rm -f "$APPDIR"/lib/libnss_mymachines.so* "$APPDIR"/lib/libnss_resolve.so* \
	"$APPDIR"/lib/libkrb5* "$APPDIR"/lib/libgssapi_krb5* \
	"$APPDIR"/lib/libk5crypto* "$APPDIR"/lib/libkeyutils* \
	"$APPDIR"/lib/libcom_err* "$APPDIR"/lib/libtirpc* "$APPDIR"/lib/libnsl.so*

./quick-sharun --make-appimage

cd "$OUTPATH"
sha256sum "$OUTNAME" > "$OUTNAME.sha256sum"
cat "$OUTNAME.sha256sum"
