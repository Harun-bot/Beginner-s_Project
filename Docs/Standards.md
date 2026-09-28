# Standards: naming, code, Blueprints, Git

These rules exist so the project stays navigable at 10,000 assets. When in doubt, follow Epic's own
[coding standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine);
everything below is either a project decision or a reminder of it.

## 1. Content folder layout

All project content lives under one root, so marketplace/Fab packs (which install to their own
top-level folders) never mix with ours.

```
Content/
  WebOfTheCity/
    Core/            game modes, player controller, game instance, Input/ (IA_, IMC_)
    Characters/
      Hero/          SK_, ABP_, animations, suit materials
      NPC/           civilians, allies
      Enemies/       one sub-folder per archetype (Thug, Brute, Gunner, ...)
    Traversal/       Data/ (DA_TraversalTuning), Anims/, VFX/ (web ribbons)
    Combat/          abilities (GA_, GE_), montages, hit reactions
    Gadgets/
    AI/              BT_, BB_, ST_, EQS_
    World/
      Districts/     Midtown/, FinancialDistrict/, ... (one per Section 7 district)
      Props/         shared props: water tanks, antennas, cranes (anchor-friendly)
      PCG/           PCG_ graphs and their data
      Materials/     shared master materials (M_) and functions (MF_)
    Maps/
      Dev/           greyboxes and sandboxes
      Test/          perf routes and automated-test maps (L_PerfRoute_*)
      Districts/     the real world partition map(s)
      Missions/      mission-only level instances
    UI/              WBP_, fonts, icons
    Audio/           Music/, SFX/, VO/, MetaSounds (MS_)
    VFX/             NS_, NE_ shared effects
    Cinematics/      LS_ level sequences
    Data/            DT_ tables, DA_ assets, curves
  Developers/        personal sandboxes (enable in Content Browser settings); never cooked
```

Source code mirrors the systems: `Source/WebOfTheCity/{Public,Private}/<System>/` with systems
`Perf`, `Traversal`, `Combat`, `Gadgets`, `AI`, `World`, `UI`, `Save`, `Online`.

## 2. Asset naming

Pattern: `Prefix_BaseName_Variant_Suffix`, PascalCase, no spaces, English, no real-world brands.
Keep IP names out of asset and class names (`Hero`, not `SpiderMan`), so the Section 19
"swap in an original hero" stays a content change instead of a rename of half the project.

| Asset type | Prefix | Example |
|---|---|---|
| Level | `L_` | `L_PerfRoute_Swing`, `L_Greybox_Midtown` |
| Blueprint (actor/class) | `BP_` | `BP_Hero` |
| Blueprint interface | `BPI_` | `BPI_Interactable` |
| Widget Blueprint | `WBP_` | `WBP_HUD` |
| Animation Blueprint | `ABP_` | `ABP_Hero` |
| Animation sequence / montage / blend space | `AS_` / `AM_` / `BS_` | `AS_Hero_Swing_Loop`, `AM_Hero_Light_01` |
| Pose Search database / schema | `PSD_` / `PSS_` | `PSD_Hero_Locomotion` |
| Skeletal mesh / skeleton / physics asset | `SK_` / `SKEL_` / `PHYS_` | `SK_Hero` |
| Static mesh | `SM_` | `SM_WaterTank_01` |
| Material / instance / function | `M_` / `MI_` / `MF_` | `MI_Glass_Tower_Blue` |
| Texture | `T_` + suffix `_D` base color, `_N` normal, `_ORM` packed AO/rough/metal, `_E` emissive, `_M` mask | `T_Brick_01_N` |
| Niagara system / emitter | `NS_` / `NE_` | `NS_WebImpact` |
| MetaSound source / sound wave | `MS_` / `A_` | `MS_Wind_Speed` |
| Level sequence | `LS_` | `LS_M01_Intro` |
| Data asset / data table / curve | `DA_` / `DT_` / `C_` | `DA_TraversalTuning`, `C_SwingFOV` |
| Enum / struct (Blueprint) | `E_` / `F_` | `E_TraversalState` |
| Input action / mapping context | `IA_` / `IMC_` | `IA_Swing`, `IMC_Traversal` |
| Gameplay ability / effect / cue | `GA_` / `GE_` / `GC_` | `GA_WebStrike` |
| Behavior tree / blackboard / StateTree / EQS | `BT_` / `BB_` / `ST_` / `EQS_` | `BT_Thug` |
| PCG graph | `PCG_` | `PCG_RooftopClutter` |
| Render target | `RT_` | `RT_PhotoMode` |

Numbered variants use two digits (`_01`). Levels for a mission use the mission number (`L_M04_Vulture`).

## 3. C++

- Epic's conventions: `U`/`A`/`F`/`E`/`I`/`T` prefixes, `b` for bools, PascalCase, `TObjectPtr<>` for
  UObject member pointers, `UPROPERTY()` on every UObject reference, no raw `new`/`delete` for UObjects.
- **Core systems are C++**: movement, combat, AI, networking, save (Section 4.1). Blueprints
  subclass them for content.
- **No magic numbers in gameplay code.** Every tunable goes in a data asset (`UPrimaryDataAsset`,
  e.g. `DA_TraversalTuning`) or config file so feel can be tuned without recompiling (Section 1).
  Defaults in C++ are fine as long as the data asset overrides them.
- One log category per system (`LogTraversal`, `LogCombat`, ...). Warnings mean "someone should
  look", errors mean "this is broken".
- Comments explain *why*, and cite the design section when a number or rule comes from it
  (`// Section 4.3: no hitch > 50 ms`).
- Headers: forward-declare instead of including when you can; include what you use in `.cpp`.
- Every system ships with its debug view (console command or `stat`/HUD overlay) so it can be tuned
  and bug-reported.
- Clean compile at warning level default; no new warnings in a merge.

## 4. Blueprints

- For content, UI, one-off level scripting and prototypes. When a Blueprint graph grows logic that
  other things depend on, move it to C++.
- Functions over macros; comment boxes around each block; no crossing wires you can't follow;
  collapse long chains into named functions.
- Expose tunables as `Instance Editable` variables with categories and tooltips.
- No Tick in Blueprints unless the profiler says it is cheap; prefer events and timers.

## 5. Git

- `main` always builds and passes the perf harness. Work on `feature/<system>-<thing>` branches.
- Small commits, imperative subject line: `Add web-zip anchor preview`, `Fix wall-run exit on ledges`.
- Merge only when the phase's test checklist passes and `analyze_perf.py` shows no regression.
- **Fix Up Redirectors** (right-click `Content` in the Content Browser) before committing moves or renames.
- Never commit `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` or IDE files (see `.gitignore`).
- Binary assets go through LFS automatically (`.gitattributes`). If you work with others, turn on
  LFS locking for `.uasset`/`.umap` (`lockable` in `.gitattributes`) so two people never edit the
  same asset.
- Baselines in `Tools/Perf/Baselines/` change only on purpose (`Nightly.ps1 -WriteBaseline`), with
  a commit message saying why.

## 6. Python tools

PEP 8, standard library only for anything under `Tools/Perf` (so it runs on any machine and the
nightly task has no install step), and a test under `Tools/Perf/tests` for every behaviour the
harness gates on. Editor scripts (`Tools/Editor`) may use the `unreal` module and must be safe to
re-run.
