# AGENTS.md - how work is done on PROJECT ECLIPSE

This file is the contract for every agent (human or automated) working in this repository.
It is deliberately short and absolute: the long-form explanations live in
`Documentation/AGENTS.md`, and every rule here has a reason recorded there or in
`Documentation/DECISIONS.md`.

## The five rules

1. **Real systems only.** No fake systems, no empty functions, no decorative UI, no
   TODO-as-feature. If a feature is not implemented, it does not exist in the tree. A
   function that returns a hard-coded value to make a system "work" is a bug.
2. **Server authority.** Clients send intent; the server decides. Anything that changes
   game state - damage, loot, crafting, trading, missions, spawning - is computed on the
   server and replicated. A client-side check that is not repeated on the server is not a
   check.
3. **C++ for systems, Blueprint for composition, Data Assets for numbers.** Gameplay logic
   belongs in this module. A number that a designer would want to change belongs in a data
   asset or in `UEclipseDeveloperSettings`, never in a `const float` inside a `Tick`.
4. **Mirror the arithmetic.** Any change to a player-visible formula is made in the C++ and
   in `Tests/Reference/eclipse_reference_model.py` **in the same commit**, with a test on
   both sides. The reference model is not documentation; it is the second implementation
   that proves the first one.
5. **Never break an existing system to add a new one.** Extend the contract or add a second
   one. If two systems disagree about a number's meaning, that is a design bug: fix the
   contract, then both systems.

## Definition of done

A change is done when all of the following are true:

- The three engine-free gates pass, and their output is quoted in the pull request:

  ```bash
  python3 Tools/ci/lint_cpp.py
  python3 -m unittest discover -s Tests/Reference -p "test_*.py"
  python3 Tools/ci/validate_content.py --quiet
  ```

- New content is validated by `Tools/ci/validate_content.py` (ids unique, cross-references
  resolve, enum values exist in C++).
- New public headers carry a comment block that explains **why** the type exists and who is
  allowed to call it.
- New delegates are broadcast where the state changes, not polled from `Tick`.
- Anything that runs per frame is inside a budget (see `PERFORMANCE.md`).
- The agent's own brief lists the path: if a role owns a directory, the file lives there.

## Review checklist

| Question | If the answer is no |
| --- | --- |
| Can a client cheat by doing this locally? | Move the decision to the server |
| Is there a single entry point for this capability? | Add one; do not add a second path |
| Would a designer need to edit code to tune it? | Move the value into a data asset |
| Does the reference model still agree? | Update it in the same commit |
| Does this leak memory, tick forever, or spawn unbounded actors? | Bound it, or the change is refused |
| Is the reason it exists written down? | Write it in the header, then commit |

## The ten agents

| # | Agent | Owns |
| --- | --- | --- |
| 01 | Game Director | Slice scope, mission chain, decisions |
| 02 | Lead Gameplay Engineer | Core, Characters, module contracts |
| 03 | Combat & Weapons Engineer | Combat, Weapons, damage pipeline |
| 04 | AI & Encounter Engineer | AI, Missions, encounters |
| 05 | World & Procedural Engineer | World, weather, procgen, vehicles |
| 06 | Systems & Economy Engineer | Inventory, loot, crafting, economy, progression, factions |
| 07 | Multiplayer & Security Engineer | Sessions, replication, validation, trust |
| 08 | Presentation Engineer | UI, audio, analytics |
| 09 | Tools, Build & Release Engineer | Tools, scripts, CI, save schema |
| 10 | QA & Security Reviewer | Tests, reference model, threat model, release verdict |

Regenerate and verify the briefs with:

```bash
python3 scripts/generate_agents.py
python3 scripts/deploy_subagents.py --check
```

## Working agreement

- **One milestone per pull request.** A milestone leaves the slice playable: it does not end
  in a state where a system is half-migrated.
- **Commits explain the why.** The diff shows the what.
- **No drive-by refactors.** Reformatting a file you are not changing hides the change.
- **A blocked release stops the milestone.** Agent 10's verdict is binding until the
  reproducer is fixed.
