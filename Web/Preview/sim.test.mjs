// Checks the JavaScript port against the same bots as Tools/TraversalSim (C++).
// Run: node Web/Preview/sim.test.mjs
import { BoxWorld, Hero, Route, State, V, makeInput } from './sim.js';
import { LAYOUT } from './layout.js';
import { defaultTuning } from './tuning.js';

const DT = 1 / 60;
let failures = 0;
const check = (cond, msg) => { if (!cond) { failures++; console.log(`  FAIL ${msg}`); } };

const world = BoxWorld.fromLayout(LAYOUT);
const route = new Route(LAYOUT.route);
const start = new V(...LAYOUT.start);

function routeBot(skilled, maxTime = 240) {
  const hero = new Hero(world, defaultTuning());
  hero.placeOnGround(start);
  let hint = 0, t = 0, frames = 0;
  const traces0 = world.traceCount;
  for (; t < maxTime; t += DT, frames++) {
    hint = Math.max(hint, route.project(hero.body.position, hint));
    if (hint >= route.length - 5) break;
    const target = route.at(hint + 60);
    const dir = target.sub(hero.body.position).h().norm();
    const input = makeInput();
    input.cameraForward = dir; input.move = dir; input.traversalHeld = true; input.jumpPressed = t === 0;
    if (skilled && hero.sim.state === State.Swing && hero.body.position.z < 45) {
      const a = hero.sim.swingAngle(hero.body.position);
      if (a > hero.T.ReleaseWindowMinAngle + 10 && a < hero.T.ReleaseWindowMaxAngle - 5) input.traversalHeld = false;
    }
    hero.step(DT, input);
    if (hero.landings > 0 && t > 0.5) break;
  }
  const s = hero.landings > 0 ? hero.sim.lastStreak : hero.sim.streak;
  return { progress: hint, time: t, landings: hero.landings, streak: s, traces: (world.traceCount - traces0) / Math.max(1, frames) };
}

for (const skilled of [false, true]) {
  const r = routeBot(skilled);
  console.log(`${skilled ? 'skilled' : 'novice '}: ${r.progress.toFixed(0)} m in ${r.time.toFixed(1)} s, landings ${r.landings}, swings ${r.streak.swings}, ` +
    `perfect ${r.streak.perfectReleases}, speed max ${r.streak.maxSpeed.toFixed(1)} m/s, max tier ${r.streak.maxTier + 1}, ${r.traces.toFixed(1)} traces/frame`);
  check(r.progress >= 2000, `${skilled ? 'skilled' : 'novice'} bot reached only ${r.progress.toFixed(0)} m`);
  if (skilled) check(r.streak.maxTier === 2, 'skilled bot reaches tier 3');
}

// Dead-end scan: same starts as the C++ test (40 street starts, 20 s each).
{
  const g = LAYOUT.grid;
  const ex = g.blocksX * (g.blockX + g.avenue), ey = g.blocksY * (g.blockY + g.street);
  const [px0, py0, px1, py1] = LAYOUT.park;
  let seed = 1962;
  const rand = () => { seed = (seed * 1103515245 + 12345) % 2147483648; return seed; };
  let clean = 0;
  for (let i = 0; i < 40; i++) {
    const onAvenue = i % 2 === 0;
    let startPos, dir;
    if (onAvenue) {
      const x = (rand() % 11) * (g.blockX + g.avenue) + g.blockX + g.avenue / 2;
      const y = 100 + (rand() % 1000) / 1000 * (ey - 200);
      startPos = new V(x, y, 20); dir = new V(0, y < ey / 2 ? 1 : -1, 0);
    } else {
      const y = (rand() % 19) * (g.blockY + g.street) + g.blockY + g.street / 2;
      const x = 100 + (rand() % 1000) / 1000 * (ex - 200);
      startPos = new V(x, y, 20); dir = new V(x < ex / 2 ? 1 : -1, 0, 0);
    }
    const hero = new Hero(world, defaultTuning());
    hero.body.position = startPos; hero.body.velocity = dir.mul(15); hero.sim.resetToAir();
    for (let t = 0; t < 20 && hero.landings === 0; t += DT) {
      let aim = dir.mul(60);
      aim = onAvenue ? new V(startPos.x - hero.body.position.x, aim.y, 0) : new V(aim.x, startPos.y - hero.body.position.y, 0);
      const look = aim.norm();
      const input = makeInput(); input.cameraForward = look; input.move = look; input.traversalHeld = true;
      hero.step(DT, input);
      const p = hero.body.position;
      if (p.x < 20 || p.y < 20 || p.x > ex - 20 || p.y > ey - 20) break;
    }
    const p = hero.body.position;
    const inPark = p.x > px0 && p.x < px1 && p.y > py0 && p.y < py1;
    if (hero.landings === 0 || inPark) clean++;
  }
  console.log(`dead-end scan: ${clean} of 40 street starts kept swinging`);
  check(clean >= 38, 'dead-end scan');
}

// Ground: sprint into a tall wall -> wall-run -> vault onto the roof.
{
  const w = new BoxWorld();
  w.addBox(new V(0, 0, -0.5), new V(4000, 4000, 1));
  w.addBox(new V(20, 0, 7.5), new V(20, 200, 15));
  const hero = new Hero(w, defaultTuning());
  hero.placeOnGround(new V(9, 0, 0));
  let sawWallRun = false;
  for (let t = 0; t < 4; t += DT) {
    const input = makeInput(); input.move = new V(1, 0, 0); input.cameraForward = new V(1, 0, 0); input.traversalHeld = true;
    hero.step(DT, input);
    sawWallRun ||= hero.sim.state === State.WallRun;
    if (hero.sim.state === State.Ground && hero.body.position.z > 10) break;
  }
  check(sawWallRun && hero.sim.state === State.Ground && Math.abs(hero.body.position.z - 15.96) < 0.1, 'wall-run then vault onto roof');
}

console.log(failures === 0 ? 'OK' : `${failures} failure(s)`);
process.exit(failures === 0 ? 0 : 1);
