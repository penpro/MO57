"""
ue_json_utils.py - Read / modify Unreal Engine DataTable JSON exports.

UE exports DataTables as a JSON array of row objects:
    [
        {"Name": "RowName1", "Field": "Value", ...},
        {"Name": "RowName2", ...}
    ]

This utility manipulates those files safely. The key challenge is that UE's
JSON exporter uses tab indentation, CRLF line endings, no BOM, and an
"Allman-style" brace-on-new-line for nested objects:
    "PlacementData":
    {
        "field": ...
    }

Python's json.dumps doesn't match this style by default. For ADD operations
we do text-mode insertion (preserves original formatting exactly). For
parse-required operations (get / list / update / delete) we use json.loads
and json.dumps with tab indent — the output will diff visually from UE's
exporter but is identical JSON content that UE re-imports correctly.

Workflow:
    1. Edit JSON via this utility OR by hand
    2. In UE Editor, right-click the DataTable asset → Reimport JSON

Subcommands:
    list FILE                       — print all row Name keys
    get FILE ROWNAME                — print one row pretty-formatted
    add FILE JSON_BLOB              — insert a new row (JSON object as string)
    add-file FILE PAYLOAD_FILE      — insert a new row from a payload .json file
    delete FILE ROWNAME             — remove a row by Name
    update FILE ROWNAME FIELD VALUE — set Row[FIELD] = VALUE (parsed as JSON)
    has FILE ROWNAME                — exit 0 if row exists, 1 otherwise
"""

import argparse
import json
import re
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# File I/O — UE's exporter is inconsistent about encoding. Some files (Items)
# come out UTF-16 LE with BOM, others (Recipes) come out plain UTF-8. We
# sniff the BOM, decode, and on write preserve whatever encoding the file
# started in so the user's source control diffs stay clean.
# ---------------------------------------------------------------------------

def detect_encoding(raw: bytes) -> str:
    """Return a Python codec name based on BOM sniffing."""
    if raw.startswith(b"\xff\xfe"):
        return "utf-16-le"
    if raw.startswith(b"\xfe\xff"):
        return "utf-16-be"
    if raw.startswith(b"\xef\xbb\xbf"):
        return "utf-8-sig"
    return "utf-8"


# Cache the source encoding per file so write_text re-encodes correctly.
# Keyed by resolved absolute path string. Populated by read_text.
_ENCODING_CACHE: dict = {}


def read_text(path: Path) -> str:
    """Read whole file, auto-detecting UE's encoding (UTF-8 or UTF-16 LE)."""
    raw = path.read_bytes()
    encoding = detect_encoding(raw)
    _ENCODING_CACHE[str(path.resolve())] = encoding
    text = raw.decode(encoding)
    # The utf-16-le / utf-16-be codecs don't strip the BOM character — they
    # treat FF FE as data. Strip it manually. utf-8-sig handles it for us.
    if text and text[0] == "﻿":
        text = text[1:]
    return text


def write_text(path: Path, text: str) -> None:
    """Write back in the same encoding the file was originally in, with CRLF."""
    encoding = _ENCODING_CACHE.get(str(path.resolve()), "utf-8")

    # Normalize line endings to LF first so we don't double up if input has CRLF.
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = text.replace("\n", "\r\n")

    # utf-16-le and utf-8-sig need the BOM re-emitted when we write.
    if encoding == "utf-16-le":
        path.write_bytes(b"\xff\xfe" + text.encode("utf-16-le"))
    elif encoding == "utf-16-be":
        path.write_bytes(b"\xfe\xff" + text.encode("utf-16-be"))
    elif encoding == "utf-8-sig":
        path.write_bytes(b"\xef\xbb\xbf" + text.encode("utf-8"))
    else:
        path.write_bytes(text.encode("utf-8"))


def load_rows(path: Path) -> list:
    """Parse the JSON file as a list of row dicts."""
    raw = read_text(path)
    data = json.loads(raw)
    if not isinstance(data, list):
        raise SystemExit(f"Expected top-level JSON array in {path}, got {type(data).__name__}")
    return data


def find_row_index(rows: list, name: str) -> int:
    """Return index of row whose 'Name' field == name, or -1."""
    for i, row in enumerate(rows):
        if row.get("Name") == name or row.get("RowName") == name:
            return i
    return -1


# ---------------------------------------------------------------------------
# Serialization — match UE's tab-indented format.
# ---------------------------------------------------------------------------

def serialize_rows(rows: list) -> str:
    """Render a list-of-rows back to a string. Tab indent, UE-friendly."""
    return json.dumps(rows, indent="\t", ensure_ascii=False)


def serialize_row(row: dict) -> str:
    """Render one row indented as it would appear inside the top-level array.
    Used by `add` so the inserted block matches surrounding indentation.
    Output is one tab indented (top-level array element)."""
    inner = json.dumps(row, indent="\t", ensure_ascii=False)
    # Indent every line by one tab (so it sits at array-element depth).
    return "\n".join("\t" + line for line in inner.split("\n"))


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def cmd_list(args: argparse.Namespace) -> int:
    rows = load_rows(Path(args.file))
    for row in rows:
        name = row.get("Name") or row.get("RowName") or "<unnamed>"
        print(name)
    print(f"\n# {len(rows)} rows", file=sys.stderr)
    return 0


def cmd_get(args: argparse.Namespace) -> int:
    rows = load_rows(Path(args.file))
    idx = find_row_index(rows, args.rowname)
    if idx < 0:
        print(f"Row not found: {args.rowname}", file=sys.stderr)
        return 1
    print(json.dumps(rows[idx], indent="\t", ensure_ascii=False))
    return 0


def cmd_has(args: argparse.Namespace) -> int:
    rows = load_rows(Path(args.file))
    return 0 if find_row_index(rows, args.rowname) >= 0 else 1


def cmd_add(args: argparse.Namespace) -> int:
    """Insert via text-mode (preserves the file's exact existing formatting).

    Strategy: parse JSON to validate + find duplicates, then text-edit the
    file to insert the new block right before the final ']'. The existing
    rows keep their original formatting byte-for-byte; only the inserted
    block uses our serializer's format.
    """
    path = Path(args.file)
    payload_text = args.json_blob
    try:
        new_row = json.loads(payload_text)
    except json.JSONDecodeError as e:
        print(f"Invalid JSON blob: {e}", file=sys.stderr)
        return 1
    if not isinstance(new_row, dict):
        print("Payload must be a JSON object (one row).", file=sys.stderr)
        return 1

    rows = load_rows(path)
    new_name = new_row.get("Name") or new_row.get("RowName")
    if not new_name:
        print("New row must include a 'Name' field.", file=sys.stderr)
        return 1

    if find_row_index(rows, new_name) >= 0:
        print(f"Row '{new_name}' already exists. Use `update` or `delete` first.",
              file=sys.stderr)
        return 1

    raw = read_text(path)
    # Find the final ']' that closes the top-level array.
    last_bracket = raw.rfind("]")
    if last_bracket < 0:
        print("File has no closing ']' — not a valid array file.", file=sys.stderr)
        return 1

    # Walk backward from ']' to find the closing '}' of the previous row.
    # Whatever whitespace sits between '},' and ']' we preserve.
    head = raw[:last_bracket].rstrip()
    if not head.endswith("}"):
        print("Could not locate the previous row's closing brace.", file=sys.stderr)
        return 1

    serialized = serialize_row(new_row)
    new_content = head + ",\n" + serialized + "\n]" + raw[last_bracket + 1:]

    write_text(path, new_content)
    print(f"Added row '{new_name}' to {path.name} ({len(rows) + 1} rows total).",
          file=sys.stderr)
    return 0


def cmd_add_file(args: argparse.Namespace) -> int:
    payload_path = Path(args.payload_file)
    args.json_blob = read_text(payload_path)
    return cmd_add(args)


def cmd_delete(args: argparse.Namespace) -> int:
    """Remove a row. Uses parse + reserialize (file formatting may change)."""
    path = Path(args.file)
    rows = load_rows(path)
    idx = find_row_index(rows, args.rowname)
    if idx < 0:
        print(f"Row not found: {args.rowname}", file=sys.stderr)
        return 1
    rows.pop(idx)
    write_text(path, serialize_rows(rows))
    print(f"Deleted row '{args.rowname}' from {path.name} "
          f"({len(rows)} rows remain).", file=sys.stderr)
    return 0


def cmd_update(args: argparse.Namespace) -> int:
    """Set a single field on a single row. Value is parsed as JSON."""
    path = Path(args.file)
    rows = load_rows(path)
    idx = find_row_index(rows, args.rowname)
    if idx < 0:
        print(f"Row not found: {args.rowname}", file=sys.stderr)
        return 1
    try:
        parsed_value = json.loads(args.value)
    except json.JSONDecodeError:
        # Treat as a raw string if not valid JSON (so `update ... Field hello`
        # works without requiring `"hello"` quoting at the shell layer).
        parsed_value = args.value
    rows[idx][args.field] = parsed_value
    write_text(path, serialize_rows(rows))
    print(f"Updated {args.rowname}.{args.field} in {path.name}.", file=sys.stderr)
    return 0


# ---------------------------------------------------------------------------
# Argparse
# ---------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="ue_json_utils",
        description="Read / modify UE DataTable JSON exports.")
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("list", help="Print all row Name keys")
    sp.add_argument("file")
    sp.set_defaults(func=cmd_list)

    sp = sub.add_parser("get", help="Print one row pretty-formatted")
    sp.add_argument("file")
    sp.add_argument("rowname")
    sp.set_defaults(func=cmd_get)

    sp = sub.add_parser("has", help="Exit 0 if row exists, 1 otherwise")
    sp.add_argument("file")
    sp.add_argument("rowname")
    sp.set_defaults(func=cmd_has)

    sp = sub.add_parser("add", help="Insert a new row (JSON object as string)")
    sp.add_argument("file")
    sp.add_argument("json_blob", help='JSON object string, e.g. \'{"Name":"X",...}\'')
    sp.set_defaults(func=cmd_add)

    sp = sub.add_parser("add-file", help="Insert a new row from a payload .json file")
    sp.add_argument("file")
    sp.add_argument("payload_file")
    sp.set_defaults(func=cmd_add_file)

    sp = sub.add_parser("delete", help="Remove a row by Name")
    sp.add_argument("file")
    sp.add_argument("rowname")
    sp.set_defaults(func=cmd_delete)

    sp = sub.add_parser("update", help="Set Row[FIELD] = VALUE")
    sp.add_argument("file")
    sp.add_argument("rowname")
    sp.add_argument("field")
    sp.add_argument("value", help="JSON value, e.g. 5 or '\"text\"' or 'true'")
    sp.set_defaults(func=cmd_update)

    return p


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
