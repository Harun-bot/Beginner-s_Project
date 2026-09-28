# Working agreement for AI assistants

The source of truth is `Docs/Design/Game_Design_Master_Prompt_v1.0.pdf`. Part A is the spec and the
rules; Part B holds the phase prompts. Section numbers in code and docs refer to it.

## Rules (Part A, Section 1, condensed)

- Work one phase at a time (Section 20). Don't polish before core feel is proven. A phase ends with a playable build and its Definition of Done.
- Deliver complete, commented, compilable code and exact editor steps (menu paths, settings, asset names). No pseudo-code or `TODO: implement` in core systems.
- Say plainly when something can't be produced as text (art, animation, VO, music), then give a sourcing plan.
- Every tunable number goes in a data asset or config file, never hard-coded.
- After each deliverable, list what to test and what "good" looks like.
- Ask at most 3 clarifying questions, and only if truly blocked. Otherwise state the assumption and proceed.
- Output format for every deliverable (Section 22): goal and assumptions, file structure, code and editor steps, tuning table, test checklist, perf notes, known limitations and next step.
- "review" means auditing the last deliverable against Sections 3 (pillars), 4.3 (perf budgets) and 22 (output format).

## Current state

- Phase 0 is delivered (see `Docs/Phase0_PreProduction.md`) and waiting for the user's go-ahead and real PC specs. Section 0 of the prompt was blank, so the Baseline tier and a beginner skill level are assumed.
- The engine is UE 5.8, module `WebOfTheCity`. The only C++ so far is `Perf/PerfRouteRunner`.
- None of the C++ or editor steps have been compiled or run yet; the user's first Windows build is the first real test.

## Conventions

- Naming and layout: `Docs/Standards.md`. Keep IP names out of code and asset names (`Hero`, not `SpiderMan`).
- Perf harness: `Tools/Perf/analyze_perf.py`, with budgets in `Tools/Perf/budgets.json`. The CSV column order is a contract with `PerfRouteRunner.cpp`.
- Tests: `python -m unittest discover -s Tools/Perf/tests`.
- Greybox city and perf route: `Tools/Editor/make_greybox_city.py`. Running it with plain `python` does a dry run that checks the route doesn't pass through buildings.
