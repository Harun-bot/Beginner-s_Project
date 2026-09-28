#pragma once

// Traversal core types (Section 6.1). Everything here is in metres, seconds and degrees.
// The core (this header + TraversalSim) has no engine dependencies beyond FVector/FMath, so
// Tools/TraversalSim can unit-test it and run swing bots outside Unreal.

#include "CoreMinimal.h"
#include "TraversalTypes.generated.h"

/** Traversal states. The movement component maps Ground to Walking, Air/Launch to Falling and the rest to Custom. */
UENUM(BlueprintType)
enum class ETraversalState : uint8
{
	Ground,
	Air,
	Swing,
	Zip,
	Launch,
	Glide,
	WallRun,
	WallCling,
	Vault,
	Count UMETA(Hidden)
};

/**
 * Every traversal number, in one struct so it can live in a data asset (DA_TraversalTuning),
 * be edited while playing (live-tuning panel, F2) and be unit-tested. Units: m, m/s, m/s^2, s, degrees.
 */
USTRUCT(BlueprintType)
struct FTraversalTuning
{
	GENERATED_BODY()

	// ---------------------------------------------------------------- Ground

	/** Jog speed on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Ground", meta = (ClampMin = "0"))
	double GroundRunSpeed = 7.0;

	/** Sprint/parkour speed while holding the traversal button on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Ground", meta = (ClampMin = "0"))
	double GroundSprintSpeed = 14.0;

	/** Upward speed of a jump (also used for coyote jumps). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Ground", meta = (ClampMin = "0"))
	double GroundJumpSpeed = 9.0;

	/** Sprinting into an obstacle up to this tall auto-vaults over it; taller walls start a wall-run. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Ground", meta = (ClampMin = "0"))
	double GroundVaultMaxHeight = 2.2;

	// ---------------------------------------------------------------- Air

	/** Real-world gravity; the per-state scales below multiply it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Air", meta = (ClampMin = "0"))
	double Gravity = 9.81;

	/** Gravity multiplier while airborne (not swinging). Above 1 feels snappier than real life. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Air", meta = (ClampMin = "0"))
	double AirGravityScale = 1.6;

	/** Sideways steering acceleration in the air. Can turn you, never speeds you past sprint speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Air", meta = (ClampMin = "0"))
	double AirControlAccel = 10.0;

	/** Terminal fall speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Air", meta = (ClampMin = "0"))
	double AirMaxFallSpeed = 60.0;

	/** After walking off a ledge, jump still works for this long. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Air", meta = (ClampMin = "0"))
	double AirCoyoteTime = 0.15;

	/** A jump press is remembered this long, so slightly early presses still count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Air", meta = (ClampMin = "0"))
	double AirJumpBufferTime = 0.15;

	// ---------------------------------------------------------------- Anchor selection

	/** How far the web can reach. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0"))
	double AnchorReach = 80.0;

	/** Full width of the search cone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0", ClampMax = "180"))
	double AnchorConeAngle = 90.0;

	/** Cone centre, degrees above the horizon, when the camera is level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0", ClampMax = "90"))
	double AnchorConePitch = 45.0;

	/** How much camera pitch tilts the cone (0 = ignore camera pitch, 1 = follow it fully). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0", ClampMax = "1"))
	double AnchorCameraPitchInfluence = 0.5;

	/** Preferred anchor elevation band (Section 6.1: 35 to 55 degrees above the horizon). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0", ClampMax = "90"))
	double AnchorIdealElevationMin = 35.0;

	/** Upper end of the preferred elevation band. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0", ClampMax = "90"))
	double AnchorIdealElevationMax = 55.0;

	/** Anchors lower than this are never used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0", ClampMax = "90"))
	double AnchorMinElevation = 15.0;

	/** Preferred distance to the anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0"))
	double AnchorIdealDistance = 40.0;

	/** The swing pivots this far out from the wall, so arcs don't scrape the building. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0"))
	double AnchorSurfaceOffset = 2.0;

	/** Radius of the anchor search traces (thin geometry is easier to hit with a bigger value). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0"))
	double AnchorTraceRadius = 0.3;

	/** Search rays across the cone. Cost is yaw x pitch traces per airborne frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "1", ClampMax = "15"))
	int32 AnchorSamplesYaw = 7;

	/** Search rays up the cone (elevation steps between the minimum elevation and the cone top). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "1", ClampMax = "9"))
	int32 AnchorSamplesPitch = 5;

	/** Score weight for being in the ideal elevation band. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0"))
	double AnchorWeightElevation = 1.0;

	/** Score weight for being near the ideal distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0"))
	double AnchorWeightDistance = 0.5;

	/** Score weight for being where the camera looks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0"))
	double AnchorWeightAlignment = 0.8;

	/** Extra score for props tagged "WebAnchor" (water tanks, cranes, lampposts). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0"))
	double AnchorPropBonus = 0.3;

	/** Soft magnetism: extra score for staying on the previously previewed anchor, so the preview doesn't flicker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0"))
	double AnchorHysteresis = 0.15;

	/** Score penalty for walls facing us head-on: swinging at them slams us into them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0"))
	double AnchorFacingPenalty = 0.6;

	/** A candidate this close to the previous preview counts as "the same anchor" for the hysteresis bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Anchor", meta = (ClampMin = "0"))
	double AnchorHysteresisRadius = 3.0;

	// ---------------------------------------------------------------- Swing

	/** Gravity multiplier on the rope (Section 6.1: 1.5 to 2.5x). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SwingGravityScale = 2.0;

	/** Shortest rope; closer anchors are ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SwingRopeMinLength = 8.0;

	/** Longest rope; farther anchors are ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SwingRopeMaxLength = 80.0;

	/** How fast the rope shortens to keep the bottom of the arc off the street. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SwingRopeReelSpeed = 25.0;

	/** How far the swing pivot moves from the wall toward our line of travel (0 = pure physics, 1 = swing straight along our heading). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0", ClampMax = "1"))
	double SwingPlaneAlign = 0.85;

	/** The lowest point of a swing stays at least this far above the ground below the anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SwingGroundClearance = 6.0;

	/** The low-point boost applies within this many degrees either side of the bottom of the arc. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0", ClampMax = "90"))
	double SwingLowPointAngle = 35.0;

	/** Tangential acceleration through the low point, up to the current speed-tier cap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SwingLowPointBoost = 18.0;

	/** Speed floor on the rope: a swing never stalls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SwingMinSpeed = 12.0;

	/** Steering acceleration while swinging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SwingAirControl = 8.0;

	/** Fraction of speed lost per second on the rope. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0", ClampMax = "1"))
	double SwingDamping = 0.02;

	/** Past the low point, the web lets go by itself at this angle (or when you stop rising). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0", ClampMax = "120"))
	double SwingAutoReleaseAngle = 65.0;

	/** Past this angle, the web also lets go as soon as you stop rising (top of the arc). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0", ClampMax = "90"))
	double SwingApexMinAngle = 10.0;

	/** While the button stays held, the next web fires this long after a release. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SwingReattachDelay = 0.12;

	/** A new web only fires once you are rising slower than this, i.e. near the top of the arc. Webs fired while rising fast swing you backwards. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SwingAttachMaxRiseSpeed = 3.0;

	/** After leaving the ground, the first web can fire after this long. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SwingMinAirTime = 0.1;

	/** A swing that starts within this long of the last release counts as a chain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SwingChainWindow = 1.5;

	/** Chained swings needed per speed tier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "1"))
	int32 SwingsPerTier = 3;

	/** A well-timed release counts as this many extra chained swings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	int32 PerfectReleaseChainBonus = 1;

	/** Speed cap at tier 1 (Section 6.1 starting values: about 25, 45, 70 m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SpeedTier1 = 25.0;

	/** Speed cap at tier 2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SpeedTier2 = 45.0;

	/** Speed cap at tier 3. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SpeedTier3 = 70.0;

	/** Above the tier cap, this fraction of speed bleeds off per second (soft cap, not a wall). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double SpeedOverCapDrag = 0.8;

	/** Perfect-release window start, degrees past the low point (negative = just before it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "-90", ClampMax = "90"))
	double ReleaseWindowMinAngle = -5.0;

	/** Perfect-release window end, degrees past the low point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "-90", ClampMax = "90"))
	double ReleaseWindowMaxAngle = 35.0;

	/** Perfect-release boost along the direction of travel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double ReleaseBoostSpeed = 6.0;

	/** Perfect-release boost upwards (kept small: height comes from anchors, not boosts). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double ReleaseBoostUp = 2.0;

	/** Pressing jump on the rope lets go with this much extra upward speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Swing", meta = (ClampMin = "0"))
	double ReleaseJumpUp = 8.0;

	// ---------------------------------------------------------------- Fallbacks (never dead-end)

	/** In wide-open spaces with nothing to hit, fire a limited "sky anchor". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Fallbacks")
	bool bSkyAnchorEnabled = true;

	/** Seconds of holding swing with no real anchor before a sky anchor fires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Fallbacks", meta = (ClampMin = "0"))
	double SkyAnchorDelay = 0.25;

	/** Sky anchors allowed in a row. Swinging from a real surface, or touching down, refills them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Fallbacks", meta = (ClampMin = "0"))
	int32 SkyAnchorMaxInARow = 2;

	/** Sky anchor height above you. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Fallbacks", meta = (ClampMin = "0"))
	double SkyAnchorHeight = 30.0;

	/** Sky anchor distance ahead of you. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Fallbacks", meta = (ClampMin = "0"))
	double SkyAnchorForward = 25.0;

	/** Accessibility: fire a web automatically before hitting the street at speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Fallbacks")
	bool bAutoWebCatch = true;

	/** Auto web-catch triggers when impact is this close (seconds)... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Fallbacks", meta = (ClampMin = "0"))
	double AutoWebCatchTime = 0.7;

	/** ...and you are falling at least this fast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Fallbacks", meta = (ClampMin = "0"))
	double AutoWebCatchMinFallSpeed = 15.0;

	// ---------------------------------------------------------------- Zip and point-launch

	/** Web-zip reach (jump button in the air). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Zip", meta = (ClampMin = "0"))
	double ZipRange = 45.0;

	/** Zip travel speed; the duration is clamped to the min/max below (Section 6.1: 0.3 to 0.5 s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Zip", meta = (ClampMin = "1"))
	double ZipSpeed = 60.0;

	/** Shortest zip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Zip", meta = (ClampMin = "0.05"))
	double ZipMinDuration = 0.3;

	/** Longest zip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Zip", meta = (ClampMin = "0.05"))
	double ZipMaxDuration = 0.5;

	/** With nothing to zip to, you still dart this far in the camera direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Zip", meta = (ClampMin = "0"))
	double ZipNoTargetDistance = 20.0;

	/** Speed carried out of a zip that didn't end on a surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Zip", meta = (ClampMin = "0"))
	double ZipExitSpeed = 14.0;

	/** Minimum time between zips. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Zip", meta = (ClampMin = "0"))
	double ZipCooldown = 0.2;

	/** Zips to a perch arrive this far above it, so the approach clears the ledge lip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Zip", meta = (ClampMin = "0"))
	double ZipPerchClearance = 0.15;

	/** Point-launch: press jump within this long of arriving on a perch (before or after). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Zip", meta = (ClampMin = "0"))
	double LaunchWindow = 0.25;

	/** Point-launch forward speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Zip", meta = (ClampMin = "0"))
	double LaunchForwardSpeed = 22.0;

	/** Point-launch upward speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Zip", meta = (ClampMin = "0"))
	double LaunchUpSpeed = 16.0;

	/** How long the Launch state lasts before it becomes ordinary Air. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Zip", meta = (ClampMin = "0"))
	double LaunchStateTime = 0.35;

	// ---------------------------------------------------------------- Walls and ledges

	/** Wall-run speed (traversal button held on a wall; runs up by default). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Wall", meta = (ClampMin = "0"))
	double WallRunSpeed = 16.0;

	/** Wall-crawl speed (button not held). No input = cling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Wall", meta = (ClampMin = "0"))
	double WallCrawlSpeed = 4.0;

	/** Hitting a wall more head-on than this sticks to it; glancing hits just slide along. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Wall", meta = (ClampMin = "0", ClampMax = "90"))
	double WallAttachMinAngle = 40.0;

	/** How far past the body the wall probe looks to stay attached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Wall", meta = (ClampMin = "0"))
	double WallStickDistance = 0.8;

	/** Jumping off a wall: speed away from it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Wall", meta = (ClampMin = "0"))
	double WallJumpOutSpeed = 10.0;

	/** Jumping off a wall: upward speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Wall", meta = (ClampMin = "0"))
	double WallJumpUpSpeed = 9.0;

	/** Height above the body centre of the "is the wall still there" head probe. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Wall", meta = (ClampMin = "0"))
	double LedgeProbeHeight = 1.2;

	/** How high above the probe a ledge top is searched for. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Wall", meta = (ClampMin = "0"))
	double LedgeSearchHeight = 2.5;

	/** Duration of the automatic mantle over a ledge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Wall", meta = (ClampMin = "0.05"))
	double VaultDuration = 0.3;

	/** Share of the vault spent rising before moving over the edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Wall", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	double VaultRiseFraction = 0.6;

	/** Forward speed at the end of a vault. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Wall", meta = (ClampMin = "0"))
	double VaultExitSpeed = 6.0;

	// ---------------------------------------------------------------- Glide (stub; unlocked mid-game)

	/** Web-wings are unlocked (mid-game in the campaign; on for the prototype). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Glide")
	bool bGlideUnlocked = true;

	/** Target forward speed while gliding. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Glide", meta = (ClampMin = "0"))
	double GlideSpeed = 24.0;

	/** How fast glide speed and sink rate are reached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Glide", meta = (ClampMin = "0"))
	double GlideAccel = 12.0;

	/** Bank-turn rate, degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Glide", meta = (ClampMin = "0"))
	double GlideTurnRate = 70.0;

	/** Gravity multiplier while gliding. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Glide", meta = (ClampMin = "0"))
	double GlideGravityScale = 0.2;

	/** Maximum descent speed while gliding. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Glide", meta = (ClampMin = "0"))
	double GlideSinkRate = 4.0;

	// ---------------------------------------------------------------- Camera (Section 11)

	/** Field of view at rest (Section 11: about 80). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Camera", meta = (ClampMin = "40", ClampMax = "140"))
	double CameraFovBase = 80.0;

	/** Field of view at top speed (Section 11: 100 to 110). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Camera", meta = (ClampMin = "40", ClampMax = "140"))
	double CameraFovMax = 105.0;

	/** FOV starts widening at this speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Camera", meta = (ClampMin = "0"))
	double CameraFovSpeedStart = 10.0;

	/** FOV reaches the maximum at this speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Camera", meta = (ClampMin = "0"))
	double CameraFovSpeedFull = 70.0;

	/** Camera distance behind the hero at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Camera", meta = (ClampMin = "0"))
	double CameraArmBase = 3.5;

	/** Camera distance behind the hero at top speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Camera", meta = (ClampMin = "0"))
	double CameraArmMax = 5.5;

	/** Camera position lag (higher = tighter). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Camera", meta = (ClampMin = "0"))
	double CameraLagSpeed = 10.0;

	/** At top speed the camera aims this far ahead along the velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Camera", meta = (ClampMin = "0"))
	double CameraLookAhead = 2.0;

	/** Motion blur amount at top speed (0 at rest). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Camera", meta = (ClampMin = "0", ClampMax = "1"))
	double CameraMotionBlurMax = 0.5;

	/** How quickly FOV, arm length and look-ahead follow speed changes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal|Camera", meta = (ClampMin = "0"))
	double CameraInterpSpeed = 4.0;
};

/** Player intent for one frame, already converted to world space by the host. */
struct FTraversalInput
{
	/** Desired move direction in world XY, camera-relative, length 0..1. */
	FVector Move = FVector::ZeroVector;

	/** Camera look direction (unit). Steers anchor search and zips. */
	FVector CameraForward = FVector::ForwardVector;

	/** Swing in the air, sprint on the ground, wall-run on walls. */
	bool bTraversalHeld = false;

	/** True only on the frame the button went down; the sim buffers it. Pass it once per frame. */
	bool bJumpPressed = false;
	bool bJumpHeld = false;

	/** True only on the frame the glide button went down. */
	bool bGlideTogglePressed = false;
};

/** Result of a world trace. */
struct FTraversalHit
{
	/** Impact point on the surface. */
	FVector Location = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	/** Distance travelled by the trace before the hit. */
	double Distance = 0.0;
	/** The surface belongs to an actor tagged "WebAnchor". */
	bool bAnchorProp = false;
};

/** What the sim needs from the world. The engine implements it with traces; tests use boxes. */
class ITraversalWorld
{
public:
	virtual ~ITraversalWorld() = default;

	/** Sphere sweep (Radius 0 = line trace) from Start to End; nearest blocking hit. */
	virtual bool Trace(const FVector& Start, const FVector& End, double Radius, FTraversalHit& OutHit) const = 0;
};

/** The moving body: position is the capsule centre. */
struct FTraversalBody
{
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	double Radius = 0.42;
	double HalfHeight = 0.96;

	/** Output of FTraversalSim::Tick: the displacement the host should sweep this step. */
	FVector Move = FVector::ZeroVector;
};

/** What the host should do after its sweep was blocked. */
enum class ETraversalBlockResponse : uint8
{
	Slide,  // slide the rest of the move along the surface
	Land,   // switch to walking
	Attach  // stop; the sim now holds on to the wall
};

struct FTraversalAnchor
{
	bool bValid = false;
	/** The swing rotates around this point (surface point pushed out along the normal). */
	FVector Pivot = FVector::ZeroVector;
	/** Where the web visibly sticks. */
	FVector SurfacePoint = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	double Score = 0.0;
	bool bSky = false;
};

/** One continuous stretch without touching down: the Phase 1 "2 km without touching the ground" measure. */
struct FTraversalStreak
{
	/** Horizontal distance covered. */
	double Distance = 0.0;
	double Time = 0.0;
	/** Slowest speed after the first second of the streak. */
	double MinSpeed = 0.0;
	double MaxSpeed = 0.0;
	int32 MaxTier = 0;
	int32 Swings = 0;
	int32 PerfectReleases = 0;
};

/** One-frame notifications for HUD, audio and VFX. Read them with FTraversalSim::ConsumeEvents. */
struct FTraversalEvents
{
	bool bSwingStarted = false;
	bool bReleased = false;
	bool bPerfectRelease = false;
	bool bTierUp = false;
	bool bSkyAnchor = false;
	bool bAutoCatch = false;
	bool bZip = false;
	bool bLaunch = false;
	bool bWallAttach = false;
	bool bVault = false;
	bool bLanded = false;
	bool bStreakEnded = false;
};

/** Camera targets for the current speed (Section 11 speed-scaled FOV). */
struct FTraversalCameraTarget
{
	double Fov = 80.0;
	double ArmLength = 3.5;
	double MotionBlur = 0.0;
	FVector LookAhead = FVector::ZeroVector;
};
