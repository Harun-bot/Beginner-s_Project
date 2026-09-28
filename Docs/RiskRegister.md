# Risk register

Likelihood / impact: H = high, M = medium, L = low. Reviewed at the end of every phase. Sources:
Section 24 of the design prompt, plus risks specific to a solo beginner on a Baseline-tier PC.

| # | Risk | L | I | Mitigation | Early warning |
|---|---|---|---|---|---|
| R1 | **Scope creep**: the design is AAA-sized; one person cannot build all of it | H | H | Vertical slice first; features frozen per phase; cut side content before story quality; every phase ends playable | A phase runs 50% over its estimate |
| R2 | **Traversal isn't fun** | M | H | Tune before anything else (Phase 1); all numbers in `DA_TraversalTuning` + live panel; keep the assists; feel ≥ 8/10 from 5 testers is a gate | Testers touch the ground, or rate < 8 |
| R3 | **Open-world performance and stutter** (UE5 open worlds are CPU-heavy) | H | H | Section 4.3 budgets are gates; nightly perf route from Phase 0; predictive streaming; PSO precaching + warm-up; profile weekly with Unreal Insights | Any nightly FAIL or regression |
| R4 | **Learning curve**: UE5 + C++ for a beginner | H | H | Blueprints for content; complete code with exact editor steps; one system at a time; ask about anything unclear in a deliverable before moving on | Stuck on one step for more than a session |
| R5 | **Toolchain drift**: UE 5.8 API changes, Visual Studio version mismatches | M | M | Pin UE 5.8.x for the whole project (no mid-project migration); keep VS updated; paste build errors back immediately | Build errors mentioning engine symbols |
| R6 | **Content volume** too large for a small team | H | H | Systemic activities over hand-made ones; PCG + modular kits; reuse; a district must reuse 80% of the kit | Art backlog growing faster than it's cleared |
| R7 | **Asset quality and licensing** | M | H | `Docs/Tracking/AssetLicenses.csv` for every third-party asset (source, license, proof); one art bible; check Fab license terms before use | An asset in `Content/` with no license row |
| R8 | **IP exposure**: Spider-Man and cast belong to Marvel/Sony; **this GitHub repo is public** | M | H | Make the repo private before adding Spider-Man-named content, art or dialogue; keep IP names out of code and asset names (done: `WebOfTheCity`, `Hero`); swap checklist before any release (Section 19); never copy assets, story, music or code from existing games or films | Anything marked "Spider-Man" in a public commit |
| R9 | **Git LFS storage/bandwidth limits** | H | M | Fine for greybox phases; check the quota before Phase 3; move to Perforce or a larger LFS host if needed | GitHub LFS usage > 70% |
| R10 | **Hardware lower than assumed** (Section 0 was blank) | M | H | Confirm specs before Phase 1; Minimum-tier fallback; engine alternative if below Minimum | Editor under 30 FPS in the greybox |
| R11 | **Experimental engine features** (Mover, MetaHuman crowds, Mass maturity) | M | M | Custom movement component instead of Mover; verify Mass/ZoneGraph in a spike before relying on them; always have a simpler fallback | A feature needs engine patches |
| R12 | **Online complexity** | L | M | Offline first; co-op is optional Phase 5 and never gates the campaign | Online code appearing before Phase 5 |
| R13 | **Data loss** on a single PC | M | H | Push at least daily; LFS objects are on GitHub; nightly packages kept outside the repo | Unpushed work older than a day |
| R14 | **Burnout / motivation** on a long solo project | M | H | Small weekly goals inside each phase; a playable build at the end of every phase; celebrate DoDs | Two weeks without a commit |
