#pragma once

#include "CoreMinimal.h"
#include "Traversal/TraversalTypes.h"

/**
 * The traversal state machine and physics (Section 6.1), independent of the engine.
 *
 * The host (UHeroMovementComponent in the game, a box world in Tools/TraversalSim) owns collision:
 *   1. Ground: host walks the body, calls TickGround() for wall-run/vault entries.
 *   2. Everything else: host calls Tick(), sweeps Body.Move with collision, and calls OnBlocked()
 *      if the sweep hits something. Land means "switch to walking", then NotifyLanded().
 *   3. Host calls NotifyLeftGround() when walking turns into falling (jump or ledge).
 * Units are metres and seconds. The sim never allocates and never touches the engine.
 */
class FTraversalSim
{
public:
	void SetTuning(const FTraversalTuning* InTuning) { Tuning = InTuning; }
	const FTraversalTuning& GetTuning() const { return *Tuning; }

	/** Airborne and wall states. Updates Body.Velocity and fills Body.Move for the host to sweep. */
	void Tick(double Dt, const FTraversalInput& Input, FTraversalBody& Body, const ITraversalWorld& World);

	/** Ground state: timers, sprint into a wall = wall-run, sprint into a low obstacle = vault. */
	void TickGround(double Dt, const FTraversalInput& Input, FTraversalBody& Body, const ITraversalWorld& World);

	/** The host's sweep hit something. Adjusts Body.Velocity and tells the host how to continue. */
	ETraversalBlockResponse OnBlocked(const FTraversalHit& Hit, bool bWalkable, FTraversalBody& Body);

	void NotifyLanded();
	void NotifyLeftGround(bool bJumped);

	/** After a teleport (fast travel, respawn): drop everything and fall. */
	void ResetToAir();

	ETraversalState GetState() const { return State; }
	double GetStateTime() const { return StateTime; }
	const FTraversalAnchor& GetSwingAnchor() const { return SwingAnchor; }
	const FTraversalAnchor& GetPreviewAnchor() const { return PreviewAnchor; }
	double GetRopeLength() const { return RopeLength; }
	int32 GetSpeedTier() const;
	double GetSpeedCap() const;
	int32 GetChainCount() const { return ChainCount; }
	int32 GetSkyAnchorsLeft() const;
	/** Signed angle past the bottom of the current swing, degrees (negative = still descending). */
	double GetSwingAngle(const FVector& Position) const;
	bool IsZipping() const { return State == ETraversalState::Zip; }
	const FVector& GetZipTarget() const { return ZipTarget; }
	const FVector& GetWallNormal() const { return WallNormal; }
	/** Direction the body should face (horizontal, unit). */
	FVector GetFacing(const FTraversalBody& Body) const;

	const FTraversalStreak& GetStreak() const { return Streak; }
	const FTraversalStreak& GetLastStreak() const { return LastStreak; }
	const FTraversalStreak& GetBestStreak() const { return BestStreak; }

	/** Returns this frame's events and clears them. */
	FTraversalEvents ConsumeEvents();

	static FTraversalCameraTarget ComputeCameraTarget(const FTraversalTuning& T, const FVector& Velocity);
	static const TCHAR* StateName(ETraversalState InState);

private:
	enum class EZipPhase : uint8 { Travel, Perch };

	void SetState(ETraversalState NewState);
	void LeaveGround(bool bJumped);
	void UpdateTimers(double Dt, const FTraversalInput& Input);

	void TickAir(double Dt, const FTraversalInput& Input, FTraversalBody& Body, const ITraversalWorld& World);
	void TickSwing(double Dt, const FTraversalInput& Input, FTraversalBody& Body, const ITraversalWorld& World);
	void TickZip(double Dt, const FTraversalInput& Input, FTraversalBody& Body);
	void TickWall(double Dt, const FTraversalInput& Input, FTraversalBody& Body, const ITraversalWorld& World);
	void TickVault(double Dt, FTraversalBody& Body);
	void TickGlide(double Dt, const FTraversalInput& Input, FTraversalBody& Body, const ITraversalWorld& World);

	void IntegrateAir(double Dt, const FTraversalInput& Input, FTraversalBody& Body) const;
	void ApplyAirControl(FVector& Velocity, const FVector& Move, double Accel, double Dt) const;

	FTraversalAnchor FindAnchor(const FTraversalBody& Body, const FTraversalInput& Input, const ITraversalWorld& World) const;
	FTraversalAnchor MakeSkyAnchor(const FTraversalBody& Body, const FTraversalInput& Input) const;
	void StartSwing(const FTraversalAnchor& Anchor, const FTraversalBody& Body, const FTraversalInput& Input);
	void Release(FTraversalBody& Body, bool bAllowBoost, bool bJumpOff);
	bool TryStartZip(const FTraversalBody& Body, const FTraversalInput& Input, const ITraversalWorld& World);
	void StartVault(const FTraversalBody& Body, const FVector& LedgeTop, const FVector& FromNormal);

	bool ProbeWall(const FVector& From, const FVector& Normal, const FTraversalBody& Body, const ITraversalWorld& World, FTraversalHit& OutHit) const;
	bool FindLedge(const FVector& From, const FVector& Normal, const FTraversalBody& Body, const ITraversalWorld& World, FVector& OutTop) const;
	bool CanAttachSwing(const FTraversalInput& Input, const FTraversalBody& Body) const;

	void UpdateStreak(const FTraversalBody& Body, double Dt);
	void EndStreak();

	const FTraversalTuning* Tuning = nullptr;

	ETraversalState State = ETraversalState::Ground;
	double StateTime = 0.0;
	double AirTime = 0.0;
	double TimeSinceGround = 0.0;
	bool bLeftGroundByJump = false;
	double JumpBufferTimer = 0.0;
	double TimeSinceRelease = 1000.0;
	double ZipCooldownTimer = 0.0;
	double NoAnchorTimer = 0.0;
	int32 SkyAnchorsUsed = 0;
	int32 ChainCount = 0;

	FTraversalAnchor SwingAnchor;
	FTraversalAnchor PreviewAnchor;
	double RopeLength = 0.0;
	FVector SwingForward = FVector::ForwardVector;
	bool bAutoCatchSwing = false;

	EZipPhase ZipPhase = EZipPhase::Travel;
	FVector ZipStart = FVector::ZeroVector;
	FVector ZipTarget = FVector::ZeroVector;
	FVector ZipDirection = FVector::ForwardVector;
	FVector ZipWallNormal = FVector::ZeroVector;
	double ZipDuration = 0.0;
	double ZipElapsed = 0.0;
	bool bZipToPerch = false;
	bool bZipToWall = false;
	bool bLaunchQueued = false;

	FVector WallNormal = FVector::ZeroVector;

	FVector VaultStart = FVector::ZeroVector;
	FVector VaultTarget = FVector::ZeroVector;
	FVector VaultNormal = FVector::ZeroVector;
	double VaultElapsed = 0.0;

	FVector LastFacing = FVector::ForwardVector;

	FTraversalStreak Streak;
	FTraversalStreak LastStreak;
	FTraversalStreak BestStreak;
	bool bStreakMinSpeedSet = false;
	FTraversalEvents Events;
};
