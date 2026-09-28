# Traversal tuning reference

Generated from `FTraversalTuning` in `Source/WebOfTheCity/Public/Traversal/TraversalTypes.h` by
`python Tools/TraversalSim/tuning_table.py`. Do not edit by hand: change the header (or `DA_TraversalTuning`) and regenerate.

Units: metres, seconds, degrees. Every value is live-editable in game (F2) and in `DA_TraversalTuning`.

| Group | Setting | Default | What it does |
|---|---|---|---|
| Ground | `GroundRunSpeed` | 7.0 | Jog speed on the ground. |
| Ground | `GroundSprintSpeed` | 14.0 | Sprint/parkour speed while holding the traversal button on the ground. |
| Ground | `GroundJumpSpeed` | 9.0 | Upward speed of a jump (also used for coyote jumps). |
| Ground | `GroundVaultMaxHeight` | 2.2 | Sprinting into an obstacle up to this tall auto-vaults over it; taller walls start a wall-run. |
| Air | `Gravity` | 9.81 | Real-world gravity; the per-state scales below multiply it. |
| Air | `AirGravityScale` | 1.6 | Gravity multiplier while airborne (not swinging). Above 1 feels snappier than real life. |
| Air | `AirControlAccel` | 10.0 | Sideways steering acceleration in the air. Can turn you, never speeds you past sprint speed. |
| Air | `AirMaxFallSpeed` | 60.0 | Terminal fall speed. |
| Air | `AirCoyoteTime` | 0.15 | After walking off a ledge, jump still works for this long. |
| Air | `AirJumpBufferTime` | 0.15 | A jump press is remembered this long, so slightly early presses still count. |
| Anchor | `AnchorReach` | 80.0 | How far the web can reach. |
| Anchor | `AnchorConeAngle` | 90.0 | Full width of the search cone. |
| Anchor | `AnchorConePitch` | 45.0 | Cone centre, degrees above the horizon, when the camera is level. |
| Anchor | `AnchorCameraPitchInfluence` | 0.5 | How much camera pitch tilts the cone (0 = ignore camera pitch, 1 = follow it fully). |
| Anchor | `AnchorIdealElevationMin` | 35.0 | Preferred anchor elevation band (Section 6.1: 35 to 55 degrees above the horizon). |
| Anchor | `AnchorIdealElevationMax` | 55.0 | Upper end of the preferred elevation band. |
| Anchor | `AnchorMinElevation` | 15.0 | Anchors lower than this are never used. |
| Anchor | `AnchorIdealDistance` | 40.0 | Preferred distance to the anchor. |
| Anchor | `AnchorSurfaceOffset` | 2.0 | The swing pivots this far out from the wall, so arcs don't scrape the building. |
| Anchor | `AnchorTraceRadius` | 0.3 | Radius of the anchor search traces (thin geometry is easier to hit with a bigger value). |
| Anchor | `AnchorSamplesYaw` | 7 | Search rays across the cone. Cost is yaw x pitch traces per airborne frame. |
| Anchor | `AnchorSamplesPitch` | 5 | Search rays up the cone (elevation steps between the minimum elevation and the cone top). |
| Anchor | `AnchorWeightElevation` | 1.0 | Score weight for being in the ideal elevation band. |
| Anchor | `AnchorWeightDistance` | 0.5 | Score weight for being near the ideal distance. |
| Anchor | `AnchorWeightAlignment` | 0.8 | Score weight for being where the camera looks. |
| Anchor | `AnchorPropBonus` | 0.3 | Extra score for props tagged "WebAnchor" (water tanks, cranes, lampposts). |
| Anchor | `AnchorHysteresis` | 0.15 | Soft magnetism: extra score for staying on the previously previewed anchor, so the preview doesn't flicker. |
| Anchor | `AnchorFacingPenalty` | 0.6 | Score penalty for walls facing us head-on: swinging at them slams us into them. |
| Anchor | `AnchorHysteresisRadius` | 3.0 | A candidate this close to the previous preview counts as "the same anchor" for the hysteresis bonus. |
| Swing | `SwingGravityScale` | 2.0 | Gravity multiplier on the rope (Section 6.1: 1.5 to 2.5x). |
| Swing | `SwingRopeMinLength` | 8.0 | Shortest rope; closer anchors are ignored. |
| Swing | `SwingRopeMaxLength` | 80.0 | Longest rope; farther anchors are ignored. |
| Swing | `SwingRopeReelSpeed` | 25.0 | How fast the rope shortens to keep the bottom of the arc off the street. |
| Swing | `SwingPlaneAlign` | 0.85 | How far the swing pivot moves from the wall toward our line of travel (0 = pure physics, 1 = swing straight along our heading). |
| Swing | `SwingGroundClearance` | 6.0 | The lowest point of a swing stays at least this far above the ground below the anchor. |
| Swing | `SwingLowPointAngle` | 35.0 | The low-point boost applies within this many degrees either side of the bottom of the arc. |
| Swing | `SwingLowPointBoost` | 18.0 | Tangential acceleration through the low point, up to the current speed-tier cap. |
| Swing | `SwingMinSpeed` | 12.0 | Speed floor on the rope: a swing never stalls. |
| Swing | `SwingAirControl` | 8.0 | Steering acceleration while swinging. |
| Swing | `SwingDamping` | 0.02 | Fraction of speed lost per second on the rope. |
| Swing | `SwingAutoReleaseAngle` | 65.0 | Past the low point, the web lets go by itself at this angle (or when you stop rising). |
| Swing | `SwingApexMinAngle` | 10.0 | Past this angle, the web also lets go as soon as you stop rising (top of the arc). |
| Swing | `SwingReattachDelay` | 0.12 | While the button stays held, the next web fires this long after a release. |
| Swing | `SwingAttachMaxRiseSpeed` | 3.0 | A new web only fires once you are rising slower than this, i.e. near the top of the arc. Webs fired while rising fast swing you backwards. |
| Swing | `SwingMinAirTime` | 0.1 | After leaving the ground, the first web can fire after this long. |
| Swing | `SwingChainWindow` | 1.5 | A swing that starts within this long of the last release counts as a chain. |
| Swing | `SwingsPerTier` | 3 | Chained swings needed per speed tier. |
| Swing | `PerfectReleaseChainBonus` | 1 | A well-timed release counts as this many extra chained swings. |
| Swing | `SpeedTier1` | 25.0 | Speed cap at tier 1 (Section 6.1 starting values: about 25, 45, 70 m/s). |
| Swing | `SpeedTier2` | 45.0 | Speed cap at tier 2. |
| Swing | `SpeedTier3` | 70.0 | Speed cap at tier 3. |
| Swing | `SpeedOverCapDrag` | 0.8 | Above the tier cap, this fraction of speed bleeds off per second (soft cap, not a wall). |
| Swing | `ReleaseWindowMinAngle` | -5.0 | Perfect-release window start, degrees past the low point (negative = just before it). |
| Swing | `ReleaseWindowMaxAngle` | 35.0 | Perfect-release window end, degrees past the low point. |
| Swing | `ReleaseBoostSpeed` | 6.0 | Perfect-release boost along the direction of travel. |
| Swing | `ReleaseBoostUp` | 2.0 | Perfect-release boost upwards (kept small: height comes from anchors, not boosts). |
| Swing | `ReleaseJumpUp` | 8.0 | Pressing jump on the rope lets go with this much extra upward speed. |
| Fallbacks | `bSkyAnchorEnabled` | true | In wide-open spaces with nothing to hit, fire a limited "sky anchor". |
| Fallbacks | `SkyAnchorDelay` | 0.25 | Seconds of holding swing with no real anchor before a sky anchor fires. |
| Fallbacks | `SkyAnchorMaxInARow` | 2 | Sky anchors allowed in a row. Swinging from a real surface, or touching down, refills them. |
| Fallbacks | `SkyAnchorHeight` | 30.0 | Sky anchor height above you. |
| Fallbacks | `SkyAnchorForward` | 25.0 | Sky anchor distance ahead of you. |
| Fallbacks | `bAutoWebCatch` | true | Accessibility: fire a web automatically before hitting the street at speed. |
| Fallbacks | `AutoWebCatchTime` | 0.7 | Auto web-catch triggers when impact is this close (seconds)... |
| Fallbacks | `AutoWebCatchMinFallSpeed` | 15.0 | ...and you are falling at least this fast. |
| Zip | `ZipRange` | 45.0 | Web-zip reach (jump button in the air). |
| Zip | `ZipSpeed` | 60.0 | Zip travel speed; the duration is clamped to the min/max below (Section 6.1: 0.3 to 0.5 s). |
| Zip | `ZipMinDuration` | 0.3 | Shortest zip. |
| Zip | `ZipMaxDuration` | 0.5 | Longest zip. |
| Zip | `ZipNoTargetDistance` | 20.0 | With nothing to zip to, you still dart this far in the camera direction. |
| Zip | `ZipExitSpeed` | 14.0 | Speed carried out of a zip that didn't end on a surface. |
| Zip | `ZipCooldown` | 0.2 | Minimum time between zips. |
| Zip | `ZipPerchClearance` | 0.15 | Zips to a perch arrive this far above it, so the approach clears the ledge lip. |
| Zip | `LaunchWindow` | 0.25 | Point-launch: press jump within this long of arriving on a perch (before or after). |
| Zip | `LaunchForwardSpeed` | 22.0 | Point-launch forward speed. |
| Zip | `LaunchUpSpeed` | 16.0 | Point-launch upward speed. |
| Zip | `LaunchStateTime` | 0.35 | How long the Launch state lasts before it becomes ordinary Air. |
| Wall | `WallRunSpeed` | 16.0 | Wall-run speed (traversal button held on a wall; runs up by default). |
| Wall | `WallCrawlSpeed` | 4.0 | Wall-crawl speed (button not held). No input = cling. |
| Wall | `WallAttachMinAngle` | 40.0 | Hitting a wall more head-on than this sticks to it; glancing hits just slide along. |
| Wall | `WallStickDistance` | 0.8 | How far past the body the wall probe looks to stay attached. |
| Wall | `WallJumpOutSpeed` | 10.0 | Jumping off a wall: speed away from it. |
| Wall | `WallJumpUpSpeed` | 9.0 | Jumping off a wall: upward speed. |
| Wall | `LedgeProbeHeight` | 1.2 | Height above the body centre of the "is the wall still there" head probe. |
| Wall | `LedgeSearchHeight` | 2.5 | How high above the probe a ledge top is searched for. |
| Wall | `VaultDuration` | 0.3 | Duration of the automatic mantle over a ledge. |
| Wall | `VaultRiseFraction` | 0.6 | Share of the vault spent rising before moving over the edge. |
| Wall | `VaultExitSpeed` | 6.0 | Forward speed at the end of a vault. |
| Glide | `bGlideUnlocked` | true | Web-wings are unlocked (mid-game in the campaign; on for the prototype). |
| Glide | `GlideSpeed` | 24.0 | Target forward speed while gliding. |
| Glide | `GlideAccel` | 12.0 | How fast glide speed and sink rate are reached. |
| Glide | `GlideTurnRate` | 70.0 | Bank-turn rate, degrees per second. |
| Glide | `GlideGravityScale` | 0.2 | Gravity multiplier while gliding. |
| Glide | `GlideSinkRate` | 4.0 | Maximum descent speed while gliding. |
| Camera | `CameraFovBase` | 80.0 | Field of view at rest (Section 11: about 80). |
| Camera | `CameraFovMax` | 105.0 | Field of view at top speed (Section 11: 100 to 110). |
| Camera | `CameraFovSpeedStart` | 10.0 | FOV starts widening at this speed. |
| Camera | `CameraFovSpeedFull` | 70.0 | FOV reaches the maximum at this speed. |
| Camera | `CameraArmBase` | 3.5 | Camera distance behind the hero at rest. |
| Camera | `CameraArmMax` | 5.5 | Camera distance behind the hero at top speed. |
| Camera | `CameraLagSpeed` | 10.0 | Camera position lag (higher = tighter). |
| Camera | `CameraLookAhead` | 2.0 | At top speed the camera aims this far ahead along the velocity. |
| Camera | `CameraMotionBlurMax` | 0.5 | Motion blur amount at top speed (0 at rest). |
| Camera | `CameraInterpSpeed` | 4.0 | How quickly FOV, arm length and look-ahead follow speed changes. |
