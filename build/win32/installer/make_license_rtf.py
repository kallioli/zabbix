"""Render COPYING as the RTF the installer's licence dialog expects.

WixUI wants RTF, and the licence in the tree is plain text, so this is a build
step rather than a file to keep in sync by hand.

Usage: python make_license_rtf.py ../../../COPYING license.rtf
"""
import sys
from pathlib import Path

src, dst = Path(sys.argv[1]), Path(sys.argv[2])

text = src.read_text(encoding="utf-8", errors="replace")

# RTF is 7-bit: escape its three metacharacters, then push anything non-ASCII
# out as a \uN escape with a '?' fallback for readers that ignore it.
out = []
for ch in text:
    if ch in "\\{}":
        out.append("\\" + ch)
    elif ch == "\n":
        out.append("\\par\n")
    elif ch == "\r":
        continue
    elif ord(ch) < 128:
        out.append(ch)
    else:
        out.append(f"\\u{ord(ch)}?")

body = "".join(out)
rtf = (
    "{\\rtf1\\ansi\\ansicpg1252\\deff0"
    "{\\fonttbl{\\f0\\fnil\\fcharset0 Consolas;}}"
    "\\viewkind4\\uc1\\pard\\f0\\fs16\n"
    + body
    + "\n}"
)

dst.write_text(rtf, encoding="ascii", errors="replace", newline="\r\n")
print(f"{dst}: {len(rtf)} bytes from {src}")
