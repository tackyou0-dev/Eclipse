#!/usr/bin/env python3
"""Deploy the project's agent briefs.

"Deploying" an agent means handing it a complete, verifiable brief: the role file is up to
date, its owned paths exist, and the manifest records what was handed over and when. This
script does exactly that and nothing else - it does not talk to any external service.

Usage
    python3 scripts/deploy_subagents.py                 # refresh roles, write the manifest
    python3 scripts/deploy_subagents.py --check          # verify, write nothing
    python3 scripts/deploy_subagents.py --agent 04       # brief for one agent
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import pathlib
import subprocess
import sys
from typing import Dict, List, Optional, Sequence

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
AGENT_DIR = REPO_ROOT / ".agents" / "agents"
MANIFEST_PATH = REPO_ROOT / ".agents" / "manifest.json"
GENERATOR = REPO_ROOT / "scripts" / "generate_agents.py"


def ensure_roles_regenerated() -> int:
    """Run the generator so the briefs on disk match their source of truth."""
    return subprocess.run([sys.executable, str(GENERATOR)], cwd=REPO_ROOT, check=False).returncode


def load_manifest() -> Dict:
    if MANIFEST_PATH.exists():
        return json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    return {}


def owned_paths_ok(role_file: pathlib.Path) -> List[str]:
    """Return the own-path list from a role file that does not exist on disk.

    A role that owns a path nobody created is a role whose work has not started; that is
    worth reporting, not failing on.
    """
    missing: List[str] = []
    in_owns = False

    for line in role_file.read_text(encoding="utf-8").splitlines():
        if line.startswith("## "):
            in_owns = line.strip().lower().startswith("## owns")
            continue
        if not in_owns or not line.startswith("- `"):
            continue

        raw = line.strip()[3:].strip("`")
        if any(character in raw for character in "*{}"):
            continue
        if not (REPO_ROOT / raw).exists():
            missing.append(raw)

    return missing


def build_manifest(role_files: Sequence[pathlib.Path]) -> Dict:
    agents = []
    for path in role_files:
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        agents.append({
            "id": path.stem,
            "file": str(path.relative_to(REPO_ROOT)),
            "sha256": digest,
            "bytes": path.stat().st_size,
            "missing_owned_paths": owned_paths_ok(path),
        })

    return {
        "project": "PROJECT ECLIPSE",
        "generated_at": dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "agent_count": len(agents),
        "agents": agents,
    }


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Deploy the Eclipse agent briefs.")
    parser.add_argument("--check", action="store_true", help="verify without writing")
    parser.add_argument("--agent", default=None, help="print one agent's brief (for example 04)")
    args = parser.parse_args(argv)

    if not args.check:
        if ensure_roles_regenerated() != 0:
            print("deploy_subagents: the role generator failed", file=sys.stderr)
            return 2

    if not AGENT_DIR.exists():
        print("deploy_subagents: no .agents/agents directory; run scripts/generate_agents.py", file=sys.stderr)
        return 2

    role_files = sorted(AGENT_DIR.glob("*.md"))
    if len(role_files) != 10:
        print(f"deploy_subagents: expected 10 role files, found {len(role_files)}", file=sys.stderr)
        return 1

    if args.agent:
        wanted = args.agent.strip()
        matches = [path for path in role_files if path.stem.startswith(wanted)]
        if not matches:
            print(f"deploy_subagents: no agent matches '{wanted}'", file=sys.stderr)
            return 2
        print(matches[0].read_text(encoding="utf-8"))
        return 0

    manifest = build_manifest(role_files)

    if args.check:
        existing = load_manifest()
        problems = 0
        if existing.get("agent_count") != manifest["agent_count"]:
            print("deploy_subagents: manifest agent count differs from the role files")
            problems += 1
        known = {entry["id"]: entry["sha256"] for entry in existing.get("agents", [])}
        for entry in manifest["agents"]:
            if known.get(entry["id"]) != entry["sha256"]:
                print(f"deploy_subagents: manifest is stale for {entry['id']}")
                problems += 1
        print(f"deploy_subagents: checked {len(role_files)} brief(s), {problems} problem(s).")
        return 1 if problems else 0

    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    print(f"deploy_subagents: deployed {len(role_files)} brief(s) -> {MANIFEST_PATH.relative_to(REPO_ROOT)}")
    for entry in manifest["agents"]:
        note = ""
        if entry["missing_owned_paths"]:
            note = f"  (owned paths not created yet: {len(entry['missing_owned_paths'])})"
        print(f"  {entry['id']}{note}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
