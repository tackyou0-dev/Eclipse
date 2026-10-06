#!/usr/bin/env python3
"""Static lint for the PROJECT ECLIPSE C++ sources.

This is a *style and structure* linter, not a compiler. It exists because CI has no Unreal
Engine, and a surprising number of real defects are visible without one: a header without
``#pragma once``, a ``.generated.h`` that is not the last include (which breaks UHT in a way
that is painful to diagnose), unbalanced braces, a leftover TODO, a ``LogTemp`` call.

It also reports the file census (headers and sources), which is the number the project's
acceptance check quotes.

Exit codes
    0   no errors (warnings are reported but do not fail unless ``--strict``)
    1   at least one error, or a warning under ``--strict``

Usage
    python3 Tools/ci/lint_cpp.py
    python3 Tools/ci/lint_cpp.py --quiet
    python3 Tools/ci/lint_cpp.py --strict
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE_ROOT = REPO_ROOT / "Source" / "Eclipse"

# Sources with no header of their own. Both are deliberate: a console-command unit and the
# automation tests, neither of which is referenced from other translation units.
HEADERLESS_SOURCES = {
    "Private/Tools/EclipseContentTools.cpp",
    "Private/Tests/EclipseAutomationTests.cpp",
}

# Patterns that must never appear in shipped gameplay code.
FORBIDDEN_PATTERNS: Sequence[Tuple[str, str]] = (
    (r"\bLogTemp\b", "use a project log category (Core/EclipseLog.h) instead of LogTemp"),
    (r"\b(TODO|FIXME|XXX|HACK)\b", "unfinished work must be tracked as an issue, not a comment"),
    (r"\bGEngine->AddOnScreenDebugMessage\b", "screen debug messages do not belong in shipped code"),
    (r"\bstd::(cout|cerr|endl)\b", "use UE_LOG, not iostream"),
    (r"\busing namespace\b", "namespace pollution: spell the namespace out"),
    (r"\bsystem\(|\bexit\s*\(", "process control calls do not belong in a game module"),
)

MAX_REPORTED = 200


@dataclass
class LintReport:
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    headers: int = 0
    sources: int = 0

    def error(self, relative: str, line: int, message: str) -> None:
        self.errors.append(f"{relative}:{line}: error: {message}")

    def warn(self, relative: str, line: int, message: str) -> None:
        self.warnings.append(f"{relative}:{line}: warning: {message}")


def strip_comments(text: str, keep_strings: bool = False) -> str:
    """Blank out comments, preserving line structure.

    Checks for forbidden identifiers must not match a comment that *documents* the thing it
    forbids (the log header explains why LogTemp is banned, for example). String literals
    are blanked too unless ``keep_strings`` is set, because a check that inspects include
    paths needs the path text to survive.
    """
    out: List[str] = []
    index = 0
    length = len(text)

    while index < length:
        char = text[index]

        if char == "/" and index + 1 < length and text[index + 1] == "/":
            while index < length and text[index] != "\n":
                index += 1
            continue

        if char == "/" and index + 1 < length and text[index + 1] == "*":
            index += 2
            while index + 1 < length and not (text[index] == "*" and text[index + 1] == "/"):
                if text[index] == "\n":
                    out.append("\n")
                index += 1
            index += 2
            continue

        if not keep_strings and (char == '"' or char == "'"):
            quote = char
            out.append(" ")
            index += 1
            while index < length and text[index] != quote:
                if text[index] == "\\":
                    index += 1
                if index < length and text[index] == "\n":
                    out.append("\n")
                index += 1
            index += 1
            continue

        out.append(char)
        index += 1

    return "".join(out)


def check_balanced(relative: str, text: str, report: LintReport) -> None:
    """Brace / bracket / parenthesis balance, with a line number for the first offender."""
    pairs = {")": "(", "]": "[", "}": "{"}
    stack: List[Tuple[str, int]] = []
    line = 1

    for char in text:
        if char == "\n":
            line += 1
            continue

        if char in "([{":
            stack.append((char, line))
        elif char in pairs:
            if not stack or stack[-1][0] != pairs[char]:
                report.error(relative, line, f"unbalanced '{char}'")
                return
            stack.pop()

    if stack:
        char, open_line = stack[-1]
        report.error(relative, open_line, f"unclosed '{char}'")


def check_generated_include(relative: str, text: str, report: LintReport) -> None:
    """A reflected header must include its own .generated.h, as the last include."""
    reflects = re.search(r"\b(UCLASS|USTRUCT|UENUM|UINTERFACE|UDELEGATE)\s*\(", text) is not None
    includes = [line.strip() for line in text.splitlines() if line.strip().startswith("#include")]
    generated = [line for line in includes if ".generated.h" in line]
    expected = pathlib.Path(relative).stem + ".generated.h"

    if reflects and not generated:
        report.error(relative, 1, f"reflected header must include {expected}")
        return

    if not generated:
        return

    if expected not in generated[-1]:
        report.error(relative, 1, f"reflected header must include '{expected}', not '{generated[-1]}'")

    if includes and includes[-1] != generated[-1]:
        report.error(relative, 1, f"'{generated[-1]}' is not the last include in the file")


def check_source_pairing(relative: str, report: LintReport) -> None:
    if not relative.endswith(".cpp") or relative in HEADERLESS_SOURCES:
        return

    stem = pathlib.Path(relative).stem
    candidates = list((SOURCE_ROOT / "Public").rglob(stem + ".h"))
    if not candidates:
        report.error(relative, 1, f"no header found for {stem}.cpp")


def check_file(path: pathlib.Path, report: LintReport) -> None:
    relative = str(path.relative_to(SOURCE_ROOT))
    raw = path.read_text(encoding="utf-8", errors="replace")

    # Include checks need the quoted paths, so they run against comment-stripped text with
    # strings intact; identifier and balance checks run against text with both removed.
    include_text = strip_comments(raw, keep_strings=True)
    code_only = strip_comments(raw, keep_strings=False)

    if "\r" in raw:
        report.error(relative, 1, "file uses CRLF line endings")

    if raw and not raw.endswith("\n"):
        report.warn(relative, len(raw.splitlines()), "file does not end with a newline")

    if path.suffix == ".h" and "#pragma once" not in code_only:
        report.error(relative, 1, "header is missing #pragma once")

    for index, line in enumerate(raw.splitlines(), start=1):
        if line.rstrip() != line:
            report.warn(relative, index, "trailing whitespace")
        if line.startswith("    ") and line.strip():
            report.warn(relative, index, "indentation uses spaces; the project uses tabs")

    for pattern, message in FORBIDDEN_PATTERNS:
        for match in re.finditer(pattern, code_only):
            line = code_only.count("\n", 0, match.start()) + 1
            report.error(relative, line, message)
            break  # one report per rule per file is enough

    check_balanced(relative, code_only, report)
    check_generated_include(relative, include_text, report)
    check_source_pairing(relative, report)


def iter_sources() -> Iterable[pathlib.Path]:
    for pattern in ("*.h", "*.cpp"):
        for path in sorted(SOURCE_ROOT.rglob(pattern)):
            yield path


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Lint the Eclipse C++ sources.")
    parser.add_argument("--quiet", action="store_true", help="print only the summary line")
    parser.add_argument("--strict", action="store_true", help="treat warnings as errors")
    parser.add_argument("--max-report", type=int, default=MAX_REPORTED, help="cap on printed findings")
    args = parser.parse_args(argv)

    report = LintReport()

    for path in iter_sources():
        if path.suffix == ".h":
            report.headers += 1
        else:
            report.sources += 1
        check_file(path, report)

    for finding in report.errors[: args.max_report]:
        print(finding)
    for finding in report.warnings[: args.max_report]:
        print(finding)

    summary = (
        f"lint_cpp: {report.headers} headers, {report.sources} sources, "
        f"{len(report.errors)} errors, {len(report.warnings)} warnings."
    )
    print(summary)

    if report.errors or (args.strict and report.warnings):
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
