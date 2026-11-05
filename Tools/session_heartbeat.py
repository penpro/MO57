"""Heartbeat for the agent sessions sharing MO57's tree and its one build window: is anyone stuck waiting on someone who isn't moving?

    python Tools/session_heartbeat.py                 one check: prints the state and any stalemate; exit 0 clear, 3 stalemate, 2 can't tell
    python Tools/session_heartbeat.py --watch         a tick every --interval s (one line in the log); exits 3 with the report on a new stalemate
        [--session LABEL=SESSION_ID ...]              whose transcript shows that a coop author is working (LABEL: part of the author's name)
        [--self SESSION_ID]                           this session: its own activity proves nothing about the others
        [--stall-min 20] [--hung-min 15] [--snooze-min 30] [--interval 120] [--max-hours 24] [--coop FILE] [--state-dir DIR]

Wes, Nov 4: "there's two sub tasks fighting and timing out, pick one to actually do the things", then "create a heartbeat to check subprocesses
for stalemate conditions". Run --watch from the session that started the others with `run_in_background`: it costs nothing while all is well, and its
exit wakes that session, which picks one and stands the other down (Docs/TOOLING.md § Session heartbeat). It only reads and reports: it stops nothing.

What it reads, none of it written for this tool:
  - claude-codex-coop.md: CLAIM / RELEASE / WITHDRAW lines, and waits ("WAITING ... when CL-0356 releases", "the build window when CL-0355 releases").
  - The process table: builds, commandlets, editors, crash reporters, and shell loops that poll the coop log for a CL id.
  - Each session's transcript's modified time (never its contents: they run to a gigabyte).

Stalemates:
  CIRCULAR_WAIT       A waits on a claim of B, B waits on a claim of A.
  BLOCKED_ON_IDLE     someone waits on an open claim, its holder hasn't moved for --stall-min, and no build / commandlet is running.
  MISSED_WAKE         what a session waited for was released, and that session hasn't moved for --stall-min.
  WAITER_ON_RELEASED  a polling loop still runs though every claim it names is closed: its condition will never come true.
  WAITER_FLAPPING     a session keeps waking and re-arming a loop on the same claim (Nov 4: one matched its own coop line).
  HUNG_PROCESS        an Unreal process used under a second of CPU in --hung-min.
  LEFTOVER_REPORTER   a crash reporter outlived the process it watched: it holds binaries and UBT refuses to build (Docs/TRAPS.md).

An open UnrealEditor.exe (an editor, or a -game instance) never counts as work: it ticks whether anyone is using it or not, and "the holder left its
editor open and went quiet" is exactly what this looks for.
"""
from __future__ import annotations

import argparse
import datetime
import glob
import json
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass, field

ID = r"(?:CL|CX)-\d{3,5}"
HEADER_RE = re.compile(r"^### (\d{4}-\d{2}-\d{2})\b[^\n]*?\s(?:—|-{1,2}|�+)\s(.+?)\s(?:—|-{1,2}|�+)\s(" + ID + r")\s*$", re.M)
CLOSE_RE = re.compile(r"^(?:RELEASED?|WITHDRAW(?:N|S)?)\b", re.I)
CLAIM_RE = re.compile(r"^CLAIM\b", re.I)
# "when CL-0355 releases", "after CL-0231's RELEASE": says what ends the wait, so "after CL-0300 shipped" and "for CL-0356: the results" don't count.
WAIT_STRICT_RE = re.compile(r"\b(?:when|until|once|after)\s+(" + ID + r")(?:'s)?\s+(?:RELEASE|releases?|is released|stops?|has stopped|ends?|finishes|is done)\b", re.I)
WAIT_ON_RE = re.compile(r"\bwait(?:ing|s)?\s+(?:on|for)\s+(" + ID + r")\b", re.I)
# An entry that is a wait from its first words names its targets more loosely ("Queued after CL-0323's lab build").
WAIT_HEAD_RE = re.compile(r"^(?:WAITING|QUEUED|BLOCKED)\b|^" + ID + r"\s+(?:is waiting|waits)\b", re.I)
WAIT_LOOSE_RE = re.compile(r"\b(?:after|when|until|once|on|for|and)\s+(" + ID + r")\b", re.I)

QUERY_NAMES = (r"^(UnrealEditor|UnrealEditor-Cmd|UnrealBuildTool|UnrealAutomationTool|AutomationTool|ShaderCompileWorker|CrashReportClient|CrashReportClientEditor"
               r"|cl|cl-filter|clang-cl|link|link-filter|lld-link|blender|dotnet|ssh|scp|bash|sh|powershell|pwsh|python[\d.]*|MO57\w*|MO_WorldLab\w*)\.exe$")
INTERACTIVE_RE = re.compile(r"^(UnrealEditor|MO57\w*|MO_WorldLab\w*)\.exe$", re.I)  # ticks whether anyone uses it or not
BATCH_RE = re.compile(r"^(UnrealEditor-Cmd|UnrealBuildTool|UnrealAutomationTool|AutomationTool|ShaderCompileWorker|cl|cl-filter|clang-cl|link|link-filter|lld-link|blender)\.exe$", re.I)
UNREAL_RE = re.compile(r"^(UnrealEditor|UnrealEditor-Cmd|UnrealBuildTool|UnrealAutomationTool|AutomationTool|MO57\w*|MO_WorldLab\w*)\.exe$", re.I)
REPORTER_RE = re.compile(r"^CrashReportClient(Editor)?\.exe$", re.I)
SHELL_RE = re.compile(r"^(bash|sh|powershell|pwsh)\.exe$", re.I)
PYTHON_RE = re.compile(r"^python[\d.]*\.exe$", re.I)
LOOP_RE = re.compile(r"\b(until|while)\b|Start-Sleep")
SELF_RE = re.compile(r"session_heartbeat|Get-CimInstance")
BUSY_CORE_FRACTION = 0.02  # of one core since the last tick, for a batch process to count as working
HUNG_CPU_SECONDS = 1.0


@dataclass
class Config:
    stall_min: float = 20.0
    hung_min: float = 15.0
    waiter_grace_min: float = 3.0
    waiter_min_age_s: float = 60.0  # a loop younger than this may be a one-off command that happens to name a claim
    reporter_min: float = 5.0
    flap_count: int = 3
    flap_min: float = 60.0
    days: int = 3
    sessions: dict = field(default_factory=dict)  # author label part -> session id (prefix)
    self_id: str = ""


@dataclass
class Entry:
    id: str
    date: str
    author: str
    status: str
    refs: str
    order: int


@dataclass
class Wait:
    entry: Entry
    targets: list


@dataclass
class Proc:
    pid: int
    ppid: int
    name: str
    started: float
    cpu: float
    cmd: str


@dataclass
class Finding:
    kind: str
    key: str
    text: str
    advice: str

    def render(self) -> str:
        return f"[{self.kind}] {self.text}\n  Do: {self.advice}"


# ------------------------------------------------------------------ the coop log

def parse_coop(text: str, today: str = "", days: int = 3) -> list:
    """The log's entries of the last `days` days (headers give a date, not a time), oldest first; an entry with no Status line is skipped."""
    limit = None
    if today:
        limit = datetime.date.fromisoformat(today) - datetime.timedelta(days=days)
    heads = list(HEADER_RE.finditer(text))
    out = []
    for i, m in enumerate(heads):
        try:
            when = datetime.date.fromisoformat(m.group(1))
        except ValueError:
            continue
        if limit and when < limit:
            continue
        body = text[m.end():heads[i + 1].start() if i + 1 < len(heads) else len(text)]
        status = re.search(r"^Status:\s*(.*)$", body, re.M)
        if not status:
            continue
        refs = re.search(r"^References:\s*(.*)$", body, re.M)
        out.append(Entry(m.group(3), m.group(1), m.group(2).strip(), status.group(1).strip(), refs.group(1).strip() if refs else "", i))
    return out


def closed_ids(status: str) -> list:
    """The ids a RELEASE / WITHDRAW line closes: only those before its first ':' or sentence end. "RELEASE CL-0355 and CL-0357: ... (CL-0356 was waiting on
    it)" does not release CL-0356; a loop that read it that way woke the crop session by mistake on Nov 4."""
    head = re.split(r":|\.(?:\s|$)", status, maxsplit=1)[0]
    return re.findall(ID, head)


def wait_targets(status: str) -> list:
    if CLOSE_RE.match(status):
        return []
    if WAIT_HEAD_RE.match(status):
        own = re.match(ID, status)
        found = [t for t in WAIT_LOOSE_RE.findall(status) if not (own and t == own.group(0))]
    else:
        found = WAIT_STRICT_RE.findall(status) + WAIT_ON_RE.findall(status)
    return list(dict.fromkeys(found))


class Ledger:
    """Who holds what and who waits for what, from the entries."""

    def __init__(self, entries: list):
        self.entries = {e.id: e for e in entries}
        self.closed = {}  # id -> the order of the entry that closed it
        for e in entries:
            if not CLOSE_RE.match(e.status):
                continue
            ids = closed_ids(e.status)
            if not ids:  # "RELEASE (all stopped)": everything of that author's still open
                ids = [o.id for o in entries if o.order < e.order and o.author == e.author and CLAIM_RE.match(o.status) and o.id not in self.closed]
            for i in ids:
                self.closed.setdefault(i, e.order)
        self.holds = [e for e in entries if CLAIM_RE.match(e.status) and e.id not in self.closed]
        self.waits = []
        for e in entries:
            targets = [t for t in wait_targets(e.status) if t in self.entries]
            if not targets or e.id in self.closed:
                continue
            later = [o for o in entries if o.order > e.order and o.author == e.author]
            if any(e.id in o.status + " " + o.refs and not wait_targets(o.status) for o in later):
                continue  # the same author has moved on from it
            if all(t in self.closed for t in targets) and any(o.order > max(self.closed[t] for t in targets) for o in later):
                continue  # released, and its owner has written since: the wait is over
            self.waits.append(Wait(e, targets))

    def is_open(self, cid: str) -> bool:
        return cid in self.entries and cid not in self.closed


def find_cycles(ledger: Ledger) -> list:
    """Each cycle of authors waiting on each other's open claims, as [(author, its waiting entry id, target id), ...]."""
    edges = {}
    for w in ledger.waits:
        for t in w.targets:
            target = ledger.entries[t]
            if ledger.is_open(t) and target.author != w.entry.author:
                edges.setdefault(w.entry.author, []).append((target.author, w.entry.id, t))
    cycles, seen = [], set()

    def walk(author, path):
        for next_author, wait_id, target_id in edges.get(author, []):
            step = path + [(author, wait_id, target_id)]
            names = [s[0] for s in step]
            if next_author in names:
                loop = step[names.index(next_author):]
                key = frozenset(s[0] for s in loop)
                if key not in seen:
                    seen.add(key)
                    cycles.append(loop)
            else:
                walk(next_author, step)

    for a in sorted(edges):
        walk(a, [])
    return cycles


# ------------------------------------------------------------------ processes

def is_working(p: Proc) -> bool:
    """A process doing a job for a session, as opposed to one that merely stays open."""
    if BATCH_RE.match(p.name):
        return True
    if p.name.lower() == "dotnet.exe":
        return bool(re.search(r"UnrealBuildTool|AutomationTool", p.cmd))
    if PYTHON_RE.match(p.name):
        return bool(re.search(r"UEProjects|ue\.py|Tools[\\/]", p.cmd)) and not SELF_RE.search(p.cmd)
    return False


def is_remote_job(p: Proc) -> bool:
    return p.name.lower() in ("ssh.exe", "scp.exe") and "darktower" in p.cmd.lower()


def find_waiters(procs: list, now: float, cfg: Config) -> list:
    """[(Proc, [claim ids])]: shell loops polling for a claim. One loop shows up as several shells (Git Bash forks a twin with the same command line, and the
    tool's wrapper shell carries the loop's text too), so a shell whose parent is a loop, or whose twin is already counted, is the same loop."""
    found = []
    for p in procs:
        if not SHELL_RE.match(p.name) or SELF_RE.search(p.cmd) or not LOOP_RE.search(p.cmd) or now - p.started < cfg.waiter_min_age_s:
            continue
        targets = list(dict.fromkeys(re.findall(ID, p.cmd)))
        if targets:
            found.append((p, targets))
    pids = {p.pid for p, _ in found}
    out, seen = [], set()
    for p, targets in sorted(found, key=lambda f: f[0].pid):
        key = (p.cmd, round(p.started / 5))
        if p.ppid not in pids and key not in seen:
            seen.add(key)
            out.append((p, targets))
    return out


def sample_cpu(procs: list, state: dict, now: float) -> dict:
    """Updates state["cpu"] and returns {pid: CPU fraction of a core since the last tick}; a process first seen now has no entry."""
    last = state.get("cpu", {})
    rates, cpu = {}, {}
    for p in procs:
        old = last.get(str(p.pid))
        if old and abs(old["started"] - p.started) < 2 and now > old["ts"]:
            rates[p.pid] = (p.cpu - old["cpu"]) / (now - old["ts"])
            moved = p.cpu - old["anchor_cpu"] >= HUNG_CPU_SECONDS
            cpu[str(p.pid)] = {"started": p.started, "cpu": p.cpu, "ts": now, "anchor_cpu": p.cpu if moved else old["anchor_cpu"], "anchor_ts": now if moved else old["anchor_ts"]}
        else:
            cpu[str(p.pid)] = {"started": p.started, "cpu": p.cpu, "ts": now, "anchor_cpu": p.cpu, "anchor_ts": now}
    state["cpu"] = cpu
    return rates


# ------------------------------------------------------------------ the check

def detect(entries: list, procs: list, ages: dict, state: dict, now: float, cfg: Config) -> tuple:
    """(findings, one line of state). `ages` is {session id: seconds since its transcript changed}. Updates `state`; touches nothing else."""
    ledger = Ledger(entries)
    rates = sample_cpu(procs, state, now)
    stall = cfg.stall_min * 60
    work = [p for p in procs if is_remote_job(p) or (is_working(p) and rates.get(p.pid, 1.0) >= BUSY_CORE_FRACTION)]
    waiters = find_waiters(procs, now, cfg)

    def quiet_seconds(author: str) -> float:
        """How long nothing has shown this author at work (inf with no transcript to go by)."""
        ids = [sid for label, sid in cfg.sessions.items() if label.lower() in author.lower()]
        if ids:
            seen = [a for s, a in ages.items() if any(s.startswith(i) for i in ids)]
        else:
            seen = [a for s, a in ages.items() if not (cfg.self_id and s.startswith(cfg.self_id))]
        return min(seen) if seen else float("inf")

    def idle(author: str) -> bool:
        return not work and quiet_seconds(author) >= stall

    def mins(seconds: float) -> str:
        return "no sign of it" if seconds == float("inf") else f"{seconds / 60:.0f} min"

    findings = []
    for loop in find_cycles(ledger):
        chain = "; ".join(f"{a} ({w}) waits on {t}" for a, w, t in loop)
        findings.append(Finding("CIRCULAR_WAIT", "cycle:" + "+".join(sorted(s[0] for s in loop)), chain,
                                "Neither can move. Pick one to go first (the furthest along, or the one whose uncommitted edits are already in the shared tree); "
                                "tell the other to stop its loop and WITHDRAW its coop line."))
    blocked = {}  # target -> who waits on it
    for w in ledger.waits:
        for t in w.targets:
            if ledger.is_open(t) and ledger.entries[t].author != w.entry.author:
                blocked.setdefault(t, []).append(f"{w.entry.author} ({w.entry.id})")
    for p, targets in waiters:
        for t in targets:
            if ledger.is_open(t):
                blocked.setdefault(t, []).append(f"a polling loop (pid {p.pid})")
    for t, who in sorted(blocked.items()):
        holder = ledger.entries[t].author
        if idle(holder):
            findings.append(Finding("BLOCKED_ON_IDLE", "blocked:" + t, f"{', '.join(dict.fromkeys(who))} waits on {t}, held by {holder}, quiet for {mins(quiet_seconds(holder))}, and no build or commandlet is running.",
                                    f"Ask {holder} to finish and RELEASE {t}. If that session has ended, whoever waits may take over once it has checked that no process of the holder's runs."))
    for w in ledger.waits:
        if all(t in ledger.closed for t in w.targets) and idle(w.entry.author):
            findings.append(Finding("MISSED_WAKE", "wake:" + w.entry.id, f"{w.entry.author} ({w.entry.id}) was waiting on {', '.join(w.targets)}, which is released, and has been quiet for {mins(quiet_seconds(w.entry.author))}.",
                                    "Nudge it: it can go. If it has finished, it should RELEASE its own line."))

    # conditions that must last a while before they count
    first_seen = state.setdefault("first_seen", {})
    live_keys = set()

    def lasting(key: str, minutes: float) -> bool:
        live_keys.add(key)
        return now - first_seen.setdefault(key, now) >= minutes * 60

    for p, targets in waiters:
        known = [t for t in targets if t in ledger.entries]
        if known and all(t in ledger.closed for t in known) and lasting(f"gone:{p.pid}", cfg.waiter_grace_min):
            findings.append(Finding("WAITER_ON_RELEASED", f"gone:{p.pid}", f"pid {p.pid}, a loop running for {(now - p.started) / 60:.0f} min, polls for {', '.join(known)}, already closed.",
                                    "Its condition will never come true (it reads the coop log wrong). Have its session stop it and go by the real line."))
    for p in procs:
        if REPORTER_RE.match(p.name):
            m = re.search(r"-MONITOR=(\d+)", p.cmd, re.I)
            if m and int(m.group(1)) not in {q.pid for q in procs} and lasting(f"reporter:{p.pid}", cfg.reporter_min):
                findings.append(Finding("LEFTOVER_REPORTER", f"reporter:{p.pid}", f"{p.name} pid {p.pid} outlived the process it watched ({m.group(1)}): it can hold binaries, and UBT refuses while it runs.",
                                        "Close it by PID (Docs/TRAPS.md: the Live Coding / LNK1104 entry); never by name."))
    for key in [k for k in first_seen if k not in live_keys]:
        del first_seen[key]

    for p in procs:
        if UNREAL_RE.match(p.name):
            mark = state["cpu"][str(p.pid)]
            if now - mark["anchor_ts"] >= cfg.hung_min * 60:
                findings.append(Finding("HUNG_PROCESS", f"hung:{p.pid}:{int(p.started)}", f"{p.name} pid {p.pid} has used under {HUNG_CPU_SECONDS:.0f} s of CPU in {(now - mark['anchor_ts']) / 60:.0f} min. Its owner may be waiting on it.",
                                        "Find its owner in the coop log and have it look. Never force-kill by name; only the owner stops it, by its recorded PID."))

    # the same claim's loop started again and again
    history = [h for h in state.get("waiter_history", []) if now - h["started"] < cfg.flap_min * 60]
    for p, targets in waiters:
        for t in targets:
            if not any(h["target"] == t and abs(h["started"] - p.started) < 5 for h in history):
                history.append({"target": t, "started": p.started})
    state["waiter_history"] = history
    for t in sorted({h["target"] for h in history}):
        count = sum(h["target"] == t for h in history)
        if count >= cfg.flap_count:
            findings.append(Finding("WAITER_FLAPPING", "flap:" + t, f"{count} loops on {t} started in {cfg.flap_min:.0f} min: a session keeps waking and waiting again.",
                                    "Its condition probably matches the wrong line (its own coop entry, say). Stop polling: have the holder or the parent session tell it when to go."))

    summary = "holds=[{}] waits=[{}] loops=[{}] work=[{}]".format(
        " ".join(e.id for e in ledger.holds),
        " ".join(f"{w.entry.id}->{'+'.join(t + ('(open)' if ledger.is_open(t) else '(closed)') for t in w.targets)}" for w in ledger.waits),
        " ".join(f"{p.pid}->{'+'.join(ts)}" for p, ts in waiters),
        " ".join(f"{p.name.replace('.exe', '')}:{p.pid}" for p in work))
    return findings, summary


def fresh_findings(findings: list, state: dict, now: float, snooze_min: float) -> list:
    """Those not reported in the last snooze_min; one that cleared and came back is new again."""
    reported = {k: v for k, v in state.get("reported", {}).items() if any(f.key == k for f in findings)}
    fresh = [f for f in findings if now - reported.get(f.key, 0) >= snooze_min * 60]
    for f in fresh:
        reported[f.key] = now
    state["reported"] = reported
    return fresh


# ------------------------------------------------------------------ the machine

def query_processes() -> list:
    """Every process of interest, through CIM (for its command line and CPU time). This never touches a file's contents."""
    script = ("[Console]::OutputEncoding=[Text.Encoding]::UTF8; Get-CimInstance Win32_Process | Where-Object { $_.Name -match '" + QUERY_NAMES + "' } | ForEach-Object { "
              "[pscustomobject]@{pid=$_.ProcessId;ppid=$_.ParentProcessId;name=$_.Name;started=$(if ($_.CreationDate) { $_.CreationDate.ToUniversalTime().ToString('o') } else { '' });"
              "cpu=($_.KernelModeTime+$_.UserModeTime)/1e7;cmd=$_.CommandLine} } | ConvertTo-Json -Compress")
    run = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script], capture_output=True, timeout=90)
    if run.returncode != 0:
        raise RuntimeError(run.stderr.decode("utf-8", "replace")[:300])
    text = run.stdout.decode("utf-8", "replace").strip()
    rows = json.loads(text) if text else []
    rows = [rows] if isinstance(rows, dict) else rows
    me = {os.getpid(), os.getppid()}
    out = []
    for r in rows:
        if r["pid"] in me:
            continue
        stamp = (r.get("started") or "")[:26]
        started = datetime.datetime.fromisoformat(stamp).replace(tzinfo=datetime.timezone.utc).timestamp() if stamp else time.time()
        out.append(Proc(r["pid"], r.get("ppid") or 0, r["name"], started, float(r.get("cpu") or 0.0), r.get("cmd") or ""))
    return out


def session_ages(now: float, projects: str) -> dict:
    """{session id: seconds since its transcript changed}, for transcripts touched in the last day."""
    ages = {}
    for path in glob.glob(os.path.join(projects, "*", "*.jsonl")):
        try:
            age = now - os.path.getmtime(path)
        except OSError:
            continue
        if age < 86400:
            sid = os.path.basename(path)[:-len(".jsonl")]
            ages[sid] = min(age, ages.get(sid, age))
    return ages


def load_state(path: str) -> dict:
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def save_state(path: str, state: dict) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(state, f)
    os.replace(tmp, path)


def tick(args, cfg: Config, state: dict) -> tuple:
    now = time.time()
    with open(args.coop, encoding="utf-8", errors="replace") as f:
        entries = parse_coop(f.read(), datetime.date.today().isoformat(), cfg.days)
    return detect(entries, query_processes(), session_ages(now, args.projects), state, now, cfg)


def report(findings: list, summary: str) -> str:
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    head = f"HEARTBEAT STALEMATE at {stamp}: {len(findings)} finding(s)" if findings else f"HEARTBEAT clear at {stamp}"
    return "\n".join([head, "  " + summary] + [f.render() for f in findings])


def watch(args, cfg: Config) -> int:
    state_path = os.path.join(args.state_dir, "state.json")
    log_path = os.path.join(args.state_dir, "heartbeat.log")
    os.makedirs(args.state_dir, exist_ok=True)
    others = [p for p in query_processes() if PYTHON_RE.match(p.name) and "session_heartbeat" in p.cmd and "--watch" in p.cmd and p.ppid != os.getpid()]
    if others:
        print(f"HEARTBEAT not started: pid {others[0].pid} is already watching.")
        return 2
    state = load_state(state_path)
    end = time.time() + args.max_hours * 3600
    failures = 0
    while time.time() < end:
        try:
            findings, summary = tick(args, cfg, state)
            failures = 0
        except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as e:
            failures += 1
            if failures >= 3:  # silence must not look like all clear
                print(f"HEARTBEAT stopped: three checks in a row failed: {e}")
                return 2
            time.sleep(args.interval)
            continue
        fresh = fresh_findings(findings, state, time.time(), args.snooze_min)
        save_state(state_path, state)
        with open(log_path, "a", encoding="utf-8") as f:
            f.write(f"{datetime.datetime.now():%Y-%m-%d %H:%M:%S} {'STALEMATE ' + ','.join(x.kind for x in findings) if findings else 'clear'} {summary}\n")
        if fresh:
            print(report(fresh, summary))
            print("Re-arm with the same command once it is dealt with; these stay quiet for --snooze-min unless they clear and come back.")
            return 3
        time.sleep(args.interval)
    print(f"HEARTBEAT ended after {args.max_hours:g} h with nothing to report: re-arm it if sessions are still running.")
    return 0


def main(argv=None) -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description="Are the agent sessions stuck waiting on each other?")
    ap.add_argument("--watch", action="store_true", help="tick until a stalemate, then print it and exit 3")
    ap.add_argument("--session", action="append", default=[], metavar="LABEL=ID", help="whose transcript shows that coop author LABEL is working")
    ap.add_argument("--self", dest="self_id", default="", help="this session's id: its activity proves nothing about the others")
    ap.add_argument("--interval", type=float, default=120.0)
    ap.add_argument("--stall-min", type=float, default=20.0)
    ap.add_argument("--hung-min", type=float, default=15.0)
    ap.add_argument("--snooze-min", type=float, default=30.0)
    ap.add_argument("--max-hours", type=float, default=24.0)
    ap.add_argument("--coop", default=os.path.join(root, "claude-codex-coop.md"))
    ap.add_argument("--state-dir", default=os.path.join(root, "Saved", "SessionHeartbeat"))
    ap.add_argument("--projects", default=os.path.join(os.path.expanduser("~"), ".claude", "projects"))
    args = ap.parse_args(argv)
    sessions = dict(s.split("=", 1) for s in args.session)
    cfg = Config(stall_min=args.stall_min, hung_min=args.hung_min, sessions=sessions, self_id=args.self_id)
    if args.watch:
        return watch(args, cfg)
    # One check: never touches the watcher's state on disk. Two samples, for the CPU rates.
    state = load_state(os.path.join(args.state_dir, "state.json"))
    try:
        tick(args, cfg, state)
        time.sleep(5)
        findings, summary = tick(args, cfg, state)
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as e:
        print(f"HEARTBEAT can't tell: {e}")
        return 2
    print(report(findings, summary))
    return 3 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
