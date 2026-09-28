# Performance test-route plan

Sections 4.3, 17 and 21: fixed routes, measured on every nightly build, with regression alerts. The
harness is `APerfRouteRunner` (camera on a spline, one CSV row per frame) + `Tools/Perf/analyze_perf.py`
(gates + baseline comparison) + `Tools/Build/Nightly.ps1` (package, run, report).

## Gates (from `Tools/Perf/budgets.json`, Baseline tier)

| Metric | Budget |
|---|---|
| Average frame time | ≤ 16.6 ms (60 FPS) |
| Average GPU / game thread / render thread | ≤ 14 / 8 / 8 ms |
| 1% low | ≥ 45 FPS |
| Hitches | none over 50 ms after warm-up |
| Peak process RAM | ≤ 12 GB |
| Regression | no metric more than 5% worse than this PC's baseline, no new hitches |

## Routes

| Route | Built in | Map | What it does | Why |
|---|---|---|---|---|
| **Swing** | Phase 0 (now) | `L_PerfRoute_Swing` greybox; the real district from Phase 3 | 2.5 km through street canyons at 25–55 m, 60 m/s, FOV 105 | Top-speed traversal: streaming, pop-in and hitch gate |
| **Combat arena** | Phase 2 | combat test map | Short slow orbit (5 m/s) + 60 s hold over a 1-versus-10 fight with max AI (12 active) and VFX | Game thread (AI, abilities) and VFX budgets |
| **Crowd plaza** | Phase 3–4 | slice district | Street-level walk at 2 m/s + 60 s hold in the busiest plaza (≈300 crowd agents, traffic) | Mass crowd/traffic cost, draw calls |
| **Streaming stress** | Phase 3 | full district | Straight 60 m/s dive + run across World Partition cell and district borders, night + rain | Worst-case streaming and MegaLights |
| **Soak** | Phase 3 | slice district | Loops Swing for 2 hours | Memory leaks: RAM must not keep climbing |

Each route is just another `PerfRouteRunner` actor with its own `RouteName`, run with
`-PerfRoute=<RouteName>`. Add one to `Nightly.ps1` by calling it again with `-Route <Name> -Map <Map> -SkipBuild`.

## Traversal bots (Phase 1)

`Tools/TraversalSim/run_tests.sh` runs the traversal core against the same greybox city (exported by
`make_greybox_city.py --export`). A novice bot (only holds swing) and a skilled bot (times its releases) each fly
the Swing route, and a dead-end scan tries 40 random street starts. Section 21's "traversal bots that swing test
routes" start here. They catch feel regressions (touchdowns, stalls, dead ends) and report traces per frame, the
main CPU cost of traversal.

## How a measurement is taken

1. Packaged **Development** build (never Debug, never the editor for gate numbers).
2. Same machine state every time: plugged in, high-performance power plan, no other heavy apps,
   same resolution (1920x1080 windowed by default), VSync off and FPS uncapped by the runner.
3. 5 s warm-up at the start (recorded, not gated) so first-frame loading doesn't count as a hitch.
   Shader/PSO hitches after the warm-up *do* count: that is the stutter Section 4.4 wants gone.
4. The CSV header records machine, GPU, CPU, RAM, resolution, screen percentage, build and engine
   version, so a report always says exactly what was measured.

## Policy

- Nightly fails → look at `perf_report.md` (the hitch table gives the distance along the route; fly
  there in the editor) and the CSV profiler capture in `Saved/Profiling/CSV` for the same run.
- A regression is fixed or explained before the change merges. Baselines move only on purpose.
- Hardware matrix (Section 21): one machine per tier eventually. Each PC keeps its own baseline under
  `Tools/Perf/Baselines/<COMPUTERNAME>/`.

## Not covered yet

VRAM (≤ 8 GB), input latency (≤ ~80 ms), cold boot (≤ 25 s), fast travel (≤ 5 s) and respawn (≤ 3 s)
need systems that don't exist yet; they join the harness in Phases 1 (latency) and 3 (the rest).
