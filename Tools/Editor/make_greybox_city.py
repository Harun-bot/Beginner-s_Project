"""Greybox city + performance route generator (Phase 0; reused as the Phase 1 traversal greybox).

Run inside the Unreal Editor with the target level open:
    Tools > Execute Python Script...  (pick this file)
or in the Output Log, switch the input box from "Cmd" to "Python" and type:
    py "<repo>/Tools/Editor/make_greybox_city.py"

What it builds (Section 7 rules: heights 20-300 m, tall core, anchors everywhere):
  - a ~1.8 x 1.8 km grid of box buildings on a ground slab, taller towards the centre
  - rooftop water tanks (future web-anchor props)
  - sun, sky atmosphere, real-time sky light and height fog, if the level has none
  - a PlayerStart and a PerfRouteRunner ("Swing"): a ~2.5 km route through the street
    canyons at 25-55 m, the height band where swinging happens

Re-running is safe: everything it made earlier (tag WOTC_Greybox) is deleted first.
Layout is seeded, so every machine gets the same city and the perf numbers compare.

Outside Unreal (plain `python make_greybox_city.py`) it does a dry run: prints the layout
stats and checks the route never passes through a building.
"""

import math
import random

try:
    import unreal
except ImportError:  # dry run outside the editor
    unreal = None

SEED = 1962
TAG = "WOTC_Greybox"
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


def make_buildings(rng):
    """Returns dicts with centre (x, y), size (sx, sy), height and roof tank flag, all in metres."""
    ext_x, ext_y = city_extent()
    cx, cy = ext_x / 2, ext_y / 2
    max_dist = math.hypot(cx, cy)
    buildings = []
    for i in range(BLOCKS_X):
        for j in range(BLOCKS_Y):
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
                buildings.append({
                    "centre": (bx, by),
                    "size": (width, usable_y),
                    "height": min(MAX_HEIGHT_M, height),
                    "tank": rng.random() < WATER_TANK_CHANCE,
                })

    # A few hero towers near the centre (the Midtown skyline).
    central = sorted(buildings, key=lambda b: math.hypot(b["centre"][0] - cx, b["centre"][1] - cy))[: LANDMARK_TOWERS * 3]
    for b in rng.sample(central, LANDMARK_TOWERS):
        b["height"] = rng.uniform(260.0, MAX_HEIGHT_M)
        b["tank"] = False
    return buildings


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


def route_collisions(points, buildings, clearance_m=2.0):
    """Route points that sit inside (or within clearance of) a building volume."""
    hits = []
    for p in points:
        for b in buildings:
            (bx, by), (sx, sy) = b["centre"], b["size"]
            if (abs(p[0] - bx) < sx / 2 + clearance_m and abs(p[1] - by) < sy / 2 + clearance_m
                    and p[2] < b["height"] + clearance_m):
                hits.append((p, b))
    return hits


# ------------------------------------------------------------------ Unreal side

def _vec(x_m, y_m, z_m):
    return unreal.Vector(x_m * UU_PER_M, y_m * UU_PER_M, z_m * UU_PER_M)


def _mark(actor, label, folder, always_loaded=False):
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_editor_property("tags", [unreal.Name(TAG)])
    if always_loaded:
        try:  # World Partition only; harmless elsewhere
            actor.set_editor_property("is_spatially_loaded", False)
        except Exception:
            pass
    return actor


def _spawn_box(actors, mesh, label, folder, centre_m, size_m, always_loaded=False):
    # Engine basic shapes are 1 m across with a centred pivot, so scale == size in metres.
    actor = actors.spawn_actor_from_object(mesh, _vec(*centre_m))
    actor.set_actor_scale3d(unreal.Vector(*size_m))
    return _mark(actor, label, folder, always_loaded)


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

    old = [a for a in actors.get_all_level_actors() if a.actor_has_tag(TAG)]
    if old:
        actors.destroy_actors(old)
        unreal.log(f"Greybox: removed {len(old)} actors from the previous run.")

    rng = random.Random(SEED)
    buildings = make_buildings(rng)
    ext_x, ext_y = city_extent()
    points, route_len = route_points()

    with unreal.ScopedSlowTask(len(buildings) + 3, "Building greybox city") as task:
        task.make_dialog(True)

        task.enter_progress_frame(1, "Ground and lighting")
        _spawn_box(actors, cube, "Ground", "Greybox", (ext_x / 2, ext_y / 2, -0.5), (ext_x + 200, ext_y + 200, 1.0), always_loaded=True)
        _ensure_lighting(actors)

        for n, b in enumerate(buildings):
            if task.should_cancel():
                unreal.log_warning("Greybox: cancelled. Re-run to finish.")
                return
            task.enter_progress_frame(1)
            (bx, by), (sx, sy), h = b["centre"], b["size"], b["height"]
            _spawn_box(actors, cube, f"Bldg_{n:03d}", "Greybox/Buildings", (bx, by, h / 2), (sx, sy, h))
            if b["tank"]:
                tx = bx + rng.uniform(-sx / 4, sx / 4)
                ty = by + rng.uniform(-sy / 4, sy / 4)
                _spawn_box(actors, cylinder, f"Tank_{n:03d}", "Greybox/RooftopProps",
                           (tx, ty, h + WATER_TANK_SIZE_M[2] / 2), WATER_TANK_SIZE_M)

        task.enter_progress_frame(1, "Player start")
        start = points[0]
        player_start = actors.spawn_actor_from_class(unreal.PlayerStart, _vec(start[0], start[1], 1.2), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
        _mark(player_start, "PlayerStart", "Greybox", always_loaded=True)

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

    tanks = sum(1 for b in buildings if b["tank"])
    unreal.log(f"Greybox: {len(buildings)} buildings, {tanks} water tanks, {ext_x / 1000:.1f} x {ext_y / 1000:.1f} km, "
               f"route '{ROUTE_NAME}' {route_len:.0f} m. Now use File > Save All.")


def dry_run():
    rng = random.Random(SEED)
    buildings = make_buildings(rng)
    points, route_len = route_points()
    ext_x, ext_y = city_extent()
    heights = sorted(b["height"] for b in buildings)
    hits = route_collisions(points, buildings)
    print(f"City {ext_x:.0f} x {ext_y:.0f} m ({ext_x * ext_y / 1e6:.2f} km^2), {len(buildings)} buildings, "
          f"{sum(b['tank'] for b in buildings)} water tanks")
    print(f"Heights: min {heights[0]:.0f} m, median {heights[len(heights) // 2]:.0f} m, max {heights[-1]:.0f} m")
    print(f"Route '{ROUTE_NAME}': {route_len:.0f} m, {len(points)} spline points, "
          f"z {min(p[2] for p in points):.0f}-{max(p[2] for p in points):.0f} m, "
          f"{route_len / 60:.0f} s at 60 m/s")
    print(f"Route points inside buildings: {len(hits)}")
    return 1 if hits else 0


if unreal is not None:
    build_in_editor()
elif __name__ == "__main__":
    raise SystemExit(dry_run())
