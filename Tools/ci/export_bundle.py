#!/usr/bin/env python3
"""Export the repository as a single hand-off document (``project-eclipse.bundle``).

The bundle exists so the whole project can be read by a person or a tool that has only the
text: every tracked file's path, size, line count and contents, plus a header with the
census and the commit it was taken from. It is not an archive format - it is the repository
as plain text, in the spirit of the source files it contains.

Usage
    python3 Tools/ci/export_bundle.py
    python3 Tools/ci/export_bundle.py --output /tmp/eclipse.bundle
    python3 Tools/ci/export_bundle.py --check ../project-eclipse.bundle
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import pathlib
import subprocess
import sys
from dataclasses import dataclass, field
from typing import List, Optional, Sequence, Tuple

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
FORMAT = "eclipse.bundle/1"
DEFAULT_OUTPUT = REPO_ROOT.parent / "project-eclipse.bundle"

# Directories that are generated, vendored or otherwise not source.
EXCLUDED_DIRS = {
    ".git", ".vs", ".idea", ".vscode", "Binaries", "Build", "DerivedDataCache",
    "Intermediate", "Saved", "Plugins/Marketplace", "node_modules", "__pycache__",
    ".pytest_cache",
}

EXCLUDED_SUFFIXES = {".bundle", ".pyc", ".pyo", ".obj", ".pdb", ".dll", ".so", ".dylib"}

# Files that are large and machine-generated but still tracked; they are listed in the tree
# with their stats and skipped in the contents section.
SKIP_CONTENTS = {"Content/Eclipse/Data/VerticalSlice/.gitkeep"}


@dataclass
class Entry:
    relative: str
    size: int
    lines: int
    digest: str
    text: str


@dataclass
class BundleStats:
    files: int = 0
    bytes: int = 0
    lines: int = 0
    skipped_contents: List[str] = field(default_factory=list)


def excluded(relative: pathlib.Path) -> bool:
    parts = relative.parts
    for index in range(len(parts)):
        joined = "/".join(parts[: index + 1])
        if joined in EXCLUDED_DIRS:
            return True
    return relative.suffix in EXCLUDED_SUFFIXES


def collect() -> Tuple[List[Entry], BundleStats]:
    entries: List[Entry] = []
    stats = BundleStats()

    for path in sorted(REPO_ROOT.rglob("*")):
        if not path.is_file():
            continue

        relative = path.relative_to(REPO_ROOT)
        if excluded(relative):
            continue

        raw = path.read_bytes()
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            stats.skipped_contents.append(str(relative))
            continue

        stats.files += 1
        stats.bytes += len(raw)
        stats.lines += text.count("\n")

        entries.append(Entry(
            relative=str(relative),
            size=len(raw),
            lines=text.count("\n"),
            digest=hashlib.sha256(raw).hexdigest(),
            text=text,
        ))

    return entries, stats


def git_commit() -> str:
    try:
        return subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def render(entries: Sequence[Entry], stats: BundleStats) -> str:
    generated = dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    out: List[str] = []
    out.append("# PROJECT ECLIPSE - source hand-off")
    out.append(f"format: {FORMAT}")
    out.append(f"generated_at: {generated}")
    out.append(f"commit: {git_commit()}")
    out.append(f"stats: files={stats.files} bytes={stats.bytes} lines={stats.lines}")
    out.append("")
    out.append(f"tree: {len(entries)} file(s)")
    for entry in entries:
        out.append(f"  {entry.relative} ({entry.size} bytes, {entry.lines} lines)")

    for entry in entries:
        if entry.relative in SKIP_CONTENTS:
            continue
        out.append("")
        out.append(f"===== FILE: {entry.relative} ({entry.size} bytes, {entry.lines} lines, sha256 {entry.digest[:12]}) =====")
        out.append(entry.text.rstrip("\n"))

    out.append("")
    return "\n".join(out)


def check_bundle(path: pathlib.Path, entries: Sequence[Entry], stats: BundleStats) -> int:
    """Verify an existing bundle against the working tree, without writing anything."""
    if not path.exists():
        print(f"export_bundle: {path} does not exist", file=sys.stderr)
        return 2

    text = path.read_text(encoding="utf-8", errors="replace")
    missing = [entry.relative for entry in entries if f"===== FILE: {entry.relative} " not in text]
    problems = 0

    if missing:
        problems += len(missing)
        for relative in missing[:20]:
            print(f"export_bundle: missing from bundle: {relative}")

    expected_header = f"stats: files={stats.files} bytes={stats.bytes} lines={stats.lines}"
    if expected_header not in text:
        print(f"export_bundle: bundle header does not match the working tree (expected '{expected_header}')")
        problems += 1

    print(f"export_bundle: checked {len(entries)} file(s), {problems} problem(s).")
    return 1 if problems else 0


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Export the repository as a single text bundle.")
    parser.add_argument("--output", type=pathlib.Path, default=DEFAULT_OUTPUT, help="bundle path")
    parser.add_argument("--check", type=pathlib.Path, default=None, help="verify an existing bundle")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)

    entries, stats = collect()

    if args.check is not None:
        return check_bundle(args.check, entries, stats)

    document = render(entries, stats)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(document, encoding="utf-8")

    if not args.quiet:
        print(f"export_bundle: {stats.files} file(s), {stats.bytes} bytes, {stats.lines} lines")
        print(f"export_bundle: wrote {args.output} ({len(document.encode('utf-8'))} bytes)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
