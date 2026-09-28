"""Greybox city + performance route generator (Phase 0 perf map, Phase 1 traversal greybox).

Run inside the Unreal Editor with the target level open:
    Tools > Execute Python Script...  (pick this file)
or in the Output Log, switch the input box from "Cmd" to "Python" and type:
    py "<repo>/Tools/Editor/make_greybox_city.py"

What it builds (Section 7 rules: heights 20-300 m, tall core, anchors everywhere):
  - a ~1.8 x 1.8 km grid of box buildings on a ground slab, taller towards the centre
  - rooftop water tanks, tagged "WebAnchor" (anchor props get a small score bonus)
  - a park (Central Park-style open space, few anchors) with trees whose canopies are anchors
  - four fast-travel stations (TargetPoints tagged "FastTravel"; press T in game)
  - sun, sky atmosphere, real-time sky light and height fog, if the level has none
  - a PlayerStart and a PerfRouteRunner ("Swing"): a ~2.5 km route through the street
    canyons at 25-55 m, the height band where swinging happens

Re-running is safe: everything it made earlier (tag WOTC_Greybox) is deleted first.
Layout is seeded, so every machine gets the same city and the perf numbers compare.

Outside Unreal:
    python make_greybox_city.py                 dry run: layout stats + route/building overlap check
    python make_greybox_city.py --export FILE   write the layout as CSV for Tools/TraversalSim tests
"""

import math
import random
import sys

try:
    import unreal
except ImportError:  # dry run outside the editor
    unreal = None

SEED = 1962
TAG = "WOTC_Greybox"
ANCHOR_TAG = "WebAnchor"
FAST_TRAVEL_TAG = "FastTravel"
UU_PER_M = 100.0

# ---- City layout, metres. Long blocks along X, avenues run along Y. ----
BLOCKS_X = 12
BLOCKS_Y = 20
BLOCK_X_M = 120.0
BLOCK_Y_M = 70.0
AVENUE_M = 30.0
STREET_M = 20.0
SIDEWALK_M = 4.0
BUILDINGS_PER_BLOCK = 2
BUILDING_GAP_M = 6.0
MIN_HEIGHT_M = 20.0
LOWRISE_MAX_M = 60.0
MAX_HEIGHT_M = 300.0
LANDMARK_TOWERS = 4
WATER_TANK_CHANCE = 0.35
WATER_TANK_SIZE_M = (6.0, 6.0, 8.0)

# ---- Park: blocks [i0..i1] x [j0..j1] have no buildings, just sparse trees ----
PARK_BLOCKS = (5, 7, 6, 9)
PARK_TREES = 24
TREE_TRUNK_M = (1.0, 1.0, 6.0)
TREE_CANOPY_M = (8.0, 8.0, 6.0)

# ---- Fast-travel stations: (avenue index, street index) intersections ----
STATIONS = [(1, 1), (9, 3), (3, 12), (10, 17)]

# ---- Perf route ----
ROUTE_NAME = "Swing"
ROUTE_AVENUE_INDEX = 1          # north leg runs up this avenue
ROUTE_TURN_STREET_INDEX = 16    # then turns east along this street
ROUTE_END_AVENUE_INDEX = 8      # and ends at this avenue
ROUTE_HEIGHT_M = 40.0           # centre of the swing band
ROUTE_WAVE_M = 15.0             # +/- swing arc height
ROUTE_WAVELENGTH_M = 120.0      # one swing arc per 120 m
ROUTE_CORNER_RADIUS_M = 10.0
ROUTE_POINT_SPACING_M = 25.0


# ------------------------------------------------------------------ pure layout (no Unreal)

def avenue_x(i):
    """Centre line of the avenue east of block column i."""
    return i * (BLOCK_X_M + AVENUE_M) + BLOCK_X_M + AVENUE_M / 2


def street_y(j):
    """Centre line of the street north of block row j."""
    return j * (BLOCK_Y_M + STREET_M) + BLOCK_Y_M + STREET_M / 2


def city_extent():
    return BLOCKS_X * (BLOCK_X_M + AVENUE_M), BLOCKS_Y * (BLOCK_Y_M + STREET_M)


def in_park(i, j):
    i0, i1, j0, j1 = PARK_BLOCKS
    return i0 <= i <= i1 and j0 <= j <= j1


def park_bounds():
    """(x0, y0, x1, y1) of the park in metres."""
    i0, i1, j0, j1 = PARK_BLOCKS
    return (i0 * (BLOCK_X_M + AVENUE_M), j0 * (BLOCK_Y_M + STREET_M),
            i1 * (BLOCK_X_M + AVENUE_M) + BLOCK_X_M, j1 * (BLOCK_Y_M + STREET_M) + BLOCK_Y_M)


def make_buildings(rng):
    """Dicts with centre (x, y), size (sx, sy), height and an optional roof tank centre (x, y), in metres."""
    ext_x, ext_y = city_extent()
    cx, cy = ext_x / 2, ext_y / 2
    max_dist = math.hypot(cx, cy)
    buildings = []
    for i in range(BLOCKS_X):
        for j in range(BLOCKS_Y):
            if in_park(i, j):
                continue
            x0 = i * (BLOCK_X_M + AVENUE_M) + SIDEWALK_M
            y0 = j * (BLOCK_Y_M + STREET_M) + SIDEWALK_M
            usable_x = BLOCK_X_M - 2 * SIDEWALK_M
            usable_y = BLOCK_Y_M - 2 * SIDEWALK_M
            width = (usable_x - BUILDING_GAP_M * (BUILDINGS_PER_BLOCK - 1)) / BUILDINGS_PER_BLOCK
            for k in range(BUILDINGS_PER_BLOCK):
                bx = x0 + k * (width + BUILDING_GAP_M) + width / 2
                by = y0 + usable_y / 2
                downtown = max(0.0, 1.0 - 1.4 * math.hypot(bx - cx, by - cy) / max_dist)
                height = rng.uniform(MIN_HEIGHT_M, LOWRISE_MAX_M) + (rng.random() ** 2) * (MAX_HEIGHT_M - LOWRISE_MAX_M) * downtown
                tank = None
                if rng.random() < WATER_TANK_CHANCE:
                    tank = (bx + rng.uniform(-width / 4, width / 4), by + rng.uniform(-usable_y / 4, usable_y / 4))
                buildings.append({
                    "centre": (bx, by),
                    "size": (width, usable_y),
                    "height": min(MAX_HEIGHT_M, height),
                    "tank": tank,
                })

    # A few hero towers near the centre (the Midtown skyline).
    central = sorted(buildings, key=lambda b: math.hypot(b["centre"][0] - cx, b["centre"][1] - cy))[: LANDMARK_TOWERS * 3]
    for b in rng.sample(central, LANDMARK_TOWERS):
        b["height"] = rng.uniform(260.0, MAX_HEIGHT_M)
        b["tank"] = None
    return buildings


def make_trees(rng):
    """Tree positions (x, y) in the park, kept away from its edges."""
    x0, y0, x1, y1 = park_bounds()
    margin = 15.0
    return [(rng.uniform(x0 + margin, x1 - margin), rng.uniform(y0 + margin, y1 - margin)) for _ in range(PARK_TREES)]


def make_layout():
    """Everything the city contains, as axis-aligned boxes (metres) plus points of interest."""
    rng = random.Random(SEED)
    buildings = make_buildings(rng)
    trees = make_trees(rng)
    ext_x, ext_y = city_extent()

    boxes = [("ground", (ext_x / 2, ext_y / 2, -0.5), (ext_x + 200, ext_y + 200, 1.0), False)]
    for b in buildings:
        (bx, by), (sx, sy), h = b["centre"], b["size"], b["height"]
        boxes.append(("building", (bx, by, h / 2), (sx, sy, h), False))
        if b["tank"]:
            tx, ty = b["tank"]
            boxes.append(("tank", (tx, ty, h + WATER_TANK_SIZE_M[2] / 2), WATER_TANK_SIZE_M, True))
    for tx, ty in trees:
        boxes.append(("trunk", (tx, ty, TREE_TRUNK_M[2] / 2), TREE_TRUNK_M, False))
        boxes.append(("canopy", (tx, ty, TREE_TRUNK_M[2] + TREE_CANOPY_M[2] / 2), TREE_CANOPY_M, True))

    points, route_len = route_points()
    stations = [(avenue_x(i), street_y(j), 0.0) for i, j in STATIONS]
    return {
        "buildings": buildings,
        "trees": trees,
        "boxes": boxes,
        "route": points,
        "route_length": route_len,
        "stations": stations,
        "start": (points[0][0], points[0][1], 0.0),
    }


def route_path_2d():
    """Dense (1 m) polyline: north up one avenue, rounded corner, east along one street."""
    ax = avenue_x(ROUTE_AVENUE_INDEX)
    y_start = street_y(0)
    y_turn = street_y(ROUTE_TURN_STREET_INDEX)
    x_end = avenue_x(ROUTE_END_AVENUE_INDEX)
    r = ROUTE_CORNER_RADIUS_M

    pts = []
    y = y_start
    while y < y_turn - r:
        pts.append((ax, y))
        y += 1.0
    centre = (ax + r, y_turn - r)
    for step in range(0, 91, 5):  # 180 deg -> 90 deg
        a = math.radians(180 - step)
        pts.append((centre[0] + r * math.cos(a), centre[1] + r * math.sin(a)))
    x = ax + r + 1.0
    while x <= x_end:
        pts.append((x, y_turn))
        x += 1.0
    return pts


def route_points():
    """Route points in metres (x, y, z), resampled every ROUTE_POINT_SPACING_M with swing-arc heights."""
    path = route_path_2d()
    out = []
    travelled = 0.0
    next_sample = 0.0
    prev = path[0]
    for p in path:
        travelled += math.hypot(p[0] - prev[0], p[1] - prev[1])
        prev = p
        if travelled >= next_sample or p is path[-1]:
            z = ROUTE_HEIGHT_M + ROUTE_WAVE_M * math.sin(2 * math.pi * travelled / ROUTE_WAVELENGTH_M)
            out.append((p[0], p[1], z))
            next_sample += ROUTE_POINT_SPACING_M
    return out, travelled


def route_collisions(points, boxes, clearance_m=2.0):
    """Route points that sit inside (or within clearance of) any solid box except the ground."""
    hits = []
    for p in points:
        for kind, (cx, cy, cz), (sx, sy, sz), _ in boxes:
            if kind == "ground":
                continue
            if (abs(p[0] - cx) < sx / 2 + clearance_m and abs(p[1] - cy) < sy / 2 + clearance_m
                    and abs(p[2] - cz) < sz / 2 + clearance_m):
                hits.append((p, kind))
    return hits


def export_csv(path):
    """Layout for the C++ traversal tests: one row per box, route point, station and the start."""
    layout = make_layout()
    lines = ["# Generated by Tools/Editor/make_greybox_city.py --export (seed %d). Metres." % SEED,
             "type,kind,x,y,z,sx,sy,sz,anchor"]
    for kind, (cx, cy, cz), (sx, sy, sz), anchor in layout["boxes"]:
        lines.append(f"box,{kind},{cx:.3f},{cy:.3f},{cz:.3f},{sx:.3f},{sy:.3f},{sz:.3f},{int(anchor)}")
    for x, y, z in layout["route"]:
        lines.append(f"route,,{x:.3f},{y:.3f},{z:.3f},0,0,0,0")
    for x, y, z in layout["stations"]:
        lines.append(f"station,,{x:.3f},{y:.3f},{z:.3f},0,0,0,0")
    x, y, z = layout["start"]
    lines.append(f"start,,{x:.3f},{y:.3f},{z:.3f},0,0,0,0")
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")
    return layout


# ------------------------------------------------------------------ Unreal side

def _vec(x_m, y_m, z_m):
    return unreal.Vector(x_m * UU_PER_M, y_m * UU_PER_M, z_m * UU_PER_M)


def _mark(actor, label, folder, always_loaded=False, extra_tags=()):
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_editor_property("tags", [unreal.Name(TAG)] + [unreal.Name(t) for t in extra_tags])
    if always_loaded:
        try:  # World Partition only; harmless elsewhere
            actor.set_editor_property("is_spatially_loaded", False)
        except Exception:
            pass
    return actor


def _spawn_shape(actors, mesh, label, folder, centre_m, size_m, always_loaded=False, extra_tags=()):
    # Engine basic shapes are 1 m across with a centred pivot, so scale == size in metres.
    actor = actors.spawn_actor_from_object(mesh, _vec(*centre_m))
    actor.set_actor_scale3d(unreal.Vector(*size_m))
    return _mark(actor, label, folder, always_loaded, extra_tags)


def _ensure_lighting(actors):
    existing = actors.get_all_level_actors()

    def has(cls):
        return any(isinstance(a, cls) for a in existing)

    if not has(unreal.DirectionalLight):
        sun = actors.spawn_actor_from_class(unreal.DirectionalLight, _vec(0, 0, 50), unreal.Rotator(roll=0.0, pitch=-40.0, yaw=35.0))
        comp = sun.get_editor_property("light_component")
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_editor_property("atmosphere_sun_light", True)
        _mark(sun, "Sun", "Greybox/Lighting", always_loaded=True)
    if not has(unreal.SkyAtmosphere):
        _mark(actors.spawn_actor_from_class(unreal.SkyAtmosphere, _vec(0, 0, 0)), "SkyAtmosphere", "Greybox/Lighting", True)
    if not has(unreal.SkyLight):
        sky = actors.spawn_actor_from_class(unreal.SkyLight, _vec(0, 0, 60))
        comp = sky.get_editor_property("light_component")
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_editor_property("real_time_capture", True)
        _mark(sky, "SkyLight", "Greybox/Lighting", always_loaded=True)
    if not has(unreal.ExponentialHeightFog):
        _mark(actors.spawn_actor_from_class(unreal.ExponentialHeightFog, _vec(0, 0, 0)), "HeightFog", "Greybox/Lighting", True)


def build_in_editor():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
    cylinder = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cylinder")
    sphere = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Sphere")

    old = [a for a in actors.get_all_level_actors() if a.actor_has_tag(TAG)]
    if old:
        actors.destroy_actors(old)
        unreal.log(f"Greybox: removed {len(old)} actors from the previous run.")

    layout = make_layout()
    ext_x, ext_y = city_extent()
    points = layout["route"]
    meshes = {"ground": cube, "building": cube, "tank": cylinder, "trunk": cylinder, "canopy": sphere}
    folders = {"ground": "Greybox", "building": "Greybox/Buildings", "tank": "Greybox/RooftopProps",
               "trunk": "Greybox/Park", "canopy": "Greybox/Park"}

    with unreal.ScopedSlowTask(len(layout["boxes"]) + 3, "Building greybox city") as task:
        task.make_dialog(True)
        _ensure_lighting(actors)

        for n, (kind, centre, size, anchor) in enumerate(layout["boxes"]):
            if task.should_cancel():
                unreal.log_warning("Greybox: cancelled. Re-run to finish.")
                return
            task.enter_progress_frame(1)
            _spawn_shape(actors, meshes[kind], f"{kind.capitalize()}_{n:04d}", folders[kind], centre, size,
                         always_loaded=(kind == "ground"), extra_tags=(ANCHOR_TAG,) if anchor else ())

        task.enter_progress_frame(1, "Player start and fast-travel stations")
        start = layout["start"]
        player_start = actors.spawn_actor_from_class(unreal.PlayerStart, _vec(start[0], start[1], 1.2), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
        _mark(player_start, "PlayerStart", "Greybox", always_loaded=True)
        for n, (x, y, z) in enumerate(layout["stations"]):
            station = actors.spawn_actor_from_class(unreal.TargetPoint, _vec(x, y, z + 1.2))
            _mark(station, f"Station_{n + 1}", "Greybox/FastTravel", always_loaded=True, extra_tags=(FAST_TRAVEL_TAG,))

        task.enter_progress_frame(1, "Perf route")
        runner_class = getattr(unreal, "PerfRouteRunner", None)
        if runner_class is None:
            unreal.log_warning("Greybox: PerfRouteRunner not found. Build the C++ project (Development Editor) and re-run to add the route.")
        else:
            runner = actors.spawn_actor_from_class(runner_class, _vec(*points[0]))
            runner.set_editor_property("route_name", ROUTE_NAME)
            spline = runner.get_editor_property("route")
            spline.set_spline_points([_vec(*p) for p in points], unreal.SplineCoordinateSpace.WORLD, True)
            _mark(runner, f"PerfRoute_{ROUTE_NAME}", "Greybox/Perf", always_loaded=True)

    tanks = sum(1 for b in layout["buildings"] if b["tank"])
    unreal.log(f"Greybox: {len(layout['buildings'])} buildings, {tanks} water tanks, {len(layout['trees'])} park trees, "
               f"{len(layout['stations'])} stations, {ext_x / 1000:.1f} x {ext_y / 1000:.1f} km, "
               f"route '{ROUTE_NAME}' {layout['route_length']:.0f} m. Now use File > Save All.")


def dry_run():
    layout = make_layout()
    buildings = layout["buildings"]
    points = layout["route"]
    ext_x, ext_y = city_extent()
    heights = sorted(b["height"] for b in buildings)
    hits = route_collisions(points, layout["boxes"])
    print(f"City {ext_x:.0f} x {ext_y:.0f} m ({ext_x * ext_y / 1e6:.2f} km^2), {len(buildings)} buildings, "
          f"{sum(1 for b in buildings if b['tank'])} water tanks, {len(layout['trees'])} park trees, "
          f"{len(layout['stations'])} fast-travel stations")
    print(f"Heights: min {heights[0]:.0f} m, median {heights[len(heights) // 2]:.0f} m, max {heights[-1]:.0f} m")
    print(f"Route '{ROUTE_NAME}': {layout['route_length']:.0f} m, {len(points)} spline points, "
          f"z {min(p[2] for p in points):.0f}-{max(p[2] for p in points):.0f} m, "
          f"{layout['route_length'] / 60:.0f} s at 60 m/s")
    print(f"Route points inside buildings or props: {len(hits)}")
    return 1 if hits else 0


if unreal is not None:
    build_in_editor()
elif __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--export":
        exported = export_csv(sys.argv[2])
        print(f"Exported {len(exported['boxes'])} boxes, {len(exported['route'])} route points to {sys.argv[2]}")
    else:
        raise SystemExit(dry_run())
