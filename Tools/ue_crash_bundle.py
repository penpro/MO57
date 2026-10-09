"""Read the crash reporter's upload bundle ("CR1") -- real crashes AND the in-game bug reports (same format).

Unreal's CrashReportClient (and MO57's bug report form, Plugins/MOFramework/.../MOBugReportBundle.cpp) POST a zlib-compressed body to the data router
(`[CrashReportClient] DataRouterUrl` in Config/DefaultEngine.ini). The site stores the body untouched as crash-dumps/<engine ver>/<date>/<uuid>.bin plus a
<uuid>.json holding the request metadata (receivedAt, clientIp, query, ...). This module opens a .bin.

Layout, after zlib inflate (Engine/Source/Runtime/CrashReportCore/Private/CrashUpload.cpp):
    'C' 'R' '1'
    directory name  : int32 length (>= 260), ANSI chars, zero padded      "UECC-Windows-<32 hex>_0000"
    file name       : same encoding                                       "<directory>.uecrash"
    int32 uncompressed size (of the whole inflated stream) , int32 file count
    per file        : int32 index, name (same encoding), int32 byte count, bytes

The bundle is UNTRUSTED input (anyone can POST to the endpoint): every length is bounded, inflate is capped, and nothing in it is executed or opened as
anything but bytes/text. XML is read with regexes, not an XML parser, so entity-expansion tricks have nothing to expand.

    python -I Tools/ue_crash_bundle.py <file.bin>       # print a summary
"""
from __future__ import annotations

import html
import json
import re
import struct
import sys
import zlib
from dataclasses import dataclass, field

MARKER = b"CR1"
MAX_INFLATED = 256 * 1024 * 1024
MAX_FILES = 1024
MAX_NAME = 4096


class BundleError(ValueError):
    """The bytes are not a well-formed bundle."""


@dataclass
class Bundle:
    directory: str
    file_name: str
    files: list = field(default_factory=list)  # [(name, bytes)]

    def get(self, name: str) -> bytes | None:
        for n, data in self.files:
            if n == name:
                return data
        return None

    def text(self, name: str) -> str:
        data = self.get(name)
        return data.decode("utf-8", errors="replace") if data is not None else ""

    def names(self) -> list:
        return [n for n, _ in self.files]


def _read_ansi(buf: bytes, pos: int) -> tuple[str, int]:
    if pos + 4 > len(buf):
        raise BundleError("truncated name length")
    (length,) = struct.unpack_from("<i", buf, pos)
    pos += 4
    if length < 0 or length > MAX_NAME or pos + length > len(buf):
        raise BundleError(f"bad name length {length}")
    raw = buf[pos:pos + length]
    pos += length
    return raw.split(b"\0", 1)[0].decode("latin-1"), pos


def parse_bundle(body: bytes) -> Bundle:
    """Inflate and split a bundle. Raises BundleError on anything malformed."""
    d = zlib.decompressobj()
    try:
        raw = d.decompress(body, MAX_INFLATED)
    except zlib.error as e:
        raise BundleError(f"not a zlib stream: {e}") from e
    if d.unconsumed_tail:
        raise BundleError("inflates to more than the size cap")
    if raw[:3] != MARKER:
        raise BundleError("missing the CR1 marker")
    pos = 3
    directory, pos = _read_ansi(raw, pos)
    file_name, pos = _read_ansi(raw, pos)
    if pos + 8 > len(raw):
        raise BundleError("truncated header")
    stored_size, count = struct.unpack_from("<ii", raw, pos)
    pos += 8
    if stored_size != len(raw):
        raise BundleError(f"header says {stored_size} bytes, stream has {len(raw)}")
    if not 0 <= count <= MAX_FILES:
        raise BundleError(f"implausible file count {count}")
    bundle = Bundle(directory, file_name)
    for i in range(count):
        if pos + 4 > len(raw):
            raise BundleError(f"truncated before file {i}")
        pos += 4  # stored index
        name, pos = _read_ansi(raw, pos)
        if pos + 4 > len(raw):
            raise BundleError(f"truncated size of '{name}'")
        (size,) = struct.unpack_from("<i", raw, pos)
        pos += 4
        if size < 0 or pos + size > len(raw):
            raise BundleError(f"truncated data of '{name}'")
        bundle.files.append((name, raw[pos:pos + size]))
        pos += size
    return bundle


def write_bundle(directory: str, files: list) -> bytes:
    """Build a bundle the same way the game does (for tests and tools that need to fabricate one)."""
    def ansi(text: str) -> bytes:
        b = text.encode("latin-1")
        length = max(len(b), 260)
        return struct.pack("<i", length) + b.ljust(length, b"\0")

    body = b"".join(struct.pack("<i", i) + ansi(n) + struct.pack("<i", len(d)) + d for i, (n, d) in enumerate(files))
    header = MARKER + ansi(directory) + ansi(directory + ".uecrash")
    total = len(header) + 8 + len(body)
    return zlib.compress(header + struct.pack("<ii", total, len(files)) + body)


# --- the CrashContext.runtime-xml -------------------------------------------------------------------------------------

_TAG = r"<{0}>(.*?)</{0}>"


def _unescape(text: str) -> str:
    return html.unescape(text)


def context_properties(xml: str) -> dict:
    """The flat <RuntimeProperties> children as {name: value}."""
    section = re.search(r"<RuntimeProperties>(.*?)</RuntimeProperties>", xml, re.S)
    props = {}
    if section:
        for name, value in re.findall(r"<([A-Za-z0-9_]+)>(.*?)</\1>", section.group(1), re.S):
            props[name] = _unescape(value.strip())
    return props


def context_fields(xml: str) -> dict:
    """The <GameData><Field name="..">..</Field> rows of a bug report as {name: value}."""
    return {_unescape(n): _unescape(v.strip()) for n, v in re.findall(r'<Field name="(.*?)">(.*?)</Field>', xml, re.S)}


def tag(xml: str, name: str) -> str:
    m = re.search(_TAG.format(re.escape(name)), xml, re.S)
    return _unescape(m.group(1).strip()) if m else ""


def context_xml(bundle: Bundle) -> str:
    return bundle.text("CrashContext.runtime-xml")


def is_bug_report(bundle: Bundle) -> bool:
    return context_properties(context_xml(bundle)).get("CrashType") == "BugReport"


def summarise(bundle: Bundle) -> dict:
    xml = context_xml(bundle)
    props = context_properties(xml)
    info = {
        "kind": "bugreport" if props.get("CrashType") == "BugReport" else "crash",
        "directory": bundle.directory,
        "files": {n: len(d) for n, d in bundle.files},
        "title": props.get("ErrorMessage", ""),
        "exe": props.get("ExecutableName", ""),
        "config": props.get("BuildConfiguration", ""),
        "engine_version": props.get("EngineVersion", ""),
        "machine": props.get("MachineId", "")[:12],
    }
    if info["kind"] == "bugreport":
        fields = context_fields(xml)
        info.update({
            "description": props.get("UserDescription", ""),
            "category": fields.get("Report.Category", ""),
            "contact": fields.get("Report.Contact", ""),
            "build": fields.get("Build.Version", ""),
            "commit": fields.get("Build.Commit", ""),
            "fields": fields,
        })
    return info


def main(argv: list) -> int:
    if len(argv) != 2:
        print(__doc__)
        return 2
    with open(argv[1], "rb") as f:
        bundle = parse_bundle(f.read())
    print(json.dumps(summarise(bundle), indent=2, default=str))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
