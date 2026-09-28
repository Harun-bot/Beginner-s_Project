# Phase 0: Pre-production

Design source: [`Docs/Design/Game_Design_Master_Prompt_v1.0.pdf`](Design/Game_Design_Master_Prompt_v1.0.pdf). Section numbers below refer to it.
The layout follows the Section 22 output format.

**Definition of Done (Section 20):** the project builds, a nightly build runs, and the performance harness records a baseline.
Checklist: [section 5](#5-test-checklist).

---

## 1. Goal and assumptions

**Goal:** a repo you can clone and build. It includes coding and naming rules, a greybox test city, an automated performance route, a nightly build and a risk list. With those in place, Phase 1 (traversal) starts on solid ground and every later change is measured against the Section 4.3 budgets.

### Section 0 was blank, so these are my assumptions

| Item | Assumed | Why |
|---|---|---|
| PC | **Baseline tier**: RTX 3060 12 GB-class GPU, 6-core CPU (Ryzen 5 3600 / i5-12400 class), 16 GB RAM, SSD with 200+ GB free, Windows 11 64-bit, 1080p monitor | The prompt's own research: RTX 3060 and 16 GB RAM are the most common Steam configuration (Section 4.2) |
| Skill level | Beginner | The repository is called *Beginner-s_Project* |
| Team / time | Solo, about 10 to 15 hours a week | Typical for a personal project |
| Engine | Unreal Engine 5.8 | Section 0 default; justified below |
| Input | Keyboard + mouse and gamepad | Section 11 |
| Online | Offline only for now; co-op decided at Phase 5 | Section 15 rule #1 |
| Asset budget | Free (Fab free content, Epic samples, MetaHuman) | Safest default |
| Scope | Vertical slice first | Recommended in Section 0 |

**If any of these is wrong, tell me before Phase 1.** Your real GPU, CPU and RAM matter most.
To find them: press `Win + R`, type `dxdiag`, press Enter. The *System* tab shows CPU and RAM, and the *Display 1* tab shows the GPU and its memory.

### Hardware tier classification (Section 4.2)

With the assumed PC you are on the **Baseline** tier. What to expect:

| | What it means for you |
|---|---|
| Game target | 1080p at 60 FPS, High preset, upscaler on Quality, software Lumen (Lite), no hardware ray tracing. This is the tier that "must be smooth". |
| Editor | Fine for greybox and prototypes (Phases 0 to 2). Once districts have final art (Phase 3), 16 GB RAM gets tight. 32 GB is strongly recommended by then. |
| First open | Unreal compiles shaders the first time you open the project and each new map. Expect 10 to 40 minutes the first time; after that it is cached. |
| C++ builds | First full build roughly 5 to 15 minutes on 6 cores; small changes then take seconds to a minute. |
| Disk | Engine 5.8 install plus project, cache and builds: plan for 150 GB or more on the SSD. |

If your PC turns out lower: at the **Minimum** tier (GTX 1650 / 4-core) UE 5.8 still works for prototypes on the Low preset, but the editor will be slow. Below Minimum (no DirectX 12 / Shader Model 6 GPU, or 8 GB RAM), I'd recommend switching to Unity 6.3 LTS or Godot 4 and re-planning, as Section 4.1 allows.
If you're on **High** or **Ultra**, nothing changes now; hardware ray tracing gets switched on in Phase 3.

### Engine choice: Unreal Engine 5.8

- **Why:** almost every system the design asks for is built in: Nanite, Lumen, MegaLights, World Partition, PCG, Motion Matching, Gameplay Ability System, Mass crowds, MetaSounds and Iris networking. 5.8 is the last planned major UE5 release, so there's no forced mid-project migration (Section 4.1).
- **Trade-off for a beginner:** UE's learning curve is steep, and core systems are in C++ (Section 4.1). Two things make that manageable: you do content, UI and prototyping in Blueprints, and I supply complete, commented code with exact editor steps.
- **Not Unity 6.3 LTS:** reaching the target realism would take much more custom work. **Not Godot 4:** great for prototypes, not built for this level of realism.
- **Movement:** a custom C++ movement component for traversal (predictable and fully tunable). Mover is still leaving Experimental in 5.8, so we can try it later on a side branch (Section 4.1).

---

## 2. Folder and file structure

```
WebOfTheCity.uproject          Unreal project (engine 5.8)
Config/                        DefaultEngine/Game/Input.ini (Lumen, VSM, DX12 SM6, Enhanced Input)
Source/
  WebOfTheCity.Target.cs       game target
  WebOfTheCityEditor.Target.cs editor target
  WebOfTheCity/
    WebOfTheCity.Build.cs
    Public/  Private/          one sub-folder per system: Perf/ now; Traversal/, Combat/, ... later
      Perf/PerfRouteRunner     the performance route actor (harness)
Content/                       created by the editor; layout in Docs/Standards.md
Tools/
  Editor/make_greybox_city.py  builds the greybox test city + perf route inside the editor
  Perf/analyze_perf.py         checks a perf CSV against Section 4.3 budgets and a baseline
  Perf/budgets.json            the Section 4.3 numbers (edit here, not in code)
  Perf/tests/                  unit tests for the analyzer
  Perf/Baselines/<PC>/         per-machine baselines (created by the first nightly run)
  Build/Nightly.ps1            package + perf run + report
  Build/RegisterNightlyTask.ps1  schedule it daily in Windows Task Scheduler
Docs/
  Phase0_PreProduction.md      this file
  Standards.md                 naming, coding, Blueprint, Git rules + Content folder layout
  PerfTestRoutes.md            performance test-route plan
  RiskRegister.md              risk list
  Tracking/AssetBudgets.csv    asset budget sheet (Section 4.1 pipeline)
  Tracking/AssetLicenses.csv   asset license tracker (Section 19)
  Design/                      the master design prompt (source of truth)
CLAUDE.md                      working agreement for AI assistants on this repo
```

---

## 3. Setup steps

### 3.1 Install the tools (once)

1. **Epic Games Launcher**: install it from epicgames.com and sign in. Go to *Unreal Engine → Library*, click **+** next to *Engine Versions*, pick **5.8.x**, then click **Install**.
   Before installing, open *Options* (the ▼ next to the version) and set:
   - Core Components: on (required)
   - Templates and Feature Packs: **on** (Phase 1 uses the Third Person pack)
   - Engine Source: on (small; lets you read engine code while debugging)
   - Editor symbols for debugging: **off** for now (tens of GB; add it later when you need to step into engine code)
   - Target platforms: leave off (Windows is built in)
2. **Visual Studio Community** (free), updated to the latest release. UE 5.8 needs a newer MSVC toolset than 5.7, so an old Visual Studio install won't build it. The exact minimum version is in the *Platform SDK Upgrades* part of the UE 5.8 release notes.
   In the Visual Studio Installer, tick these workloads:
   - **Game development with C++**, and under *Installation details* also tick **IDE support for Unreal Engine**
   - **Desktop development with C++**
   - **.NET desktop development**

   Once you generate project files (step 3.3), Unreal writes a `.vsconfig` next to the `.sln`. Opening the `.sln` then offers to install anything still missing: accept.
   *Alternative:* JetBrains Rider is free for non-commercial use and very good with Unreal.
3. **Git for Windows** (installs Git LFS too; keep the defaults). Then open a terminal once and run `git lfs install`.
   *Optional GUI:* GitHub Desktop.
4. **Python 3.11 or newer** from python.org. Tick *Add python.exe to PATH*. This is used by `Tools/Perf`.

### 3.2 Get the project

Use a short path without spaces. Windows path-length limits cause trouble for Unreal projects in deep folders.

```bat
cd C:\
mkdir Dev
cd Dev
git clone https://github.com/Harun-bot/Beginner-s_Project.git WebOfTheCity
cd WebOfTheCity
git checkout claude/pdf-review-b2yfbr
git lfs install
```

(After this branch is merged, stay on `main`.)

### 3.3 First build

1. Right-click `WebOfTheCity.uproject` → **Generate Visual Studio project files**. On Windows 11 the item is under *Show more options*.
2. Open `WebOfTheCity.sln`. In the toolbar set **Development Editor** and **Win64**, then *Build → Build Solution* (`Ctrl+Shift+B`). The build must end with `Build succeeded` and 0 errors.
3. Press `F5` (or double-click the `.uproject`) to start the editor. Let the first shader compile finish; the progress is shown bottom-right.

*Shortcut:* you can skip steps 1 and 2 by double-clicking the `.uproject`. When it says the *WebOfTheCity* module is missing and asks to rebuild, click **Yes**. Visual Studio still has to be installed.

### 3.4 Check project settings (one time, in the editor)

`Config/DefaultEngine.ini` already sets these. Just confirm them:

- *Edit → Project Settings → Engine → Rendering*: Dynamic Global Illumination **Lumen**; Reflection Method **Lumen**; Shadow Map Method **Virtual Shadow Maps**; Support Hardware Ray Tracing **off** (until Phase 3); Anti-Aliasing **TSR**.
- *Project Settings → Platforms → Windows*: Default RHI **DirectX 12**; Targeted RHIs **SM6**.
- *Edit → Plugins*: **Python Editor Script Plugin**, **Editor Scripting Utilities**, **Modeling Tools Editor Mode** and **Enhanced Input** are all enabled.
- *Edit → Editor Preferences → General → Performance*: **untick "Use Less CPU when in Background"**, otherwise perf runs in the editor get throttled when the window loses focus.

### 3.5 Build the perf test map

1. *File → New Level → **Empty Open World** → Create.* Then *File → Save Current Level As…*, create the folders `WebOfTheCity/Maps/Test` under *Content*, and save as **`L_PerfRoute_Swing`**.
2. *Tools → Execute Python Script…* and pick `Tools/Editor/make_greybox_city.py`. It takes about a minute. The Output Log should end with:
   `Greybox: 456 buildings, 136 water tanks, 24 park trees, 4 stations, 1.8 x 1.8 km, route 'Swing' 2486 m. Now use File > Save All.`
   (Phase 1 added the park, anchor tags and fast-travel stations; re-run the script on an older map to get them.)
3. *File → Save All*.
4. Test the route in the editor: press **Play**, press the backtick key (`` ` ``) to open the console, type `PerfRoute.Start Swing`, and press Enter. The camera flies through the street canyons for about 45 s (including the 5 s warm-up). The Output Log then prints `Route 'Swing' finished: … Wrote …\Saved\Perf\PerfRoute_Swing_<time>.csv`.
5. *Project Settings → Maps & Modes*: set **Editor Startup Map** and **Game Default Map** to `L_PerfRoute_Swing` for now. *Project Settings → Packaging → List of maps to include in a packaged build*: add `L_PerfRoute_Swing`.
6. (Prep for Phase 1, optional) *Content Drawer → + Add → Add Feature or Content Pack → Blueprint Feature → **Third Person** → Add to Project.* This gives us the mannequin and animations as placeholders.

### 3.6 Git + LFS

The repo already contains `.gitattributes`, which sends every binary asset (`.uasset`, `.umap`, textures, audio, source art) to **Git LFS**, and `.gitignore`, which leaves out `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` and IDE files.

```bat
git add -A
git commit -m "Add perf test map"
git push
git lfs ls-files        :: should list the .umap and the map's external actor .uasset files
```

Rules (full list in `Docs/Standards.md`):

- `main` must always build. Do each task on a branch (`feature/traversal-swing`) and merge it when that phase's checklist passes and the perf report shows no regression.
- Before committing content moves or renames, right-click the *Content* folder and choose **Fix Up Redirectors**.
- **Storage:** GitHub's free LFS allowance is small for an Unreal project. It's fine for Phases 0 to 2 (greybox), but check your usage under *GitHub → Settings → Billing* before Phase 3 art arrives. Options then: buy LFS data, a host with a larger LFS allowance, or Perforce (free for small teams, and the Unreal industry standard).
- **Visibility:** this repository is currently **public**. See risk R8 in `Docs/RiskRegister.md`, and make it private before adding anything Spider-Man-branded.

### 3.7 Run the harness and record the baseline (the DoD)

From the repo folder, in PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File Tools\Build\Nightly.ps1
```

The first run compiles and cooks everything, so allow 30 to 90 minutes. It then:
1. packages a Development build to `%USERPROFILE%\WebOfTheCityBuilds\LatestPackage`,
2. launches it on `L_PerfRoute_Swing` with `-PerfRoute=Swing -PerfRouteQuit` (don't touch the PC while the window flies the route),
3. writes `perf_report.md` in `%USERPROFILE%\WebOfTheCityBuilds\<date_time>\`,
4. creates **`Tools/Perf/Baselines/<YOUR-PC>/Swing.json`**, the recorded baseline. **Commit this file.**

If the engine isn't in `C:\Program Files\Epic Games\UE_5.8`, add `-EngineDir "D:\Epic\UE_5.8"`, or set a `UE_ROOT` environment variable.

Schedule it every night (runs while you're logged in; the task can wake the PC from sleep):

```powershell
powershell -ExecutionPolicy Bypass -File Tools\Build\RegisterNightlyTask.ps1 -Time 03:00
```

Quick check without packaging (editor numbers are indicative only):

```bat
python Tools\Perf\analyze_perf.py Saved\Perf
```

---

## 4. Tuning table

| Setting | Where | Default | What it does |
|---|---|---|---|
| `SpeedMetersPerSecond` | PerfRoute actor, Details panel | 60 | Camera speed. 60 m/s is the top swing speed used by the streaming gate (Section 4.3). |
| `WarmupSeconds` | PerfRoute actor | 5 | Parked at the start first; frames recorded but not gated. |
| `HoldAtEndSeconds` | PerfRoute actor | 0 | Parks at the end. Used later for the combat-arena and crowd-plaza routes. |
| `FieldOfView` | PerfRoute actor | 105 | Top-speed swing FOV (Section 11), the worst case for rendering. |
| `bUncapFrameRate` | PerfRoute actor | on | Turns off VSync and the FPS cap during the run so we measure headroom. |
| `bCaptureCsvProfile` | PerfRoute actor | on | Also records Unreal's detailed CSV profile (`Saved/Profiling/CSV`) for deep dives. |
| `bAutoStartInEditor` | PerfRoute actor | off | Starts the route automatically when you press Play. |
| Budgets | `Tools/Perf/budgets.json` | Section 4.3 | Frame 16.6 ms, GPU 14, game thread 8, render thread 8, 1% low 45 FPS, hitch 50 ms, RAM 12 GB. |
| `regression_tolerance_pct` | `Tools/Perf/budgets.json` | 5 | How much worse than the baseline any metric may get before the run fails. |
| City layout | Top of `make_greybox_city.py` | 12 x 20 blocks, 20–300 m | Block and street sizes, heights, seed. Changing these changes perf numbers, so re-record the baseline if you do. |
| Nightly options | `Nightly.ps1` parameters | 1920x1080, `Swing` | `-Map`, `-Route`, `-ResX/-ResY`, `-SkipBuild`, `-WriteBaseline`. |

---

## 5. Test checklist

| # | Test | Pass when |
|---|---|---|
| 1 | Build *Development Editor / Win64* in Visual Studio | `Build succeeded`, 0 errors |
| 2 | Open the `.uproject` | Editor opens with no "missing modules" prompt |
| 3 | Run `make_greybox_city.py` in the empty open-world map | Log shows 456 buildings and route `Swing` 2486 m; the city is visible; `PerfRoute_Swing` is in the Outliner |
| 4 | Play → `PerfRoute.Start Swing` | Camera flies the whole route without hitting buildings; log prints `Wrote …csv` |
| 5 | `python Tools\Perf\analyze_perf.py Saved\Perf` | A report with a gates table appears (the verdict can be FAIL in the editor; that's fine) |
| 6 | `git lfs ls-files` after committing the map | Lists `.umap` / `.uasset` files |
| 7 | `Nightly.ps1` | Exit code 0 or 1 (not 2 or 3); a package, `perf_report.md` and `Tools/Perf/Baselines/<PC>/Swing.json` exist |
| 8 | `RegisterNightlyTask.ps1` | *Task Scheduler* shows "WebOfTheCity Nightly", and the next morning `history.csv` has a new line |
| 9 | `python -m unittest discover -s Tools/Perf/tests` | `OK` (14 tests) |

**What "good" looks like:** the greybox is just lit boxes, so a Baseline PC should beat the 16.6 ms frame budget with lots of room to spare (expect well above 60 FPS) and show zero hitches after the warm-up. If it doesn't, the setup is the problem, not the game. Check that you ran a Development package (not Debug), that a laptop is plugged in and in a high-performance power mode, and that nothing else heavy is running.

---

## 6. Performance notes

- **Gate numbers come from the packaged Development build** (`Nightly.ps1`). Numbers from Play-in-Editor include editor overhead and are only a quick signal.
- Game-thread, render-thread and GPU times come from the same counters as `stat unit`, sampled every frame. They lag the frame time by one or two frames, which doesn't affect route averages.
- **1% low** is the FPS of the average of the slowest 1% of frames. **Hitch** means any single frame over 50 ms after the warm-up.
- **Not measured automatically yet:** VRAM (≤ 8 GB; check *Task Manager → Performance → GPU → Dedicated GPU memory* during a run), input latency (≤ ~80 ms; measured in Phase 1 once there is a controllable character), and cold boot, fast-travel and respawn times (Phase 3, once menus and fast travel exist).
- Resolution and upscaling: Section 4.3's Baseline is 1080p output with the upscaler on Quality. Scalability presets arrive in Phase 3. Until then runs use engine defaults, and each report header records the resolution and screen percentage so runs stay comparable.

---

## 7. Known limitations and next step

**Limitations (please read):**

- **I could not compile or run Unreal here.** I work in a Linux cloud container without UE 5.8, so the C++ and editor steps are untested. I kept the C++ to standard, conservative engine APIs, but the first build on your PC is the real test. If the compiler complains about `GGameThreadTime`, `GRenderThreadTime` or `RHIGetGPUFrameCycles`, 5.8 has renamed a stat global: paste me the error and I'll fix it.
- The PowerShell scripts were not run (no Windows here). The Python pieces were: the analyzer passes 14 unit tests, and the greybox layout dry run gives a 2,486 m route with 0 points inside buildings.
- I couldn't reach Epic's documentation site from here, so the Visual Studio version is described rather than pinned. Use the release notes as the authority.
- Everything in section 1 is an assumption until you confirm it.

**Next step: Phase 1, the traversal prototype** (swing, zip, launch, wall-run, cling, glide stub, camera, `TraversalTuning` data asset, live-tuning panel). Before I start, I need:

1. **Your real PC specs** (GPU + VRAM, CPU, RAM, free SSD space) and skill level, if my assumptions are wrong.
2. **Your approval (or changes) for this keyboard + mouse layout.** Section 11 says to get it approved before locking it in. Everything will be rebindable anyway.

| Action | Keyboard + mouse | Gamepad (Section 11) |
|---|---|---|
| Move / camera | W A S D / mouse | LS / RS |
| **Swing** (hold); on the ground: sprint, parkour, wall-run | **Left Shift** (hold) | R2 / RT (hold); L3 |
| Jump · web-zip · point-launch (in air) | Space | A |
| Light attack | Left mouse | X |
| Heavy attack / launcher | Right mouse | Y |
| Web-strike | E | B |
| Dodge | Left Ctrl | R1 / RB |
| Gadget wheel (hold) | Tab | L1 / LB |
| Aim gadget (hold) + fire | Q (hold) + left mouse | L2 / LT |
| Lock-on | Middle mouse | R3 |
| Focus finisher | F | (combo, set in Phase 2) |
| Interact · map · pause | R · M · Esc | (context) · Touchpad/View · Options/Menu |

3. **Go-ahead:** reply "Begin Phase 1" once the section 5 checklist passes on your PC, or send me any errors you hit.
