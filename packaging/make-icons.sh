#!/bin/sh
# Regenerate the icon bitmaps from packaging/ncmail.svg:
#
#   packaging/ncmail.png         256x256, used by the AppImage and the .desktop file
#   packaging/windows/ncmail.ico 16/32/48/256, used by the Windows installer
#
# Needs rsvg-convert (librsvg) and python3 with Pillow. Each .ico frame is
# rendered from the SVG at its own size rather than downscaled, and the 256x256
# frame is stored as PNG -- Inno Setup embeds SetupIconFile frames into setup.exe
# uncompressed, so a raw frame there would add ~270 KB to the installer.
set -eu

HERE="$(CDPATH= cd "$(dirname "$0")" && pwd)"
SVG="$HERE/ncmail.svg"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

for sz in 16 32 48 256; do
	rsvg-convert -w "$sz" -h "$sz" "$SVG" -o "$TMP/$sz.png"
done

python3 - "$TMP" "$HERE/ncmail.png" "$HERE/windows/ncmail.ico" <<'EOF'
import io, struct, sys
from PIL import Image

tmp, png_out, ico_out = sys.argv[1], sys.argv[2], sys.argv[3]

Image.open(f"{tmp}/256.png").convert("RGBA").save(png_out)

def frame(sz):
    """Payload for one .ico frame: BMP below 256, PNG at 256.

    PNG frames need Vista or newer, and only pay off at 256 where a raw frame
    would be ~270 KB. Each size comes from its own SVG render, so nothing is
    resampled."""
    src = f"{tmp}/{sz}.png"
    if sz == 256:
        return open(src, "rb").read()
    buf = io.BytesIO()
    Image.open(src).convert("RGBA").save(buf, format="ICO", sizes=[(sz, sz)],
                                        bitmap_format="bmp")
    one = buf.getvalue()
    size, off = struct.unpack_from("<II", one, 6 + 8)
    return one[off:off + size]

sizes = (16, 32, 48, 256)
head = bytearray(struct.pack("<HHH", 0, 1, len(sizes)))
body = bytearray()
off = 6 + 16 * len(sizes)
for sz in sizes:
    data = frame(sz)
    head += struct.pack("<BBBBHHII", sz % 256, sz % 256, 0, 0, 1, 32,
                        len(data), off + len(body))
    body += data
open(ico_out, "wb").write(bytes(head + body))
print(f"{ico_out}: {len(head) + len(body)} bytes")
EOF

ls -l "$HERE/ncmail.png" "$HERE/windows/ncmail.ico"
