"""A local stand-in for the crash endpoint, so the bug report form can be tested end to end without touching the real site.

    python Tools/bugreport_receiver.py --port 8765 --out <dir> [--status 200] [--delay 0]

Stores each POST the way the real site does: <out>/<engine version>/<date>/<uuid>.bin (the raw body) and <uuid>.json (id, receivedAt, contentType,
contentLength, query). Prints one JSON line per request to stdout and answers --status (default 200) so the failure path can be exercised
(`--status 500`). Binds to 127.0.0.1 only. GET /health answers 200 (the harness polls it).

The game only talks to a loopback address when `MO.BugReport.EndpointOverride` is set (a cheat console variable; UMOBugReportSubsystem::IsAllowedEndpoint
refuses any other host for the override).
"""
from __future__ import annotations

import argparse
import datetime
import json
import os
import sys
import time
import uuid
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlparse

MAX_BODY = 16 * 1024 * 1024


def make_handler(out_dir: str, status: int, delay: float):
    class Handler(BaseHTTPRequestHandler):
        server_version = "MOBugReportReceiver/1"

        def log_message(self, fmt, *args):  # keep stdout for our JSON lines only
            pass

        def _reply(self, code: int, text: str):
            body = text.encode()
            self.send_response(code)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            self._reply(200, "ok")

        def do_POST(self):
            length = int(self.headers.get("Content-Length") or 0)
            if length <= 0 or length > MAX_BODY:
                self._reply(413 if length > MAX_BODY else 400, "bad length")
                return
            body = self.rfile.read(length)
            if delay:
                time.sleep(delay)
            query = parse_qs(urlparse(self.path).query)
            rid = str(uuid.uuid4())
            version = (query.get("AppVersion") or ["unknown"])[0]
            safe_version = "".join(c if c.isalnum() or c in ".-_" else "_" for c in version)[:80]
            day = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d")
            folder = os.path.join(out_dir, safe_version, day)
            os.makedirs(folder, exist_ok=True)
            with open(os.path.join(folder, rid + ".bin"), "wb") as f:
                f.write(body)
            meta = {
                "id": rid,
                "receivedAt": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                "contentType": self.headers.get("Content-Type"),
                "contentLength": length,
                "path": urlparse(self.path).path,
                "query": {k: v[0] for k, v in query.items()},
                "answered": status,
            }
            with open(os.path.join(folder, rid + ".json"), "w", encoding="utf-8") as f:
                json.dump(meta, f, indent=2)
            print(json.dumps(meta), flush=True)
            self._reply(status, "ok" if status < 300 else "error")

    return Handler


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--out", required=True)
    ap.add_argument("--status", type=int, default=200)
    ap.add_argument("--delay", type=float, default=0.0)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    server = HTTPServer(("127.0.0.1", args.port), make_handler(args.out, args.status, args.delay))
    print(json.dumps({"listening": f"http://127.0.0.1:{args.port}/", "out": args.out}), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
