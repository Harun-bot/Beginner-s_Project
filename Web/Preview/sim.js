// Traversal core for the browser preview: a line-by-line port of
// Source/WebOfTheCity/Private/Traversal/TraversalSim.cpp (same states, tuning names and behaviour),
// plus the box world and host from Tools/TraversalSim/tests. Metres, seconds, Z up.
// Verified by Web/Preview/sim.test.mjs, which runs the same bots as the C++ tests.

export const State = Object.freeze({
  Ground: 'Ground', Air: 'Air', Swing: 'Swing', Zip: 'Zip', Launch: 'Launch',
  Glide: 'Glide', WallRun: 'WallRun', WallCling: 'WallCling', Vault: 'Vault',
});

const DEG = Math.PI / 180;

// ------------------------------------------------------------------ vectors

export class V {
  constructor(x = 0, y = 0, z = 0) { this.x = x; this.y = y; this.z = z; }
  add(o) { return new V(this.x + o.x, this.y + o.y, this.z + o.z); }
  sub(o) { return new V(this.x - o.x, this.y - o.y, this.z - o.z); }
  mul(s) { return new V(this.x * s, this.y * s, this.z * s); }
  div(s) { const r = 1 / s; return new V(this.x * r, this.y * r, this.z * r); }
  neg() { return new V(-this.x, -this.y, -this.z); }
  dot(o) { return this.x * o.x + this.y * o.y + this.z * o.z; }
  cross(o) { return new V(this.y * o.z - this.z * o.y, this.z * o.x - this.x * o.z, this.x * o.y - this.y * o.x); }
  len2() { return this.x * this.x + this.y * this.y + this.z * this.z; }
  len() { return Math.sqrt(this.len2()); }
  len2D() { return Math.sqrt(this.x * this.x + this.y * this.y); }
  h() { return new V(this.x, this.y, 0); }
  // Same contract as FVector::GetSafeNormal(UE_SMALL_NUMBER, ResultIfZero).
  norm(ifZero = ZERO) {
    const s = this.len2();
    if (s === 1) return this;
    if (s < 1e-8) return ifZero;
    const k = 1 / Math.sqrt(s);
    return new V(this.x * k, this.y * k, this.z * k);
  }
  nearlyZero(t = 1e-4) { return Math.abs(this.x) <= t && Math.abs(this.y) <= t && Math.abs(this.z) <= t; }
  planeProject(n) { return this.sub(n.mul(this.dot(n))); }
  clone() { return new V(this.x, this.y, this.z); }
  static dist(a, b) { return a.sub(b).len(); }
  static lerp(a, b, t) { return a.add(b.sub(a).mul(t)); }
}
export const ZERO = Object.freeze(new V(0, 0, 0));
export const UP = Object.freeze(new V(0, 0, 1));
export const FORWARD = Object.freeze(new V(1, 0, 0));

const clamp = (x, lo, hi) => (x < lo ? lo : x < hi ? x : hi);
const lerp = (a, b, t) => a + t * (b - a);
const rotateZ = (v, r) => { const c = Math.cos(r), s = Math.sin(r); return new V(v.x * c - v.y * s, v.x * s + v.y * c, v.z); };

function freshAnchor() {
  return { valid: false, pivot: ZERO, surface: ZERO, normal: UP, score: 0, sky: false };
}
function freshStreak() {
  return { distance: 0, time: 0, minSpeed: 0, maxSpeed: 0, maxTier: 0, swings: 0, perfectReleases: 0 };
}
function freshEvents() {
  return { swingStarted: false, released: false, perfectRelease: false, tierUp: false, skyAnchor: false, autoCatch: false,
    zip: false, launch: false, wallAttach: false, vault: false, landed: false, streakEnded: false };
}

// ------------------------------------------------------------------ the state machine (FTraversalSim)

export class TraversalSim {
  constructor(tuning) {
    this.T = tuning;
    this.state = State.Ground; this.stateTime = 0; this.airTime = 0; this.timeSinceGround = 0;
    this.leftGroundByJump = false; this.jumpBuffer = 0; this.timeSinceRelease = 1000; this.zipCooldown = 0;
    this.noAnchorTimer = 0; this.skyUsed = 0; this.chain = 0;
    this.swingAnchor = freshAnchor(); this.preview = freshAnchor(); this.rope = 0; this.swingForward = FORWARD;
    this.autoCatchSwing = false;
    this.zipPhase = 'Travel'; this.zipStart = ZERO; this.zipTarget = ZERO; this.zipDir = FORWARD; this.zipWallNormal = ZERO;
    this.zipDuration = 0; this.zipElapsed = 0; this.zipToPerch = false; this.zipToWall = false; this.launchQueued = false;
    this.wallNormal = ZERO; this.vaultStart = ZERO; this.vaultTarget = ZERO; this.vaultNormal = ZERO; this.vaultElapsed = 0;
    this.lastFacing = FORWARD;
    this.streak = freshStreak(); this.lastStreak = freshStreak(); this.bestStreak = freshStreak(); this.streakMinSet = false;
    this.events = freshEvents();
  }

  // --- queries
  speedTier() { return clamp(Math.trunc(this.chain / Math.max(1, this.T.SwingsPerTier)), 0, 2); }
  speedCap() { const t = this.speedTier(); return t === 0 ? this.T.SpeedTier1 : t === 1 ? this.T.SpeedTier2 : this.T.SpeedTier3; }
  skyLeft() { return this.T.bSkyAnchorEnabled ? Math.max(0, this.T.SkyAnchorMaxInARow - this.skyUsed) : 0; }
  swingAngle(pos) {
    if (!this.swingAnchor.valid) return 0;
    const r = pos.sub(this.swingAnchor.pivot).norm();
    return Math.atan2(r.h().dot(this.swingForward), -r.z) / DEG;
  }
  facing(body) {
    switch (this.state) {
      case State.WallRun: case State.WallCling: return this.wallNormal.neg().norm(this.lastFacing);
      case State.Vault: return this.vaultNormal.neg().norm(this.lastFacing);
      case State.Zip: return this.zipDir.h().norm(this.lastFacing);
      default: { const h = body.velocity.h(); return h.len2() > 1 ? h.norm() : this.lastFacing; }
    }
  }
  consumeEvents() { const e = this.events; this.events = freshEvents(); return e; }

  static cameraTarget(T, velocity) {
    const range = Math.max(0.01, T.CameraFovSpeedFull - T.CameraFovSpeedStart);
    const x = clamp((velocity.len() - T.CameraFovSpeedStart) / range, 0, 1);
    const a = x * x * (3 - 2 * x);
    return { fov: lerp(T.CameraFovBase, T.CameraFovMax, a), arm: lerp(T.CameraArmBase, T.CameraArmMax, a),
      blur: T.CameraMotionBlurMax * a, lookAhead: velocity.norm().mul(T.CameraLookAhead * a), alpha: a };
  }

  // --- ground / air transitions
  notifyLanded() {
    if (this.state !== State.Ground) { this.endStreak(); this.events.landed = true; }
    this.setState(State.Ground);
    this.chain = 0; this.skyUsed = 0; this.noAnchorTimer = 0; this.airTime = 0; this.timeSinceGround = 0;
    this.leftGroundByJump = false; this.autoCatchSwing = false; this.preview = freshAnchor();
  }
  notifyLeftGround(jumped) {
    if (this.state === State.Ground) { this.leaveGround(jumped); this.setState(State.Air); }
  }
  leaveGround(jumped) {
    this.leftGroundByJump = jumped;
    if (jumped) this.jumpBuffer = 0;
    this.timeSinceGround = 0; this.airTime = 0; this.streak = freshStreak(); this.streakMinSet = false;
  }
  resetToAir() {
    this.endStreak(); this.leaveGround(true); this.setState(State.Air);
    this.chain = 0; this.skyUsed = 0; this.noAnchorTimer = 0; this.jumpBuffer = 0; this.autoCatchSwing = false;
    this.swingAnchor = freshAnchor(); this.preview = freshAnchor();
  }
  setState(s) {
    if (this.state === s) return;
    if (this.state === State.Swing) this.swingAnchor = freshAnchor();
    this.state = s; this.stateTime = 0;
  }
  updateTimers(dt, input) {
    this.stateTime += dt; this.timeSinceRelease += dt;
    this.zipCooldown = Math.max(0, this.zipCooldown - dt);
    this.jumpBuffer = input.jumpPressed ? this.T.AirJumpBufferTime : Math.max(0, this.jumpBuffer - dt);
    if (this.state !== State.Ground) { this.airTime += dt; this.timeSinceGround += dt; }
  }

  // --- entry points
  tick(dt, input, body, world) {
    body.move = ZERO;
    if (dt <= 0) return;
    if (this.state === State.Ground) this.notifyLeftGround(false);
    this.updateTimers(dt, input);
    const s = this.state;
    const couldSwing = s === State.Air || s === State.Launch || s === State.Glide || s === State.Swing;
    this.preview = couldSwing ? this.findAnchor(body, input, world) : freshAnchor();
    switch (s) {
      case State.Air: case State.Launch: this.tickAir(dt, input, body, world); break;
      case State.Swing: this.tickSwing(dt, input, body, world); break;
      case State.Zip: this.tickZip(dt, input, body); break;
      case State.WallRun: case State.WallCling: this.tickWall(dt, input, body, world); break;
      case State.Vault: this.tickVault(dt, body); break;
      case State.Glide: this.tickGlide(dt, input, body, world); break;
      default: this.integrateAir(dt, input, body);
    }
    this.updateStreak(body, dt);
    const h = body.velocity.h();
    if (h.len2() > 1) this.lastFacing = h.norm();
  }

  tickGround(dt, input, body, world) {
    if (dt <= 0) return;
    if (this.state !== State.Ground) this.notifyLanded();
    this.updateTimers(dt, input);
    const T = this.T;
    const move = input.move.h();
    if (!input.traversalHeld || move.len() < 0.3) return;
    const dir = move.norm();
    const hit = world.trace(body.position, body.position.add(dir.mul(body.radius + T.WallStickDistance)), 0);
    if (!hit) return;
    if (Math.abs(hit.normal.z) >= 0.3 || dir.dot(hit.normal.neg()) < 0.7) return;
    const normal = hit.normal.h().norm();
    if (this.probeWall(body.position.add(UP.mul(T.LedgeProbeHeight)), normal, body, world)) {
      this.leaveGround(true); this.wallNormal = normal; this.events.wallAttach = true; this.setState(State.WallRun);
      return;
    }
    const top = this.findLedge(body.position, normal, body, world);
    if (top && top.z - (body.position.z - body.halfHeight) <= T.GroundVaultMaxHeight) {
      this.leaveGround(true); this.startVault(body, top, normal);
    }
  }

  // Returns 'Slide' | 'Land' | 'Attach'.
  onBlocked(hit, walkable, body) {
    const T = this.T; const v = body.velocity;
    switch (this.state) {
      case State.Ground: case State.Vault: return 'Slide';
      case State.WallRun: case State.WallCling:
        if (walkable && v.z <= 0) return 'Land';
        if (v.dot(hit.normal) < 0) body.velocity = v.planeProject(hit.normal);
        return 'Slide';
      default: break;
    }
    if (walkable) return 'Land';
    if (Math.abs(hit.normal.z) < 0.3) {
      const speed = v.len();
      const headOn = speed > 0.1 ? v.div(speed).dot(hit.normal.neg()) : 1;
      if (this.state === State.Zip || headOn >= Math.sin(T.WallAttachMinAngle * DEG)) {
        this.wallNormal = hit.normal.h().norm(); body.velocity = ZERO; this.events.wallAttach = true;
        this.setState(State.WallCling);
        return 'Attach';
      }
    }
    if (v.dot(hit.normal) < 0) body.velocity = v.planeProject(hit.normal);
    return 'Slide';
  }

  // --- air
  tickAir(dt, input, body, world) {
    const T = this.T;
    if (this.state === State.Launch && this.stateTime >= T.LaunchStateTime) this.setState(State.Air);
    if (this.jumpBuffer > 0) {
      if (!this.leftGroundByJump && this.timeSinceGround <= T.AirCoyoteTime) {
        this.jumpBuffer = 0; this.leftGroundByJump = true;
        body.velocity = new V(body.velocity.x, body.velocity.y, Math.max(body.velocity.z, T.GroundJumpSpeed));
      } else if (this.zipCooldown <= 0 && this.tryStartZip(body, input, world)) {
        this.jumpBuffer = 0; this.tickZip(dt, input, body); return;
      }
    }
    if (input.glidePressed && T.bGlideUnlocked) { this.setState(State.Glide); this.tickGlide(dt, input, body, world); return; }
    if (this.canAttach(input, body)) {
      if (this.preview.valid) { this.startSwing(this.preview, body, input); this.tickSwing(dt, input, body, world); return; }
      this.noAnchorTimer += dt;
      if (this.noAnchorTimer >= T.SkyAnchorDelay && this.skyLeft() > 0) {
        this.skyUsed++; this.events.skyAnchor = true;
        this.startSwing(this.makeSkyAnchor(body, input), body, input); this.tickSwing(dt, input, body, world); return;
      }
    } else {
      this.noAnchorTimer = 0;
    }
    if (T.bAutoWebCatch && !input.traversalHeld && body.velocity.z < -T.AutoWebCatchMinFallSpeed) {
      const lookDown = -body.velocity.z * T.AutoWebCatchTime + body.halfHeight;
      if (world.trace(body.position, body.position.sub(UP.mul(lookDown)), body.radius)) {
        let c = this.preview;
        if (!c.valid && this.skyLeft() > 0) { this.skyUsed++; c = this.makeSkyAnchor(body, input); }
        if (c.valid) {
          this.startSwing(c, body, input); this.autoCatchSwing = true; this.events.autoCatch = true;
          this.tickSwing(dt, input, body, world); return;
        }
      }
    }
    this.integrateAir(dt, input, body);
  }
  integrateAir(dt, input, body) {
    const T = this.T;
    let v = body.velocity.clone();
    v.z -= T.Gravity * T.AirGravityScale * dt;
    v = this.airControl(v, input.move, T.AirControlAccel, dt);
    v.z = Math.max(v.z, -T.AirMaxFallSpeed);
    body.velocity = v; body.move = v.mul(dt);
  }
  airControl(v, move, accel, dt) {
    const hor = v.h();
    const limit = Math.max(hor.len(), this.T.GroundSprintSpeed);
    let nh = hor.add(move.h().mul(accel * dt));
    const n = nh.len();
    if (n > limit) nh = nh.mul(limit / n);
    return new V(nh.x, nh.y, v.z);
  }
  canAttach(input, body) {
    const T = this.T;
    return input.traversalHeld && this.timeSinceRelease >= T.SwingReattachDelay && this.airTime >= T.SwingMinAirTime
      && body.velocity.z <= T.SwingAttachMaxRiseSpeed;
  }

  // --- anchors
  findAnchor(body, input, world) {
    const T = this.T;
    let best = freshAnchor();
    const origin = body.position.add(UP.mul(body.halfHeight * 0.6));
    const cam = input.cameraForward.norm(this.lastFacing);
    let fwd = cam.h().norm();
    if (fwd.nearlyZero()) fwd = body.velocity.h().norm(this.lastFacing);
    const side = new V(-fwd.y, fwd.x, 0);
    const camPitch = Math.asin(clamp(cam.z, -1, 1)) / DEG;
    const axis = clamp(T.AnchorConePitch + camPitch * T.AnchorCameraPitchInfluence, 10, 80);
    const half = T.AnchorConeAngle * 0.5;
    const ny = Math.max(1, T.AnchorSamplesYaw), np = Math.max(1, T.AnchorSamplesPitch);
    const pLo = Math.max(T.AnchorMinElevation, axis - half);
    const pHi = Math.max(pLo, Math.min(85, axis + half));
    for (let iy = 0; iy < ny; iy++) {
      const yaw = (ny > 1 ? lerp(-half, half, iy / (ny - 1)) : 0) * DEG;
      const dirH = fwd.mul(Math.cos(yaw)).add(side.mul(Math.sin(yaw)));
      for (let ip = 0; ip < np; ip++) {
        const pitch = (np > 1 ? lerp(pLo, pHi, ip / (np - 1)) : axis) * DEG;
        const dir = dirH.mul(Math.cos(pitch)).add(UP.mul(Math.sin(pitch)));
        const hit = world.trace(origin, origin.add(dir.mul(T.AnchorReach)), T.AnchorTraceRadius);
        if (!hit) continue;
        const to = hit.location.sub(origin);
        const dist = to.len();
        if (dist < T.SwingRopeMinLength) continue;
        const elev = Math.asin(clamp(to.z / dist, -1, 1)) / DEG;
        if (elev < T.AnchorMinElevation) continue;
        const pivot = hit.location.add(hit.normal.mul(T.AnchorSurfaceOffset));
        if (V.dist(pivot, body.position) > T.SwingRopeMaxLength) continue;
        let es = 1;
        if (elev < T.AnchorIdealElevationMin) es = (elev - T.AnchorMinElevation) / Math.max(1, T.AnchorIdealElevationMin - T.AnchorMinElevation);
        else if (elev > T.AnchorIdealElevationMax) es = (90 - elev) / Math.max(1, 90 - T.AnchorIdealElevationMax);
        const ds = Math.max(0, 1 - Math.abs(dist - T.AnchorIdealDistance) / Math.max(1, T.AnchorReach));
        const as = Math.max(0, to.h().norm().dot(fwd));
        const facing = Math.max(0, -hit.normal.h().norm().dot(fwd));
        let score = T.AnchorWeightElevation * es + T.AnchorWeightDistance * ds + T.AnchorWeightAlignment * as - T.AnchorFacingPenalty * facing;
        if (hit.anchorProp) score += T.AnchorPropBonus;
        if (this.preview.valid && V.dist(pivot, this.preview.pivot) < T.AnchorHysteresisRadius) score += T.AnchorHysteresis;
        if (!best.valid || score > best.score) best = { valid: true, sky: false, pivot, surface: hit.location, normal: hit.normal, score };
      }
    }
    return best;
  }
  makeSkyAnchor(body, input) {
    const T = this.T;
    let fwd = input.cameraForward.h().norm();
    if (fwd.nearlyZero()) fwd = body.velocity.h().norm(this.lastFacing);
    const pivot = body.position.add(fwd.mul(T.SkyAnchorForward)).add(UP.mul(T.SkyAnchorHeight));
    return { valid: true, sky: true, pivot, surface: pivot, normal: UP.neg(), score: 0 };
  }

  // --- swing
  startSwing(anchor, body, input) {
    const T = this.T;
    const oldTier = this.speedTier();
    this.chain = this.timeSinceRelease <= T.SwingChainWindow ? this.chain + 1 : 1;
    let travel = body.velocity.h();
    if (travel.len() < T.SwingMinSpeed * 0.5) travel = input.cameraForward.h();
    if (travel.len2() < 1e-4) travel = anchor.pivot.sub(body.position).h();
    this.swingForward = travel.norm(this.lastFacing);
    if (!anchor.sky) this.skyUsed = 0;
    const toPivotH = anchor.pivot.sub(body.position).h();
    const lateral = toPivotH.sub(this.swingForward.mul(toPivotH.dot(this.swingForward)));
    this.swingAnchor = { ...anchor, pivot: anchor.pivot.sub(lateral.mul(T.SwingPlaneAlign)) };
    this.rope = clamp(V.dist(body.position, this.swingAnchor.pivot), T.SwingRopeMinLength, T.SwingRopeMaxLength);
    this.autoCatchSwing = false; this.noAnchorTimer = 0;
    this.setState(State.Swing);
    this.events.swingStarted = true; this.streak.swings++;
    if (this.speedTier() > oldTier) this.events.tierUp = true;
  }
  release(body, allowBoost, jumpOff) {
    const T = this.T;
    const angle = this.swingAngle(body.position);
    if (allowBoost && angle >= T.ReleaseWindowMinAngle && angle <= T.ReleaseWindowMaxAngle) {
      const oldTier = this.speedTier();
      body.velocity = body.velocity.add(body.velocity.norm().mul(T.ReleaseBoostSpeed)).add(UP.mul(T.ReleaseBoostUp));
      this.chain += T.PerfectReleaseChainBonus;
      this.events.perfectRelease = true; this.streak.perfectReleases++;
      if (this.speedTier() > oldTier) this.events.tierUp = true;
    }
    if (jumpOff) body.velocity = new V(body.velocity.x, body.velocity.y, Math.max(body.velocity.z, 0) + T.ReleaseJumpUp);
    this.timeSinceRelease = 0; this.autoCatchSwing = false; this.events.released = true;
    this.setState(State.Air);
  }
  tickSwing(dt, input, body, world) {
    const T = this.T;
    if (this.jumpBuffer > 0) { this.jumpBuffer = 0; this.release(body, true, true); this.integrateAir(dt, input, body); return; }
    if (input.glidePressed && T.bGlideUnlocked) {
      this.release(body, true, false); this.setState(State.Glide); this.integrateAir(dt, input, body); return;
    }
    if (!input.traversalHeld && !this.autoCatchSwing) { this.release(body, true, false); this.integrateAir(dt, input, body); return; }

    const pivot = this.swingAnchor.pivot;
    const cap = this.speedCap();
    let v = body.velocity.clone();
    v.z -= T.Gravity * T.SwingGravityScale * dt;
    v = v.add(input.move.h().mul(T.SwingAirControl * dt));
    const r = body.position.sub(pivot).norm(UP.neg());
    let tangent = v.sub(r.mul(v.dot(r)));
    if (Math.abs(this.swingAngle(body.position)) <= T.SwingLowPointAngle && tangent.len() < cap) {
      v = v.add(tangent.norm().mul(T.SwingLowPointBoost * dt));
    }
    v = v.mul(Math.max(0, 1 - T.SwingDamping * dt));
    tangent = v.sub(r.mul(v.dot(r)));
    const ts = tangent.len();
    if (ts < T.SwingMinSpeed) {
      const dir = ts > 0.1 ? tangent.div(ts) : this.swingForward.planeProject(r).norm();
      v = r.mul(v.dot(r)).add(dir.mul(T.SwingMinSpeed));
    }
    const speed = v.len();
    if (speed > cap) v = v.mul(Math.max(cap / speed, 1 - T.SpeedOverCapDrag * dt));

    const down = this.rope + body.halfHeight + T.SwingGroundClearance;
    const ground = world.trace(pivot, pivot.sub(UP.mul(down)), 0);
    if (ground) {
      const maxLen = pivot.z - (ground.location.z + T.SwingGroundClearance + body.halfHeight);
      if (this.rope > maxLen) this.rope = Math.max(maxLen, this.rope - T.SwingRopeReelSpeed * dt);
    }
    this.rope = Math.max(this.rope, T.SwingRopeMinLength);

    let np = body.position.add(v.mul(dt));
    const d = np.sub(pivot);
    const dl = d.len();
    if (dl > this.rope) {
      const n = d.div(dl);
      np = pivot.add(n.mul(this.rope));
      const radial = v.dot(n);
      if (radial > 0) v = v.sub(n.mul(radial));
    }
    body.velocity = v; body.move = np.sub(body.position);

    const na = this.swingAngle(np);
    const pastApex = na > T.SwingApexMinAngle && v.z <= 0;
    const back = na > 0 && v.h().dot(this.swingForward) < 0;
    const catchDone = this.autoCatchSwing && na > T.SwingApexMinAngle;
    if (na >= T.SwingAutoReleaseAngle || pastApex || back || catchDone) this.release(body, false, false);
  }

  // --- zip and point-launch
  tryStartZip(body, input, world) {
    const T = this.T;
    const eye = body.position.add(UP.mul(body.halfHeight * 0.6));
    const dir = input.cameraForward.norm(this.lastFacing);
    this.zipToPerch = false; this.zipToWall = false; this.launchQueued = false;
    let target = eye.add(dir.mul(T.ZipNoTargetDistance));
    const hit = world.trace(eye, eye.add(dir.mul(T.ZipRange)), 0);
    if (hit) {
      if (hit.normal.z > 0.7) {
        target = hit.location.add(UP.mul(body.halfHeight + T.ZipPerchClearance)); this.zipToPerch = true;
      } else if (Math.abs(hit.normal.z) < 0.3) {
        const wn = hit.normal.h().norm();
        const top = this.findLedge(hit.location.add(wn.mul(body.radius)), wn, body, world);
        if (top) { target = top.add(UP.mul(body.halfHeight + T.ZipPerchClearance)); this.zipToPerch = true; }
        else { target = hit.location.add(wn.mul(body.radius + 0.05)); this.zipWallNormal = wn; this.zipToWall = true; }
      } else {
        target = hit.location.add(hit.normal.mul(body.radius + body.halfHeight));
      }
    }
    const delta = target.sub(body.position);
    const dist = delta.len();
    if (dist < 1) return false;
    this.zipStart = body.position; this.zipTarget = target; this.zipDir = delta.div(dist);
    this.zipDuration = clamp(dist / Math.max(1, T.ZipSpeed), T.ZipMinDuration, T.ZipMaxDuration);
    this.zipElapsed = 0; this.zipPhase = 'Travel'; this.zipCooldown = T.ZipCooldown;
    this.events.zip = true; this.setState(State.Zip);
    return true;
  }
  tickZip(dt, input, body) {
    const T = this.T;
    if (this.zipPhase === 'Travel') {
      this.zipElapsed += dt;
      if (this.jumpBuffer > 0 && this.zipToPerch && this.zipDuration - this.zipElapsed <= T.LaunchWindow) {
        this.launchQueued = true; this.jumpBuffer = 0;
      }
      const a = clamp(this.zipElapsed / this.zipDuration, 0, 1);
      const rem = 1 - a;
      const lead = 1 - rem * rem * rem, lag = 1 - rem * rem;
      const rising = this.zipTarget.z > this.zipStart.z;
      const ha = rising ? lag : lead, va = rising ? lead : lag;
      const np = new V(lerp(this.zipStart.x, this.zipTarget.x, ha), lerp(this.zipStart.y, this.zipTarget.y, ha), lerp(this.zipStart.z, this.zipTarget.z, va));
      body.move = np.sub(body.position); body.velocity = body.move.div(dt);
      if (a >= 1) {
        if (this.zipToWall) {
          this.wallNormal = this.zipWallNormal; body.velocity = ZERO; this.events.wallAttach = true; this.setState(State.WallCling);
        } else if (this.zipToPerch) {
          this.zipPhase = 'Perch'; this.zipElapsed = 0; body.velocity = ZERO;
        } else {
          body.velocity = this.zipDir.mul(T.ZipExitSpeed); this.setState(State.Air);
        }
      }
      return;
    }
    this.zipElapsed += dt;
    body.velocity = ZERO; body.move = ZERO;
    const fwd = this.zipDir.h().norm(this.lastFacing);
    if (this.launchQueued || this.jumpBuffer > 0) {
      this.launchQueued = false; this.jumpBuffer = 0;
      body.velocity = fwd.mul(T.LaunchForwardSpeed).add(UP.mul(T.LaunchUpSpeed));
      body.move = body.velocity.mul(dt); this.events.launch = true; this.setState(State.Launch);
      return;
    }
    if (this.zipElapsed >= T.LaunchWindow || input.traversalHeld) {
      body.velocity = fwd.sub(UP); body.move = body.velocity.mul(dt); this.setState(State.Air);
    }
  }

  // --- walls and ledges
  probeWall(from, normal, body, world) {
    const hit = world.trace(from, from.sub(normal.mul(body.radius + this.T.WallStickDistance)), 0);
    return hit && Math.abs(hit.normal.z) < 0.3 ? hit : null;
  }
  findLedge(from, normal, body, world) {
    const T = this.T;
    const above = from.add(UP.mul(T.LedgeSearchHeight));
    const inward = normal.neg().mul(body.radius + 0.6);
    if (world.trace(above, above.add(inward), 0)) return null;
    const top = world.trace(above.add(inward), from.add(inward).sub(UP.mul(body.halfHeight)), 0);
    if (!top || top.normal.z < 0.7) return null;
    const feet = top.location.add(UP.mul(0.1));
    if (world.trace(feet, feet.add(UP.mul(body.halfHeight * 2)), 0)) return null;
    return top.location;
  }
  tickWall(dt, input, body, world) {
    const T = this.T;
    const wall = this.probeWall(body.position, this.wallNormal, body, world);
    if (!wall) {
      const top = body.velocity.z >= 0 ? this.findLedge(body.position, this.wallNormal, body, world) : null;
      if (top) { this.startVault(body, top, this.wallNormal); this.tickVault(dt, body); }
      else { this.setState(State.Air); this.integrateAir(dt, input, body); }
      return;
    }
    this.wallNormal = wall.normal.h().norm(this.wallNormal);
    if (this.jumpBuffer > 0) {
      this.jumpBuffer = 0;
      body.velocity = this.wallNormal.mul(T.WallJumpOutSpeed).add(UP.mul(T.WallJumpUpSpeed));
      body.move = body.velocity.mul(dt); this.setState(State.Air);
      return;
    }
    this.setState(input.traversalHeld ? State.WallRun : State.WallCling);
    const right = this.wallNormal.cross(UP).norm();
    const move = input.move.h();
    const inUp = move.dot(this.wallNormal.neg()), inRight = move.dot(right);
    let desired;
    if (this.state === State.WallRun) {
      const has = Math.abs(inUp) > 0.1 || Math.abs(inRight) > 0.1;
      desired = UP.mul(has ? inUp : 1).add(right.mul(inRight)).norm().mul(T.WallRunSpeed);
    } else {
      desired = UP.mul(inUp).add(right.mul(inRight)).mul(T.WallCrawlSpeed);
      if (desired.len() > T.WallCrawlSpeed) desired = desired.norm().mul(T.WallCrawlSpeed);
    }
    if (desired.z > 0 && !this.probeWall(body.position.add(UP.mul(T.LedgeProbeHeight)), this.wallNormal, body, world)) {
      const top = this.findLedge(body.position, this.wallNormal, body, world);
      if (top) { this.startVault(body, top, this.wallNormal); this.tickVault(dt, body); return; }
    }
    body.velocity = desired;
    const gap = Math.max(0, wall.distance - body.radius - 0.02);
    body.move = desired.mul(dt).sub(this.wallNormal.mul(gap));
  }
  startVault(body, top, normal) {
    this.vaultStart = body.position; this.vaultTarget = top.add(UP.mul(body.halfHeight + 0.05));
    this.vaultNormal = normal; this.vaultElapsed = 0; this.events.vault = true; this.setState(State.Vault);
  }
  tickVault(dt, body) {
    const T = this.T;
    this.vaultElapsed += dt;
    const a = clamp(this.vaultElapsed / T.VaultDuration, 0, 1);
    const rise = clamp(T.VaultRiseFraction, 0.05, 0.95);
    const ux = clamp(a / rise, 0, 1), fx = clamp((a - rise) / (1 - rise), 0, 1);
    const ua = ux * ux * (3 - 2 * ux), fa = fx * fx * (3 - 2 * fx);
    const np = new V(lerp(this.vaultStart.x, this.vaultTarget.x, fa), lerp(this.vaultStart.y, this.vaultTarget.y, fa), lerp(this.vaultStart.z, this.vaultTarget.z, ua));
    body.move = np.sub(body.position); body.velocity = body.move.div(dt);
    if (a >= 1) { body.velocity = this.vaultNormal.neg().mul(T.VaultExitSpeed); this.setState(State.Air); }
  }

  // --- glide (stub)
  tickGlide(dt, input, body, world) {
    const T = this.T;
    if ((input.glidePressed && this.stateTime > 0) || !T.bGlideUnlocked) { this.setState(State.Air); this.integrateAir(dt, input, body); return; }
    if (this.canAttach(input, body) && this.preview.valid) { this.startSwing(this.preview, body, input); this.tickSwing(dt, input, body, world); return; }
    if (this.jumpBuffer > 0 && this.zipCooldown <= 0 && this.tryStartZip(body, input, world)) { this.jumpBuffer = 0; this.tickZip(dt, input, body); return; }
    const v = body.velocity.clone();
    const hor = v.h(); const hs = hor.len();
    let heading = hs > 0.5 ? hor.div(hs) : input.cameraForward.h().norm(this.lastFacing);
    const mh = input.move.h();
    const wanted = mh.len2() > 0.01 ? mh.norm() : heading;
    const cross = heading.x * wanted.y - heading.y * wanted.x;
    const maxTurn = T.GlideTurnRate * DEG * dt;
    heading = rotateZ(heading, clamp(Math.atan2(cross, heading.dot(wanted)), -maxTurn, maxTurn));
    const ns = hs + clamp(T.GlideSpeed - hs, -T.GlideAccel * dt, T.GlideAccel * dt);
    let vz = v.z;
    if (vz > 0) vz -= T.Gravity * T.AirGravityScale * dt;
    else if (vz < -T.GlideSinkRate) vz = Math.min(-T.GlideSinkRate, vz + T.GlideAccel * dt);
    else vz = Math.max(-T.GlideSinkRate, vz - T.Gravity * T.GlideGravityScale * dt);
    body.velocity = new V(heading.x * ns, heading.y * ns, vz); body.move = body.velocity.mul(dt);
  }

  // --- streaks
  updateStreak(body, dt) {
    if (this.state === State.Ground) return;
    const s = this.streak; const speed = body.velocity.len();
    s.distance += body.move.len2D(); s.time += dt;
    s.maxSpeed = Math.max(s.maxSpeed, speed); s.maxTier = Math.max(s.maxTier, this.speedTier());
    if (s.time > 1) { s.minSpeed = this.streakMinSet ? Math.min(s.minSpeed, speed) : speed; this.streakMinSet = true; }
  }
  endStreak() {
    if (this.streak.time <= 0) return;
    this.lastStreak = this.streak;
    if (this.streak.distance > this.bestStreak.distance) this.bestStreak = this.streak;
    this.events.streakEnded = true; this.streak = freshStreak(); this.streakMinSet = false;
  }
}

// ------------------------------------------------------------------ box world (Tools/TraversalSim FBoxWorld)

const CELL = 50;
const axis = (v, a) => (a === 0 ? v.x : a === 1 ? v.y : v.z);
const axisVec = (a, s) => new V(a === 0 ? s : 0, a === 1 ? s : 0, a === 2 ? s : 0);

export class BoxWorld {
  constructor() { this.boxes = []; this.big = []; this.cells = new Map(); this.stamps = []; this.stamp = 0; this.traceCount = 0; }
  static cell(v) { return Math.floor(v / CELL); }
  static key(x, y) { return x * 100003 + y; }
  addBox(centre, size, anchor = false, kind = 'box') {
    const box = { min: centre.sub(size.mul(0.5)), max: centre.add(size.mul(0.5)), anchor, kind };
    const i = this.boxes.length;
    this.boxes.push(box); this.stamps.push(0);
    if (size.x > 500 || size.y > 500) { this.big.push(i); return; }
    for (let cx = BoxWorld.cell(box.min.x); cx <= BoxWorld.cell(box.max.x); cx++) {
      for (let cy = BoxWorld.cell(box.min.y); cy <= BoxWorld.cell(box.max.y); cy++) {
        const k = BoxWorld.key(cx, cy);
        if (!this.cells.has(k)) this.cells.set(k, []);
        this.cells.get(k).push(i);
      }
    }
  }
  static fromLayout(layout) {
    const w = new BoxWorld();
    for (const [kind, cx, cy, cz, sx, sy, sz, anchor] of layout.boxes) w.addBox(new V(cx, cy, cz), new V(sx, sy, sz), !!anchor, kind);
    return w;
  }
  trace(start, end, radius) { this.traceCount++; return this.sweep(start, end, radius, radius, radius); }
  sweepBody(start, end, body) { return this.sweep(start, end, body.radius, body.radius, body.halfHeight); }
  sweep(start, end, ix, iy, iz) {
    const d = end.sub(start);
    const len = d.len();
    if (len < 1e-9) return null;
    const inflate = [ix, iy, iz];
    let bestT = 2, bestN = UP, bestAnchor = false;
    this.stamp++;
    const test = (i) => {
      if (this.stamps[i] === this.stamp) return;
      this.stamps[i] = this.stamp;
      const b = this.boxes[i];
      let tEnter = -Infinity, tExit = Infinity, n = UP;
      for (let a = 0; a < 3; a++) {
        const s = axis(start, a), dir = axis(d, a);
        const lo = axis(b.min, a) - inflate[a], hi = axis(b.max, a) + inflate[a];
        if (Math.abs(dir) < 1e-12) { if (s <= lo || s >= hi) return; continue; }
        let t1 = (lo - s) / dir, t2 = (hi - s) / dir, n1 = -1;
        if (t1 > t2) { const t = t1; t1 = t2; t2 = t; n1 = 1; }
        if (t1 > tEnter) { tEnter = t1; n = axisVec(a, n1); }
        tExit = Math.min(tExit, t2);
        if (tEnter > tExit) return;
      }
      if (tExit <= 1e-9 || tEnter > 1) return;
      if (tEnter < 0) {
        let best = Infinity;
        for (let a = 0; a < 3; a++) {
          const s = axis(start, a), lo = axis(b.min, a) - inflate[a], hi = axis(b.max, a) + inflate[a];
          if (s - lo < best) { best = s - lo; n = axisVec(a, -1); }
          if (hi - s < best) { best = hi - s; n = axisVec(a, 1); }
        }
        if (d.dot(n) >= 0) return;
        tEnter = 0;
      }
      if (tEnter < bestT) { bestT = tEnter; bestN = n; bestAnchor = b.anchor; }
    };
    for (const i of this.big) test(i);
    const minX = Math.min(start.x, end.x) - ix, maxX = Math.max(start.x, end.x) + ix;
    const minY = Math.min(start.y, end.y) - iy, maxY = Math.max(start.y, end.y) + iy;
    for (let cx = BoxWorld.cell(minX); cx <= BoxWorld.cell(maxX); cx++) {
      for (let cy = BoxWorld.cell(minY); cy <= BoxWorld.cell(maxY); cy++) {
        const list = this.cells.get(BoxWorld.key(cx, cy));
        if (list) for (const i of list) test(i);
      }
    }
    if (bestT > 1) return null;
    const c = start.add(d.mul(bestT));
    return { distance: bestT * len, normal: bestN, anchorProp: bestAnchor,
      location: new V(c.x - bestN.x * ix, c.y - bestN.y * iy, c.z - bestN.z * iz) };
  }
}

// ------------------------------------------------------------------ host (what the Unreal movement component does)

export function makeInput() {
  return { move: ZERO, cameraForward: FORWARD, traversalHeld: false, jumpPressed: false, jumpHeld: false, glidePressed: false };
}

export class Hero {
  constructor(world, tuning) {
    this.world = world; this.T = tuning; this.sim = new TraversalSim(tuning);
    this.body = { position: ZERO, velocity: ZERO, radius: 0.42, halfHeight: 0.96, move: ZERO };
    this.landings = 0;
  }
  placeOnGround(feet) {
    this.body.position = feet.add(UP.mul(this.body.halfHeight)); this.body.velocity = ZERO; this.sim.notifyLanded();
  }
  teleport(feet) {
    this.body.position = feet.add(UP.mul(this.body.halfHeight + 1)); this.body.velocity = ZERO; this.sim.resetToAir();
  }
  step(dt, input) {
    const sim = this.sim, T = this.T, body = this.body;
    if (sim.state === State.Ground) {
      sim.tickGround(dt, input, body, this.world);
      if (sim.state === State.Ground) {
        const speed = input.traversalHeld ? T.GroundSprintSpeed : T.GroundRunSpeed;
        const m = input.move.h();
        body.velocity = m.mul(speed);
        if (input.jumpPressed) {
          body.velocity = new V(body.velocity.x, body.velocity.y, T.GroundJumpSpeed);
          sim.notifyLeftGround(true);
          this.moveWithCollision(body.velocity.mul(dt), false);
          return;
        }
        this.moveWithCollision(body.velocity.mul(dt), true);
        // Walked off an edge?
        if (!this.world.sweepBody(body.position, body.position.sub(UP.mul(0.25)), body)) sim.notifyLeftGround(false);
        return;
      }
    }
    sim.tick(dt, input, body, this.world);
    this.moveWithCollision(body.move, false);
  }
  moveWithCollision(delta, walking) {
    const body = this.body;
    for (let it = 0; it < 4 && delta.len2() > 1e-12; it++) {
      const hit = this.world.sweepBody(body.position, body.position.add(delta), body);
      if (!hit) { body.position = body.position.add(delta); return; }
      const len = delta.len();
      const travel = delta.mul(Math.max(0, hit.distance - 0.01) / len);
      body.position = body.position.add(travel);
      const walkable = hit.normal.z > 0.7;
      if (walking) { delta = delta.sub(travel).planeProject(hit.normal); continue; }
      const r = this.sim.onBlocked(hit, walkable, body);
      if (r === 'Land') { body.velocity = ZERO; this.sim.notifyLanded(); this.landings++; return; }
      if (r === 'Attach') return;
      delta = delta.sub(travel).planeProject(hit.normal);
    }
  }
}

// ------------------------------------------------------------------ route helper (bots and the HUD's route guide)

export class Route {
  constructor(points) {
    this.p = points.map(([x, y]) => new V(x, y, 0));
    this.c = [0];
    for (let i = 1; i < this.p.length; i++) this.c.push(this.c[i - 1] + V.dist(this.p[i - 1], this.p[i]));
  }
  get length() { return this.c[this.c.length - 1]; }
  project(pos, hint) {
    const q = pos.h(); let best = hint, bd = Infinity;
    for (let i = 1; i < this.p.length; i++) {
      if (this.c[i] < hint - 150 || this.c[i - 1] > hint + 150) continue;
      const a = this.p[i - 1], ab = this.p[i].sub(a);
      const t = clamp(q.sub(a).dot(ab) / Math.max(1e-9, ab.len2()), 0, 1);
      const dd = V.dist(q, a.add(ab.mul(t)));
      if (dd < bd) { bd = dd; best = this.c[i - 1] + t * (this.c[i] - this.c[i - 1]); }
    }
    return best;
  }
  at(s) {
    s = clamp(s, 0, this.length);
    for (let i = 1; i < this.p.length; i++) {
      if (this.c[i] >= s) { const t = (s - this.c[i - 1]) / Math.max(1e-9, this.c[i] - this.c[i - 1]); return V.lerp(this.p[i - 1], this.p[i], t); }
    }
    return this.p[this.p.length - 1];
  }
}
