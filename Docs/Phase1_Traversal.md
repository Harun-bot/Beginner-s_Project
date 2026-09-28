# Phase 1: Traversal prototype

Design source: Sections 6.1 (traversal), 11 (camera and controls), 20 (Definition of Done) and 21 (bots, playtests).
The layout follows the Section 22 output format.

**Definition of Done (Section 20):** 2 km of continuous swinging; 60 FPS on Baseline; feel ≥ 8/10 from 5 testers;
no traversal dead ends; the live-tuning panel works. [Section 5](#5-test-checklist) says how to measure each one.

---

## 1. Goal and assumptions

**Goal:** swinging that feels like the fantasy within the first minute (Pillar 1). Web-swing with automatic anchors,
web-zip, point-launch, wall-run, wall-cling/crawl, ledge vault, a glide stub, a speed-scaled camera, one data asset
holding every number, a live-tuning panel, a debug HUD, and a fast-travel stub.

**Assumptions**
- Phase 0's assumptions stand (Baseline PC, beginner). You said "do all as you wish", so I've treated the proposed
  keyboard + mouse layout as approved. Everything is rebindable once the settings menu exists (Phase 3).
- Phase 0's Definition of Done still has to be confirmed on your PC; Phase 1 builds on the same project.
- Placeholder visuals only: the Unreal mannequin, a thin cylinder for the web, a sphere for the anchor preview.
  Swing, wall-run and zip animations belong to the animation plan (Section 10, Phase 3).

**How it's built (and why)**
- The traversal logic (`FTraversalSim`) has no engine dependencies. It covers every state, the rope physics, the
  assists, anchor scoring and zips. That let me compile and test it here with **21 automated tests**, including
  bots that swing the whole greybox city. In the game, `UHeroMovementComponent` plugs it into Unreal's character
  movement:
  - walking stays Unreal's;
  - falling and every traversal state (swing, zip, wall-run and the rest) run the sim.
- The swing is a "hybrid", as Section 6.1 asks: a damped pendulum with designer assists. Those assists are:
  - a boost through the bottom of the arc;
  - a minimum-speed floor;
  - three speed-tier caps;
  - automatic rope shortening so the arc clears the street;
  - a swing pivot pulled over your line of travel, so arcs carry you along the street instead of into buildings.

---

## 2. Files

```
Source/WebOfTheCity/
  Public/Traversal/TraversalTypes.h       states, FTraversalTuning (all 104 numbers), sim input/output types
  Public/Traversal/TraversalSim.h         the state machine + physics (engine-independent)
  Private/Traversal/TraversalSim.cpp
  Public/Traversal/TraversalTuningAsset.h DA_TraversalTuning (data asset wrapping FTraversalTuning)
  Public/Traversal/HeroMovementComponent.h/.cpp   Unreal character movement <-> the sim
  Public/Traversal/HeroCharacter.h/.cpp   camera, controls, web line + anchor marker, fast travel, run log
  Public/Traversal/HeroDebugHUD.h/.cpp    stats HUD, live-tuning panel, Trav.* console commands
  Public/Core/WebOfTheCityGameMode.h/.cpp hero + HUD by default; uses BP_Hero if you make one
Config/DefaultEngine.ini                  GlobalDefaultGameMode = WebOfTheCityGameMode
Config/DefaultGame.ini                    always cook /Game/WebOfTheCity/Characters and /Traversal
Tools/Editor/make_greybox_city.py         + park with trees, anchor-tagged props, 4 fast-travel stations, --export
Tools/TraversalSim/                       tests for the sim outside Unreal
  run_tests.sh                            build + run (g++ or clang++)
  shim/                                   minimal FVector/FMath stand-ins so the engine code compiles here
  tests/traversal_tests.cpp               unit tests + route bots + dead-end scan
  tuning_table.py                         regenerates Docs/TraversalTuning.md from the header
Docs/TraversalTuning.md                   every tuning value, default and meaning (generated)
Docs/Playtests/Phase1_Playtest.md         playtest script (Section 21: observe, don't explain)
Docs/Playtests/Phase1_Survey.csv          feel survey sheet for 5 testers
```

---

## 3. Editor steps

**3.1 Update and build.** Get the new code (`git pull` once it's pushed), then right-click `WebOfTheCity.uproject` →
**Generate Visual Studio project files**. This step is needed because there are new source files. Then build
*Development Editor | Win64*.

**3.2 Rebuild the greybox.** Open `L_PerfRoute_Swing` → *Tools → Execute Python Script…* →
`Tools/Editor/make_greybox_city.py` → *File → Save All*. The log should end with:
`456 buildings, 136 water tanks, 24 park trees, 4 stations, … route 'Swing' 2486 m`.
The script now adds:
- a park (open space with trees you can web from);
- the tag `WebAnchor` on water tanks and tree tops (anchor props get a small score bonus);
- four fast-travel stations.

**3.3 Create the tuning asset (recommended).**
1. In the Content Drawer, create the folders `WebOfTheCity/Traversal/Data`.
2. Right-click → *Miscellaneous → Data Asset* → pick **TraversalTuningAsset** → name it **`DA_TraversalTuning`**.

The hero loads it automatically from that path. Without it, the hero uses built-in defaults and the tuning panel
still works, but changes can't be saved.

**3.4 Give the hero a body (recommended).**
1. If you haven't already, add the **Third Person** pack (Phase 0, step 3.5.6).
2. Create the folders `WebOfTheCity/Characters/Hero`.
3. Right-click → *Blueprint Class → All Classes* → **HeroCharacter** → name it **`BP_Hero`**.
4. Open it and select the **Mesh** component:
   - *Skeletal Mesh Asset*: search `SKM_Manny` (or `SKM_Quinn`);
   - *Anim Class*: the unarmed Anim Blueprint that came with the pack (search `ABP_`).
5. Compile and save.

The game mode uses `BP_Hero` automatically. Without it you play as a visible capsule.

**3.5 Play.** Press **Play**. The game mode is set project-wide, so any map works.

| Action | Keyboard + mouse | Gamepad |
|---|---|---|
| Swing (hold in the air) · sprint on the ground · wall-run on walls | **Left Shift** | RT, or L3 |
| Jump · web-zip (in the air) · point-launch (on a perch) · jump off wall or rope | **Space** | A |
| Glide on/off (stub) | G | D-pad up |
| Move / look | WASD / mouse | left / right stick |
| Fast travel to next station · respawn | T · F5 | D-pad right · — |
| Stats HUD · tuning panel | F1 · F2 | — |
| Tuning panel: select · change · restore | PgUp/PgDn · ←/→ · Backspace | — |

**How to swing:** jump, then hold Shift. Keep holding and the hero chains swings automatically. Let go at the bottom
of the arc, when the HUD shows "release now", for a **perfect release**: extra speed, and it counts toward the next
speed tier. Keep holding Shift when you hit a wall to run up it.

**3.6 Tune while playing.**
1. Press **F2**, pick a value with PgUp/PgDn and change it with ←/→. The change applies instantly (changed values
   turn green), and Backspace restores the value you started with.
2. Stop playing and **save `DA_TraversalTuning`**: it shows an asterisk in the Content Browser.

From the console (`` ` ``):
- `Trav.Dump` prints every value;
- `Trav.Set SwingGravityScale 2.2` sets one value;
- `Trav.Stats` prints your current, last and best runs.

---

## 4. Tuning table

All 104 values, with defaults and meanings, are in **[Docs/TraversalTuning.md](TraversalTuning.md)** (generated from
the code). These are the ones to reach for first:

| Setting | Default | Feel it changes |
|---|---|---|
| `SwingGravityScale` | 2.0 | Heavier = snappier, faster arcs (Section 6.1 range 1.5–2.5) |
| `SwingLowPointBoost` | 18 m/s² | How much push you get through the bottom of each swing |
| `SwingMinSpeed` | 12 m/s | The "never stall" floor |
| `SpeedTier1/2/3` | 25 / 45 / 70 m/s | Speed caps as chains build (Section 6.1) |
| `SwingsPerTier` | 3 | Chained swings per tier |
| `ReleaseWindowMinAngle` / `ReleaseWindowMaxAngle` | −5° / 35° | How forgiving the perfect release is |
| `SwingPlaneAlign` | 0.85 | 1 = swings go straight along your heading; 0 = raw pendulum toward the wall |
| `SwingGroundClearance` | 6 m | How close to the street the arc may dip |
| `AnchorReach` / `AnchorConeAngle` | 80 m / 90° | Where webs can reach (Section 6.1) |
| `AnchorIdealElevationMin` / `AnchorIdealElevationMax` | 35° / 55° | Preferred anchor height band (Section 6.1) |
| `SkyAnchorMaxInARow` | 2 | Emergency webs over open ground |
| `bAutoWebCatch` | on | Accessibility: auto web before a hard landing |
| `ZipMinDuration` / `ZipMaxDuration` | 0.3 / 0.5 s | Zip snappiness (Section 6.1) |
| `WallRunSpeed` | 16 m/s | Wall-run speed (roughly 19 s up a 300 m tower) |
| `CameraFovBase` / `CameraFovMax` | 80° / 105° | Speed-scaled FOV (Section 11) |

---

## 5. Test checklist

### Definition of Done

| DoD item | How to measure it | Pass when |
|---|---|---|
| **2 km of continuous swinging** | Swing without touching down; the HUD line "Airborne … m" turns green at 2,000 m. Every airborne run of 50 m or more is appended to `Saved/Traversal/Runs.csv` (distance, time, min/max speed, swings, perfect releases: Section 6.1's recorded test routes). | A tester (not you) gets a run of 2,000 m or more after 5 minutes of practice. |
| **60 FPS on Baseline** | While swinging fast through the city, open the console and type `stat unit`. Also run the nightly perf route (`Tools\Build\Nightly.ps1`). | Frame ≤ 16.6 ms while swinging, and the perf report is PASS with no regression. |
| **Feel ≥ 8/10 from 5 testers** | Run [the playtest script](Playtests/Phase1_Playtest.md) with 5 people and fill in `Docs/Playtests/Phase1_Survey.csv`. | The average of the "felt like a web-slinger" score is ≥ 8. |
| **No traversal dead ends** | Automated: the dead-end scan in `Tools/TraversalSim` (below). Manual: playtesters try to get stuck (park, tower tops, narrow streets). | Scan 40/40; no tester gets stuck with no way to keep moving. |
| **Live-tuning panel works** | F2 → change `SwingGravityScale` to 2.5 → the swing gets snappier right away → Backspace restores it → change it again → stop → save `DA_TraversalTuning` → Play → the value is still there. | All steps behave as described. |

### Functional checks (in the editor)

| # | Do this | Good looks like |
|---|---|---|
| 1 | Press Play | You spawn at the PlayerStart; the HUD shows `Ground`; the capsule or mannequin is visible. |
| 2 | Jump, then hold Shift | A web line appears within about half a second; the HUD shows `Swing`; the FOV widens with speed. |
| 3 | Keep holding Shift along an avenue for 30 s | Continuous chained swings, no street contact, the tier rises to 2. |
| 4 | Release when the HUD says "release now" | "PERFECT RELEASE" pops up; you gain speed; the tier climbs to 3 with a few of these. |
| 5 | In the air, look at a wall and press Space | A 0.3–0.5 s zip, then you cling to the wall. |
| 6 | Look at the top edge of a low building and press Space, then Space again as you land | Zip to the ledge, then "POINT LAUNCH" throws you forward and up. |
| 7 | Sprint (Shift) into a tall building | Wall-run straight up; a vault onto the roof at the top. |
| 8 | On a wall, let go of Shift and use WASD | Slow crawl; with no input you cling in place; Space jumps off. |
| 9 | Sprint into a 1–2 m obstacle | An automatic vault over it. |
| 10 | Fly into the park holding Shift | Webs to the trees, or up to 2 "sky anchor" swings; never stuck. |
| 11 | Fall from high without holding Shift | "auto web-catch" catches you before the street (turn it off with `Trav.Set bAutoWebCatch False`). |
| 12 | In the air, press G | Glide: slow sink, you bank toward the stick direction; Shift hands off to a swing. |
| 13 | Press T a few times, then F5 | A fade to each of the 4 stations, then back to the start. |
| 14 | `Trav.LatencyFlash 1`, then film the screen and keyboard at 240 fps | A white square appears the frame the key lands. Count frames: ≤ 19 frames at 240 fps means ≤ 80 ms (Section 4.3). |

### Automated tests (already passing here)

```
Tools/TraversalSim/run_tests.sh          (Linux, macOS, WSL or Git Bash with g++/clang++)
```

Result on this branch: **21 tests, 155 checks, 0 failures** (g++ and clang, `-Wall -Wextra -Werror`), including:

| Test | Result |
|---|---|
| Novice bot (only holds swing) on the 2.5 km perf route | **2,476 m, 0 touchdowns**, 30 swings, 131 s, speed 11–62 m/s, reached tier 2 |
| Skilled bot (times its releases, manages height) | **2,477 m, 0 touchdowns**, 44 swings, 33 perfect releases, 73 s, top speed 76 m/s, reached tier 3 |
| Dead-end scan: 40 random street starts, 20 s each | **40 / 40** kept swinging (or left town); landing in the park counts as fine |
| Park | A swing starts within 0.35 s of holding Shift |
| Rope / speed floor / release window / sky anchor / auto-catch / zip / point-launch / wall / vault / glide / coyote / camera | All pass |

---

## 6. Performance notes

- Anchor search costs up to 35 short traces per airborne frame (`AnchorSamplesYaw` × `AnchorSamplesPitch`), plus
  up to 3 probes. That should be a small fraction of the 8 ms game-thread budget, but it's an estimate. Measure it
  with `stat game` or Unreal Insights while swinging, and cut the samples if it shows up.
- No per-frame allocations in the sim. The web line and anchor marker are two static meshes, no VFX yet.
- The camera changes FOV, arm length and motion blur each frame, which has no rendering cost of its own. The wider
  FOV at speed does draw more of the city, and the perf route already flies at FOV 105 to measure that worst case.
- Nothing about Phase 1 changes the Phase 0 perf harness. The perf map now spawns the hero at the start, and the
  route camera still takes over the view.

---

## 7. Known limitations and next step

- **The Unreal glue has not been compiled.** The sim was compiled and tested here with g++ and clang. The glue has
  not: the movement component, character, HUD and game mode, about 900 lines. It uses standard, stable engine APIs.
  If the build fails, paste me the first error. The engine calls I'd check first are:
  - `UCharacterMovementComponent` overrides (`PhysFalling`, `PhysCustom`, `UpdateCharacterStateBeforeMovement`);
  - runtime-created Enhanced Input actions (`UInputAction::ValueType`, `UInputMappingContext::MapKey`);
  - `ExportTextItem_Direct` / `ImportText_Direct` in the tuning panel.
- **The bots swing through a box version of the city.** The capsule is approximated by a box. Unreal's rounded
  capsule slides more gracefully over edges, so real play should be at least as forgiving, but a playtest is the
  real proof.
- **No traversal animations yet.** The mannequin plays its falling or running poses while swinging. Section 10's
  animation work (motion matching, swing pose blending, IK) is planned with the vertical slice.
- **Glide is a stub:** a toggle, always unlocked, no animation.
- **Some Section 6.1 states aren't separate yet.** Perch happens at the end of a zip; dive is free-fall; Stagger and
  Interact arrive with combat (Phase 2).
- **Controls are built in code.** Rebinding and hold/toggle options come with the settings menu (Phase 3).
- **Single-player only.** The custom movement modes aren't network-predicted; that's Phase 5.
- **The defaults were tuned by the bots,** not by people yet. The playtests will move them.

**Next step:** confirm the Phase 0 and Phase 1 checklists on your PC. That means building, playing, running 5
playtests, and sending me the survey sheet and `Runs.csv` (or any build errors). I'll then retune from the data, and
we move to **Phase 2: combat prototype**.
