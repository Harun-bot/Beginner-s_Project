# Web of the City (working title)

A PC open-world, third-person web-swinging action-adventure built in **Unreal Engine 5.8**:
swing through a dense, living city, fight readable acrobatic battles, and balance a hero's two lives.
Offline-first; optional co-op later.

> Private learning / fan project. Spider-Man and related characters belong to Marvel/Sony. Code and
> assets use neutral names (`WebOfTheCity`, `Hero`) so an original hero can be swapped in before any release
> (design prompt, Section 19).

## Status

| Phase | What | State |
|---|---|---|
| 0 | Pre-production: engine, repo, standards, perf harness, nightly build, risks | **Delivered, waiting for your go-ahead** |
| 1 | Traversal prototype in a greybox district | Next |
| 2 | Combat prototype | |
| 3 | Vertical slice (one district, missions 1–4, Vulture boss) | |
| 4 | Full production | |
| 5 | Online co-op (optional) | |
| 6 | Polish and hardening | |
| 7 | Release and live-ops | |

## Start here

1. **[Docs/Phase0_PreProduction.md](Docs/Phase0_PreProduction.md)**: hardware tier, engine choice, install
   steps, first build, test map, nightly build, and the Phase 0 checklist.
2. [Docs/Standards.md](Docs/Standards.md): folder layout, naming, C++/Blueprint/Git rules.
3. [Docs/PerfTestRoutes.md](Docs/PerfTestRoutes.md): how performance is measured and gated.
4. [Docs/RiskRegister.md](Docs/RiskRegister.md): what could sink the project and what we do about it.
5. [Docs/Design/](Docs/Design/): the master design prompt (the source of truth for every section number).

## Quick commands

```bat
:: first build: right-click WebOfTheCity.uproject > Generate Visual Studio project files, then build "Development Editor | Win64"

:: nightly package + perf route + report (records this PC's baseline on first run)
powershell -ExecutionPolicy Bypass -File Tools\Build\Nightly.ps1

:: analyze the newest perf CSV from an editor run
python Tools\Perf\analyze_perf.py Saved\Perf

:: harness unit tests
python -m unittest discover -s Tools/Perf/tests
```
