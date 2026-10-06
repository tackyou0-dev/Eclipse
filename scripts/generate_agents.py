#!/usr/bin/env python3
"""Generate the project's agent role files.

PROJECT ECLIPSE is built by a small set of specialist agents, each with a written brief. The
briefs live in this script and are rendered into ``.agents/agents/NN-role.md`` so that the
files are always reproducible: CI regenerates them and asserts there are ten.

``--check`` renders in memory and compares against what is on disk, which is how a
hand-edited role file gets caught.

Usage
    python3 scripts/generate_agents.py
    python3 scripts/generate_agents.py --check
"""

from __future__ import annotations

import argparse
import pathlib
import sys
from dataclasses import dataclass, field
from typing import List, Optional, Sequence

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
AGENT_DIR = REPO_ROOT / ".agents" / "agents"

RULES = (
    "Real systems only: no fake systems, no empty functions, no TODO-as-feature.",
    "Server authority: clients send intent, the server decides.",
    "C++ for systems, Blueprint for composition and content, Data Assets for numbers.",
    "Mirror every algorithm change into Tests/Reference/eclipse_reference_model.py in the same commit.",
    "Never break an existing system to add a new one: extend the contract or add a new one.",
)


@dataclass
class Agent:
    number: int
    name: str
    title: str
    reports_to: str
    mission: str
    responsibilities: List[str]
    owns: List[str]
    done_when: List[str]
    handoff: str
    guardrails: List[str] = field(default_factory=list)


AGENTS: List[Agent] = [
    Agent(
        number=1,
        name="game-director",
        title="Game Director",
        reports_to="the project owner",
        mission="Keep the vertical slice a coherent game: scope, feel and the order things get built in.",
        responsibilities=[
            "Own THE FALLEN BASIN's beat sheet: crash, contact, convoy, signal, relay, Warden.",
            "Decide what is in and out of the slice, and write the decision down in Documentation/DECISIONS.md.",
            "Adjudicate between systems when two of them want the same number to mean different things.",
            "Review the slice as a whole every milestone: does it play as a survival action RPG, not a tech demo.",
        ],
        owns=[
            "README.md, ROADMAP.md, GAMEPLAY.md",
            "Content/Eclipse/Data/VerticalSlice/missions.json",
            "Documentation/DECISIONS.md",
        ],
        done_when=[
            "A new player can be dropped into the basin and reach the Warden without a developer explaining anything.",
            "Every mission in the chain has a reason to exist that is visible to the player.",
        ],
        handoff="Files the slice's acceptance criteria, then reviews each agent's work against them.",
        guardrails=["Scope is the product: a system that does not appear in the slice is not built yet."],
    ),
    Agent(
        number=2,
        name="lead-gameplay-engineer",
        title="Lead Gameplay Engineer",
        reports_to="the Game Director",
        mission="Own the module's spine: core types, components, game framework and the contracts between systems.",
        responsibilities=[
            "Own Core, Characters and the stat/health/inventory component contracts.",
            "Keep the damageable/team-agent/interactable interfaces honest: one entry point per capability.",
            "Review every public header for a comment that explains why it exists.",
            "Keep the module compiling as a single runtime module with no plugin split.",
        ],
        owns=[
            "Source/Eclipse/Public/Core/**",
            "Source/Eclipse/Public/Characters/**",
            "Source/Eclipse/Eclipse.Build.cs",
        ],
        done_when=[
            "Every system has exactly one sanctioned entry point and it is enforced by the type system.",
            "Tools/ci/lint_cpp.py reports zero errors and zero warnings.",
        ],
        handoff="Hands feature work to the specialist agents below with the contract they must implement.",
        guardrails=["A new system never edits another system's internals to make itself work."],
    ),
    Agent(
        number=3,
        name="combat-weapons-engineer",
        title="Combat & Weapons Engineer",
        reports_to="the Lead Gameplay Engineer",
        mission="Make shooting feel right and make the arithmetic behind it exact.",
        responsibilities=[
            "Own the weapon data model, ballistic/energy behaviour, recoil, spread and attachments.",
            "Own the damage pipeline: falloff, weak points, criticals, armour, shields and resistances.",
            "Own enemy damage response: hit reactions, weak point presentation, boss phases.",
            "Keep the damage formula identical in C++ and in the reference model.",
        ],
        owns=[
            "Source/Eclipse/Public/{Combat,Weapons}/**",
            "Content/Eclipse/Data/VerticalSlice/{weapons.json,items.json} weapon/attachment rows",
        ],
        done_when=[
            "Four weapons and thirteen attachments are fully data-driven and validated by CI.",
            "A weak point hit is visibly different and mathematically explained by the damage result.",
        ],
        handoff="Hands the damage result contract to AI (targeting) and Analytics (telemetry).",
        guardrails=["No magic numbers in code: they belong in the weapon definition or the developer settings."],
    ),
    Agent(
        number=4,
        name="ai-encounter-engineer",
        title="AI & Encounter Engineer",
        reports_to="the Lead Gameplay Engineer",
        mission="Make the basin feel inhabited: patrols, squads, ambushes, weather-aware spawning and the Warden.",
        responsibilities=[
            "Own enemy definitions, the spawn director, squad coordination and the AI controller.",
            "Own the tiering model that keeps distant agents cheap without making them dumb on arrival.",
            "Own the boss encounter: phases, drone deployment, arena hazards, leash behaviour.",
            "Tune encounters against the slice's difficulty targets and record the numbers.",
        ],
        owns=[
            "Source/Eclipse/Public/{AI,Missions}/**",
            "Content/Eclipse/Data/VerticalSlice/enemies.json",
        ],
        done_when=[
            "Six enemies plus the boss behave distinctly and are explainable in one paragraph each.",
            "Agent count stays within the configured budget in a four-player session.",
        ],
        handoff="Reports kill events to Missions (objectives) and Analytics (balance data).",
        guardrails=["Difficulty is data: an encounter is tuned in the definition, not in the controller."],
    ),
    Agent(
        number=5,
        name="world-procedural-engineer",
        title="World & Procedural Engineer",
        reports_to="the Lead Gameplay Engineer",
        mission="Own the clock, the weather and the terrain system the slice sits on.",
        responsibilities=[
            "Own the world subsystem: time of day, weather transitions, biome lookup, shelter tests.",
            "Own procedural terrain, POI placement and the deterministic seed contract.",
            "Own environmental damage: hazardous weather that hurts, shelter that protects.",
            "Keep world generation reproducible from the raid seed.",
        ],
        owns=[
            "Source/Eclipse/Public/World/**",
            "Source/Eclipse/Public/{Vehicles}/** for traversal tuning",
        ],
        done_when=[
            "The same seed produces the same basin, and a test proves it.",
            "Weather reads as gameplay: it changes spawns, movement, aim and survivability.",
        ],
        handoff="Publishes the weather and biome state that AI, UI and Audio consume.",
        guardrails=["Never roll back the clock or the weather without telling the game state."],
    ),
    Agent(
        number=6,
        name="systems-economy-engineer",
        title="Systems & Economy Engineer",
        reports_to="the Lead Gameplay Engineer",
        mission="Own everything the player spends, carries, crafts and earns.",
        responsibilities=[
            "Own inventory, equipment, loot tables, crafting and the vendor economy.",
            "Own progression: the XP curve, skill trees and the modifier contract.",
            "Own faction standing and the relation matrix it drives.",
            "Keep every transaction logged, validated and reversible on partial failure.",
        ],
        owns=[
            "Source/Eclipse/Public/{Inventory,Loot,Crafting,Economy,Progression,Factions}/**",
            "Content/Eclipse/Data/VerticalSlice/{items.json,crafting.json,skills.json,factions.json}",
        ],
        done_when=[
            "A currency delta cannot exceed the configured ceiling and be applied.",
            "Loot rolls are reproducible from the seed and mirrored in the reference model.",
        ],
        handoff="Hands prices and loot to UI, and transaction events to Analytics.",
        guardrails=["Nothing is granted without a check; no refund is skipped because \"it cannot happen\"."],
    ),
    Agent(
        number=7,
        name="multiplayer-security-engineer",
        title="Multiplayer & Security Engineer",
        reports_to="the Lead Gameplay Engineer",
        mission="Make four players share the basin without either trusting the wrong client or hitching the frame.",
        responsibilities=[
            "Own session hosting/joining, travel, late-join policy and the raid lifecycle.",
            "Own server-side validation: rate limits, movement plausibility, transaction ceilings.",
            "Own the trust model and the violation log the post-raid report reads.",
            "Own replication hygiene: RepNotify over polling, intents over state on the wire.",
        ],
        owns=[
            "Source/Eclipse/Public/{Multiplayer,Security}/**",
        ],
        done_when=[
            "A client that lies about its position is corrected and its trust drops.",
            "Late joining works as configured and is refused when the host says so.",
        ],
        handoff="Reports violations to Analytics and exposes trust to the UI for the debug overlay.",
        guardrails=["A security check that only exists on the client is not a security check."],
    ),
    Agent(
        number=8,
        name="presentation-engineer",
        title="UI, Audio & Analytics Engineer",
        reports_to="the Lead Gameplay Engineer",
        mission="Make the state of the world and the player readable without a debug print.",
        responsibilities=[
            "Own the UI subsystem: menus, prompts, notifications, crafting progress, the HUD contract.",
            "Own the audio state model: ambience, wind, reverb and music intensity by weather and hour.",
            "Own telemetry: what is recorded, what is never recorded, and the post-raid summary.",
            "Own the accessibility pass: readable text, colour-blind-safe state cues, subtitles.",
        ],
        owns=[
            "Source/Eclipse/Public/{UI,Audio,Analytics}/**",
            "Content/Eclipse/UI/**",
        ],
        done_when=[
            "Every gameplay state a player must react to has a non-textual cue as well as a textual one.",
            "No telemetry event contains anything a player would consider personal.",
        ],
        handoff="Consumes delegates from every other system; never polls them.",
        guardrails=["Widgets are only ever created for a locally controlled player controller."],
    ),
    Agent(
        number=9,
        name="tools-build-engineer",
        title="Tools, Build & Release Engineer",
        reports_to="the Lead Gameplay Engineer",
        mission="Make the loop fast: build, content import, validation and release.",
        responsibilities=[
            "Own the build rules, the CI pipeline and the engine-free gates.",
            "Own the content import pipeline that turns the slice JSON into data assets.",
            "Own the save schema and its migrations.",
            "Own packaging and the release checklist in PUBLISHING.md.",
        ],
        owns=[
            "Tools/**",
            "scripts/**",
            ".github/workflows/ci.yml",
            "Source/Eclipse/Public/SaveSystem/**",
        ],
        done_when=[
            "The three engine-free gates run on every push and are trusted.",
            "A save written by an older schema loads or is refused with a reason, never half-read.",
        ],
        handoff="Publishes the bundle export that documents a build's exact contents.",
        guardrails=["CI must not require an engine install that contributors cannot get."],
    ),
    Agent(
        number=10,
        name="qa-security-reviewer",
        title="QA & Security Reviewer",
        reports_to="the Game Director",
        mission="Break the slice before a player does, and say so precisely.",
        responsibilities=[
            "Own the test plan: what is proven by unit tests, by automation tests, and by playing.",
            "Own the reference model's test coverage and the divergence policy.",
            "Own the threat model: what a malicious client can attempt and what stops it.",
            "Own the release verdict: ship, ship with caveats, or block with a repro.",
        ],
        owns=[
            "Tests/Reference/**",
            "Source/Eclipse/Private/Tests/**",
            "TESTING.md",
        ],
        done_when=[
            "Every algorithm that affects player-visible numbers has a mirror test.",
            "Every blocked release has a minimal reproduction attached to it.",
        ],
        handoff="Reports to the Game Director; a blocked release stops the milestone.",
        guardrails=["A passing test that cannot fail is not a test: prove it fails when the behaviour regresses."],
    ),
]


def render(agent: Agent) -> str:
    lines: List[str] = []
    lines.append(f"# {agent.number:02d} - {agent.title}")
    lines.append("")
    lines.append(f"- **Agent id:** `agent-{agent.number:02d}-{agent.name}`")
    lines.append(f"- **Reports to:** {agent.reports_to}")
    lines.append("")
    lines.append("## Mission")
    lines.append("")
    lines.append(agent.mission)
    lines.append("")
    lines.append("## Responsibilities")
    lines.append("")
    lines.extend(f"- {item}" for item in agent.responsibilities)
    lines.append("")
    lines.append("## Owns")
    lines.append("")
    lines.extend(f"- `{item}`" for item in agent.owns)
    lines.append("")
    lines.append("## Done when")
    lines.append("")
    lines.extend(f"- {item}" for item in agent.done_when)
    lines.append("")
    lines.append("## Handoff")
    lines.append("")
    lines.append(agent.handoff)
    lines.append("")
    lines.append("## Project rules that bind this role")
    lines.append("")
    lines.extend(f"- {item}" for item in RULES)
    lines.extend(f"- {item}" for item in agent.guardrails)
    lines.append("")
    lines.append("---")
    lines.append("")
    lines.append("Generated by `scripts/generate_agents.py`; edit the script, not this file.")
    lines.append("")
    return "\n".join(lines)


def expected_files() -> dict:
    return {
        f"{agent.number:02d}-{agent.name}.md": render(agent)
        for agent in AGENTS
    }


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the .agents role files.")
    parser.add_argument("--check", action="store_true", help="verify the files on disk instead of writing")
    args = parser.parse_args(argv)

    files = expected_files()

    if args.check:
        problems = 0
        on_disk = sorted(path.name for path in AGENT_DIR.glob("*.md")) if AGENT_DIR.exists() else []
        if len(on_disk) != len(files):
            print(f"generate_agents: expected {len(files)} role files, found {len(on_disk)}")
            problems += 1
        for name, content in files.items():
            path = AGENT_DIR / name
            if not path.exists():
                print(f"generate_agents: missing {path.relative_to(REPO_ROOT)}")
                problems += 1
            elif path.read_text(encoding="utf-8") != content:
                print(f"generate_agents: stale {path.relative_to(REPO_ROOT)}")
                problems += 1
        print(f"generate_agents: checked {len(files)} role file(s), {problems} problem(s).")
        return 1 if problems else 0

    AGENT_DIR.mkdir(parents=True, exist_ok=True)
    for name, content in files.items():
        (AGENT_DIR / name).write_text(content, encoding="utf-8")

    print(f"generate_agents: wrote {len(files)} role file(s) to {AGENT_DIR.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
