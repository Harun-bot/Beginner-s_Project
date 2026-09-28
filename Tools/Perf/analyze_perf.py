#!/usr/bin/env python3
"""Check a PerfRouteRunner CSV against the Section 4.3 budgets and an optional baseline.

Usage:
    python Tools/Perf/analyze_perf.py <csv-or-folder> [--baseline FILE] [--write-baseline FILE]
                                      [--budgets FILE] [--report FILE]

<csv-or-folder> is a Saved/Perf/PerfRoute_*.csv file, or a folder (the newest PerfRoute_*.csv
in it is used). Exit code: 0 = all gates pass, 1 = a gate failed or a metric regressed,
2 = bad input. Standard library only, so it runs anywhere Python 3.9+ is installed.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path

REQUIRED_COLUMNS = ("frame", "time_s", "phase", "distance_m", "frame_ms", "game_ms", "render_ms", "gpu_ms", "ram_mb")
DEFAULT_BUDGETS = Path(__file__).with_name("budgets.json")


class InputError(Exception):
    pass


@dataclass
class Run:
    path: Path
    metadata: dict[str, str]
    rows: list[dict[str, float | str]]
    warmup_frames: int


@dataclass
class Gate:
    name: str
    budget: str
    measured: str
    status: str  # PASS, FAIL or N/A


@dataclass
class Result:
    run: Run
    metrics: dict[str, float]
    gates: list[Gate]
    hitches: list[dict[str, float | str]]
    regressions: list[Gate] = field(default_factory=list)

    @property
    def passed(self) -> bool:
        return all(g.status != "FAIL" for g in self.gates + self.regressions)


# ---------------------------------------------------------------- parsing

def resolve_csv(path: Path) -> Path:
    if path.is_dir():
        candidates = sorted(path.glob("PerfRoute_*.csv"), key=lambda p: p.stat().st_mtime)
        if not candidates:
            raise InputError(f"No PerfRoute_*.csv files in {path}")
        return candidates[-1]
    if not path.is_file():
        raise InputError(f"File not found: {path}")
    return path


def parse_metadata(line: str) -> dict[str, str]:
    metadata = {}
    for pair in line.lstrip("#").strip().split(";"):
        key, sep, value = pair.partition("=")
        if sep:
            metadata[key.strip()] = value.strip()
    return metadata


def load_run(path: Path) -> Run:
    metadata: dict[str, str] = {}
    data_lines: list[str] = []
    with path.open(encoding="utf-8-sig", newline="") as handle:
        for line in handle:
            if line.startswith("#"):
                metadata.update(parse_metadata(line))
            elif line.strip():
                data_lines.append(line)

    if not data_lines:
        raise InputError(f"{path} has no header row")

    reader = csv.DictReader(data_lines)
    missing = [c for c in REQUIRED_COLUMNS if c not in (reader.fieldnames or [])]
    if missing:
        raise InputError(f"{path} is missing columns: {', '.join(missing)}")

    rows: list[dict[str, float | str]] = []
    warmup = 0
    for line_no, raw in enumerate(reader, start=2):
        try:
            row: dict[str, float | str] = {k: float(raw[k]) for k in REQUIRED_COLUMNS if k != "phase"}
        except (TypeError, ValueError) as exc:
            raise InputError(f"{path}: bad number on data line {line_no}: {exc}") from exc
        row["phase"] = raw["phase"].strip()
        if row["phase"] == "warmup":
            warmup += 1
        else:
            rows.append(row)

    if not rows:
        raise InputError(f"{path} has no measured (non-warmup) frames")
    return Run(path=path, metadata=metadata, rows=rows, warmup_frames=warmup)


# ---------------------------------------------------------------- statistics

def mean(values: list[float]) -> float:
    return sum(values) / len(values)


def percentile(values: list[float], pct: float) -> float:
    """Linear-interpolated percentile (same as numpy's default)."""
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    rank = (len(ordered) - 1) * pct / 100.0
    low = math.floor(rank)
    high = math.ceil(rank)
    return ordered[low] + (ordered[high] - ordered[low]) * (rank - low)


def one_percent_low_fps(frame_ms: list[float]) -> float:
    """FPS of the average of the slowest 1% of frames (at least one frame)."""
    slowest = sorted(frame_ms, reverse=True)[: max(1, math.ceil(len(frame_ms) * 0.01))]
    return 1000.0 / mean(slowest)


def compute_metrics(run: Run) -> dict[str, float]:
    col = {name: [float(r[name]) for r in run.rows] for name in ("frame_ms", "game_ms", "render_ms", "gpu_ms", "ram_mb")}
    frame = col["frame_ms"]
    return {
        "frames": float(len(frame)),
        "duration_s": sum(frame) / 1000.0,
        "distance_m": max(float(r["distance_m"]) for r in run.rows),
        "avg_frame_ms": mean(frame),
        "avg_fps": 1000.0 / mean(frame),
        "p50_frame_ms": percentile(frame, 50),
        "p95_frame_ms": percentile(frame, 95),
        "p99_frame_ms": percentile(frame, 99),
        "max_frame_ms": max(frame),
        "one_percent_low_fps": one_percent_low_fps(frame),
        "avg_game_ms": mean(col["game_ms"]),
        "avg_render_ms": mean(col["render_ms"]),
        "avg_gpu_ms": mean(col["gpu_ms"]),
        "peak_ram_mb": max(col["ram_mb"]),
    }


# ---------------------------------------------------------------- gates

def upper_gate(name: str, value: float, limit: float, unit: str, measured_ok: bool = True) -> Gate:
    if not measured_ok:
        return Gate(name, f"<= {limit:g} {unit}", "not measured", "N/A")
    return Gate(name, f"<= {limit:g} {unit}", f"{value:.2f} {unit}", "PASS" if value <= limit else "FAIL")


def evaluate(run: Run, budgets: dict) -> Result:
    m = compute_metrics(run)
    hitch_ms = float(budgets["hitch_ms"])
    hitches = [r for r in run.rows if float(r["frame_ms"]) > hitch_ms]

    # A column of zeros means the platform did not report that timing; don't pass or fail on it.
    def measured(column: str) -> bool:
        return any(float(r[column]) > 0 for r in run.rows)

    gates = [
        upper_gate("Average frame time", m["avg_frame_ms"], budgets["avg_frame_ms"], "ms"),
        upper_gate("Average GPU time", m["avg_gpu_ms"], budgets["avg_gpu_ms"], "ms", measured("gpu_ms")),
        upper_gate("Average game thread", m["avg_game_ms"], budgets["avg_game_ms"], "ms", measured("game_ms")),
        upper_gate("Average render thread", m["avg_render_ms"], budgets["avg_render_ms"], "ms", measured("render_ms")),
        Gate(
            "1% low FPS",
            f">= {budgets['min_one_percent_low_fps']:g} FPS",
            f"{m['one_percent_low_fps']:.1f} FPS",
            "PASS" if m["one_percent_low_fps"] >= budgets["min_one_percent_low_fps"] else "FAIL",
        ),
        Gate(
            f"Hitches > {hitch_ms:g} ms",
            f"<= {budgets['max_hitches']}",
            str(len(hitches)),
            "PASS" if len(hitches) <= budgets["max_hitches"] else "FAIL",
        ),
        upper_gate("Peak process RAM", m["peak_ram_mb"], budgets["peak_ram_mb"], "MB", measured("ram_mb")),
    ]
    return Result(run=run, metrics=m, gates=gates, hitches=hitches)


# Metrics compared against the baseline, and whether a bigger number is worse.
REGRESSION_METRICS = {
    "avg_frame_ms": True,
    "p95_frame_ms": True,
    "avg_gpu_ms": True,
    "avg_game_ms": True,
    "avg_render_ms": True,
    "peak_ram_mb": True,
    "one_percent_low_fps": False,
}


def compare_to_baseline(result: Result, baseline: dict, tolerance_pct: float) -> list[Gate]:
    gates = []
    base_metrics = baseline.get("metrics", {})
    for name, higher_is_worse in REGRESSION_METRICS.items():
        if name not in base_metrics:
            continue
        old, new = float(base_metrics[name]), result.metrics[name]
        if old <= 0:
            continue
        change_pct = (new - old) / old * 100.0
        worse_pct = change_pct if higher_is_worse else -change_pct
        gates.append(Gate(
            name,
            f"<= {tolerance_pct:g}% worse than {old:.2f}",
            f"{new:.2f} ({change_pct:+.1f}%)",
            "FAIL" if worse_pct > tolerance_pct else "PASS",
        ))

    old_hitches = int(baseline.get("hitch_count", 0))
    gates.append(Gate(
        "hitch_count",
        f"<= {old_hitches} (baseline)",
        str(len(result.hitches)),
        "FAIL" if len(result.hitches) > old_hitches else "PASS",
    ))
    return gates


def baseline_payload(result: Result) -> dict:
    meta = result.run.metadata
    return {
        "route": meta.get("route", ""),
        "machine": meta.get("machine", ""),
        "gpu": meta.get("gpu", ""),
        "cpu": meta.get("cpu", ""),
        "resolution": meta.get("resolution", ""),
        "build": meta.get("build", ""),
        "engine": meta.get("engine", ""),
        "recorded": meta.get("date", ""),
        "source_csv": result.run.path.name,
        "hitch_count": len(result.hitches),
        "metrics": {k: round(v, 3) for k, v in result.metrics.items()},
    }


# ---------------------------------------------------------------- report

def render_report(result: Result, baseline: dict | None) -> str:
    meta, m = result.run.metadata, result.metrics
    out = [
        f"# Perf report: route `{meta.get('route', '?')}`",
        "",
        f"- File: `{result.run.path.name}`",
        f"- Machine: {meta.get('machine', '?')} | GPU: {meta.get('gpu', '?')} | CPU: {meta.get('cpu', '?')} ({meta.get('cores', '?')} cores) | RAM: {meta.get('ram_gb', '?')} GB",
        f"- Build: {meta.get('build', '?')} | Engine: {meta.get('engine', '?')} | RHI: {meta.get('rhi', '?')} | Resolution: {meta.get('resolution', '?')} @ {meta.get('screen_percentage', '?')}%",
        f"- Route: {m['distance_m']:.0f} m at {meta.get('speed_mps', '?')} m/s | {int(m['frames'])} frames measured over {m['duration_s']:.1f} s ({result.run.warmup_frames} warm-up frames excluded)",
        "",
        "## Section 4.3 gates",
        "",
        "| Gate | Budget | Measured | Result |",
        "|---|---|---|---|",
    ]
    out += [f"| {g.name} | {g.budget} | {g.measured} | {g.status} |" for g in result.gates]
    out += [
        "",
        f"Frame time: avg {m['avg_frame_ms']:.2f} ms ({m['avg_fps']:.0f} FPS), p50 {m['p50_frame_ms']:.2f}, "
        f"p95 {m['p95_frame_ms']:.2f}, p99 {m['p99_frame_ms']:.2f}, max {m['max_frame_ms']:.2f} ms.",
    ]

    if result.hitches:
        out += ["", "## Hitches", "", "| Frame | Time (s) | Distance (m) | Frame (ms) | Game | Render | GPU |", "|---|---|---|---|---|---|---|"]
        for r in result.hitches[:20]:
            out.append(f"| {int(r['frame'])} | {r['time_s']:.2f} | {r['distance_m']:.0f} | {r['frame_ms']:.1f} | "
                       f"{r['game_ms']:.1f} | {r['render_ms']:.1f} | {r['gpu_ms']:.1f} |")
        if len(result.hitches) > 20:
            out.append(f"| ... | {len(result.hitches) - 20} more | | | | | |")

    if baseline is not None:
        out += ["", f"## Regression vs baseline ({baseline.get('source_csv', '?')}, {baseline.get('recorded', '?')})", ""]
        for key in ("route", "machine", "resolution", "build"):
            if baseline.get(key) and meta.get(key) and baseline[key] != meta[key]:
                out.append(f"> Warning: baseline {key} is `{baseline[key]}` but this run is `{meta[key]}`; the comparison may not be fair.")
        out += ["", "| Metric | Allowed | Measured | Result |", "|---|---|---|---|"]
        out += [f"| {g.name} | {g.budget} | {g.measured} | {g.status} |" for g in result.regressions]

    out += ["", f"**Verdict: {'PASS' if result.passed else 'FAIL'}**", ""]
    return "\n".join(out)


# ---------------------------------------------------------------- CLI

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csv", type=Path, help="PerfRoute CSV, or a folder to take the newest one from")
    parser.add_argument("--budgets", type=Path, default=DEFAULT_BUDGETS, help="budget JSON (default: budgets.json next to this script)")
    parser.add_argument("--baseline", type=Path, help="baseline JSON to check for regressions (skipped if the file does not exist)")
    parser.add_argument("--write-baseline", type=Path, help="save this run as the new baseline JSON")
    parser.add_argument("--report", type=Path, help="also write the markdown report to this file")
    args = parser.parse_args(argv)

    try:
        budgets = json.loads(args.budgets.read_text(encoding="utf-8"))
        run = load_run(resolve_csv(args.csv))
    except (InputError, OSError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    result = evaluate(run, budgets)

    baseline = None
    if args.baseline and args.baseline.is_file():
        baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
        result.regressions = compare_to_baseline(result, baseline, float(budgets["regression_tolerance_pct"]))
    elif args.baseline:
        print(f"note: no baseline at {args.baseline} yet; run with --write-baseline to record one.", file=sys.stderr)

    report = render_report(result, baseline)
    print(report)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(report, encoding="utf-8")
    if args.write_baseline:
        args.write_baseline.parent.mkdir(parents=True, exist_ok=True)
        args.write_baseline.write_text(json.dumps(baseline_payload(result), indent=2) + "\n", encoding="utf-8")
        print(f"Baseline written to {args.write_baseline}", file=sys.stderr)

    return 0 if result.passed else 1


if __name__ == "__main__":
    sys.exit(main())
