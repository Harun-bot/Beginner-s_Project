#include "Traversal/TraversalSim.h"

namespace TSim
{
	FVector Horizontal(const FVector& V)
	{
		return FVector(V.X, V.Y, 0.0);
	}

	FVector RotateZ(const FVector& V, double Radians)
	{
		const double C = FMath::Cos(Radians);
		const double S = FMath::Sin(Radians);
		return FVector(V.X * C - V.Y * S, V.X * S + V.Y * C, V.Z);
	}
}

// ------------------------------------------------------------------ queries

int32 FTraversalSim::GetSpeedTier() const
{
	return FMath::Clamp(ChainCount / FMath::Max(1, Tuning->SwingsPerTier), 0, 2);
}

double FTraversalSim::GetSpeedCap() const
{
	switch (GetSpeedTier())
	{
	case 0: return Tuning->SpeedTier1;
	case 1: return Tuning->SpeedTier2;
	default: return Tuning->SpeedTier3;
	}
}

int32 FTraversalSim::GetSkyAnchorsLeft() const
{
	return Tuning->bSkyAnchorEnabled ? FMath::Max(0, Tuning->SkyAnchorMaxInARow - SkyAnchorsUsed) : 0;
}

double FTraversalSim::GetSwingAngle(const FVector& Position) const
{
	if (!SwingAnchor.bValid)
	{
		return 0.0;
	}
	const FVector R = (Position - SwingAnchor.Pivot).GetSafeNormal();
	const double Ahead = FVector::DotProduct(TSim::Horizontal(R), SwingForward);
	return FMath::RadiansToDegrees(FMath::Atan2(Ahead, -R.Z));
}

FVector FTraversalSim::GetFacing(const FTraversalBody& Body) const
{
	switch (State)
	{
	case ETraversalState::WallRun:
	case ETraversalState::WallCling:
		return (-WallNormal).GetSafeNormal(UE_SMALL_NUMBER, LastFacing);
	case ETraversalState::Vault:
		return (-VaultNormal).GetSafeNormal(UE_SMALL_NUMBER, LastFacing);
	case ETraversalState::Zip:
		return TSim::Horizontal(ZipDirection).GetSafeNormal(UE_SMALL_NUMBER, LastFacing);
	default:
		break;
	}
	const FVector Hor = TSim::Horizontal(Body.Velocity);
	return Hor.SizeSquared() > 1.0 ? Hor.GetSafeNormal() : LastFacing;
}

FTraversalEvents FTraversalSim::ConsumeEvents()
{
	const FTraversalEvents Out = Events;
	Events = FTraversalEvents();
	return Out;
}

FTraversalCameraTarget FTraversalSim::ComputeCameraTarget(const FTraversalTuning& T, const FVector& Velocity)
{
	const double Range = FMath::Max(0.01, T.CameraFovSpeedFull - T.CameraFovSpeedStart);
	const double X = FMath::Clamp((Velocity.Size() - T.CameraFovSpeedStart) / Range, 0.0, 1.0);
	const double Alpha = X * X * (3.0 - 2.0 * X);

	FTraversalCameraTarget Out;
	Out.Fov = FMath::Lerp(T.CameraFovBase, T.CameraFovMax, Alpha);
	Out.ArmLength = FMath::Lerp(T.CameraArmBase, T.CameraArmMax, Alpha);
	Out.MotionBlur = T.CameraMotionBlurMax * Alpha;
	Out.LookAhead = Velocity.GetSafeNormal() * (T.CameraLookAhead * Alpha);
	return Out;
}

const TCHAR* FTraversalSim::StateName(ETraversalState InState)
{
	switch (InState)
	{
	case ETraversalState::Ground: return TEXT("Ground");
	case ETraversalState::Air: return TEXT("Air");
	case ETraversalState::Swing: return TEXT("Swing");
	case ETraversalState::Zip: return TEXT("Zip");
	case ETraversalState::Launch: return TEXT("Launch");
	case ETraversalState::Glide: return TEXT("Glide");
	case ETraversalState::WallRun: return TEXT("WallRun");
	case ETraversalState::WallCling: return TEXT("WallCling");
	case ETraversalState::Vault: return TEXT("Vault");
	default: return TEXT("?");
	}
}

// ------------------------------------------------------------------ ground / air transitions

void FTraversalSim::NotifyLanded()
{
	if (State != ETraversalState::Ground)
	{
		EndStreak();
		Events.bLanded = true;
	}
	SetState(ETraversalState::Ground);
	ChainCount = 0;
	SkyAnchorsUsed = 0;
	NoAnchorTimer = 0.0;
	AirTime = 0.0;
	TimeSinceGround = 0.0;
	bLeftGroundByJump = false;
	bAutoCatchSwing = false;
	PreviewAnchor = FTraversalAnchor();
}

void FTraversalSim::NotifyLeftGround(bool bJumped)
{
	if (State == ETraversalState::Ground)
	{
		LeaveGround(bJumped);
		SetState(ETraversalState::Air);
	}
}

void FTraversalSim::LeaveGround(bool bJumped)
{
	bLeftGroundByJump = bJumped;
	if (bJumped)
	{
		JumpBufferTimer = 0.0;
	}
	TimeSinceGround = 0.0;
	AirTime = 0.0;
	Streak = FTraversalStreak();
	bStreakMinSpeedSet = false;
}

void FTraversalSim::ResetToAir()
{
	EndStreak();
	LeaveGround(true);
	SetState(ETraversalState::Air);
	ChainCount = 0;
	SkyAnchorsUsed = 0;
	NoAnchorTimer = 0.0;
	JumpBufferTimer = 0.0;
	bAutoCatchSwing = false;
	SwingAnchor = FTraversalAnchor();
	PreviewAnchor = FTraversalAnchor();
}

void FTraversalSim::SetState(ETraversalState NewState)
{
	if (State == NewState)
	{
		return;
	}
	if (State == ETraversalState::Swing)
	{
		SwingAnchor = FTraversalAnchor();
	}
	State = NewState;
	StateTime = 0.0;
}

void FTraversalSim::UpdateTimers(double Dt, const FTraversalInput& Input)
{
	StateTime += Dt;
	TimeSinceRelease += Dt;
	ZipCooldownTimer = FMath::Max(0.0, ZipCooldownTimer - Dt);
	JumpBufferTimer = Input.bJumpPressed ? Tuning->AirJumpBufferTime : FMath::Max(0.0, JumpBufferTimer - Dt);
	if (State != ETraversalState::Ground)
	{
		AirTime += Dt;
		TimeSinceGround += Dt;
	}
}

// ------------------------------------------------------------------ main entry points

void FTraversalSim::Tick(double Dt, const FTraversalInput& Input, FTraversalBody& Body, const ITraversalWorld& World)
{
	Body.Move = FVector::ZeroVector;
	if (!Tuning || Dt <= 0.0)
	{
		return;
	}

	if (State == ETraversalState::Ground)
	{
		// The host started airborne physics without telling us (knocked off a ledge, spawned in the air).
		NotifyLeftGround(false);
	}
	UpdateTimers(Dt, Input);

	const bool bCouldSwingSoon = State == ETraversalState::Air || State == ETraversalState::Launch
		|| State == ETraversalState::Glide || State == ETraversalState::Swing;
	PreviewAnchor = bCouldSwingSoon ? FindAnchor(Body, Input, World) : FTraversalAnchor();

	switch (State)
	{
	case ETraversalState::Air:
	case ETraversalState::Launch:
		TickAir(Dt, Input, Body, World);
		break;
	case ETraversalState::Swing:
		TickSwing(Dt, Input, Body, World);
		break;
	case ETraversalState::Zip:
		TickZip(Dt, Input, Body);
		break;
	case ETraversalState::WallRun:
	case ETraversalState::WallCling:
		TickWall(Dt, Input, Body, World);
		break;
	case ETraversalState::Vault:
		TickVault(Dt, Body);
		break;
	case ETraversalState::Glide:
		TickGlide(Dt, Input, Body, World);
		break;
	default:
		IntegrateAir(Dt, Input, Body);
		break;
	}

	UpdateStreak(Body, Dt);
	const FVector Hor = TSim::Horizontal(Body.Velocity);
	if (Hor.SizeSquared() > 1.0)
	{
		LastFacing = Hor.GetSafeNormal();
	}
}

void FTraversalSim::TickGround(double Dt, const FTraversalInput& Input, FTraversalBody& Body, const ITraversalWorld& World)
{
	if (!Tuning || Dt <= 0.0)
	{
		return;
	}
	if (State != ETraversalState::Ground)
	{
		NotifyLanded();
	}
	UpdateTimers(Dt, Input);

	const FTraversalTuning& T = *Tuning;
	const FVector Move = TSim::Horizontal(Input.Move);
	if (!Input.bTraversalHeld || Move.Size() < 0.3)
	{
		return;
	}

	// Sprinting into something: tall wall = wall-run, low obstacle = vault over it.
	const FVector Dir = Move.GetSafeNormal();
	FTraversalHit Hit;
	if (!World.Trace(Body.Position, Body.Position + Dir * (Body.Radius + T.WallStickDistance), 0.0, Hit))
	{
		return;
	}
	if (FMath::Abs(Hit.Normal.Z) >= 0.3 || FVector::DotProduct(Dir, -Hit.Normal) < 0.7)
	{
		return;
	}
	const FVector Normal = TSim::Horizontal(Hit.Normal).GetSafeNormal();

	FTraversalHit HeadHit;
	if (ProbeWall(Body.Position + FVector::UpVector * T.LedgeProbeHeight, Normal, Body, World, HeadHit))
	{
		LeaveGround(true);
		WallNormal = Normal;
		Events.bWallAttach = true;
		SetState(ETraversalState::WallRun);
		return;
	}

	FVector Top;
	if (FindLedge(Body.Position, Normal, Body, World, Top) && Top.Z - (Body.Position.Z - Body.HalfHeight) <= T.GroundVaultMaxHeight)
	{
		LeaveGround(true);
		StartVault(Body, Top, Normal);
	}
}

ETraversalBlockResponse FTraversalSim::OnBlocked(const FTraversalHit& Hit, bool bWalkable, FTraversalBody& Body)
{
	const FTraversalTuning& T = *Tuning;
	const FVector V = Body.Velocity;

	switch (State)
	{
	case ETraversalState::Ground:
	case ETraversalState::Vault:
		return ETraversalBlockResponse::Slide;

	case ETraversalState::WallRun:
	case ETraversalState::WallCling:
		if (bWalkable && V.Z <= 0.0)
		{
			return ETraversalBlockResponse::Land;
		}
		if (FVector::DotProduct(V, Hit.Normal) < 0.0)
		{
			Body.Velocity = FVector::VectorPlaneProject(V, Hit.Normal);
		}
		return ETraversalBlockResponse::Slide;

	default:
		break;
	}

	if (bWalkable)
	{
		return ETraversalBlockResponse::Land;
	}

	// A wall: stick to it if we hit it fairly head-on, otherwise slide along (no instant fail when brushing walls).
	if (FMath::Abs(Hit.Normal.Z) < 0.3)
	{
		const double Speed = V.Size();
		const double HeadOn = Speed > 0.1 ? FVector::DotProduct(V / Speed, -Hit.Normal) : 1.0;
		if (State == ETraversalState::Zip || HeadOn >= FMath::Sin(FMath::DegreesToRadians(T.WallAttachMinAngle)))
		{
			WallNormal = TSim::Horizontal(Hit.Normal).GetSafeNormal();
			Body.Velocity = FVector::ZeroVector;
			Events.bWallAttach = true;
			SetState(ETraversalState::WallCling);
			return ETraversalBlockResponse::Attach;
		}
	}

	if (FVector::DotProduct(V, Hit.Normal) < 0.0)
	{
		Body.Velocity = FVector::VectorPlaneProject(V, Hit.Normal);
	}
	return ETraversalBlockResponse::Slide;
}

// ------------------------------------------------------------------ air

void FTraversalSim::TickAir(double Dt, const FTraversalInput& Input, FTraversalBody& Body, const ITraversalWorld& World)
{
	const FTraversalTuning& T = *Tuning;

	if (State == ETraversalState::Launch && StateTime >= T.LaunchStateTime)
	{
		SetState(ETraversalState::Air);
	}

	// Jump in the air: a coyote jump just after walking off a ledge, otherwise a web-zip.
	if (JumpBufferTimer > 0.0)
	{
		if (!bLeftGroundByJump && TimeSinceGround <= T.AirCoyoteTime)
		{
			JumpBufferTimer = 0.0;
			bLeftGroundByJump = true;
			Body.Velocity.Z = FMath::Max(Body.Velocity.Z, T.GroundJumpSpeed);
		}
		else if (ZipCooldownTimer <= 0.0 && TryStartZip(Body, Input, World))
		{
			JumpBufferTimer = 0.0;
			TickZip(Dt, Input, Body);
			return;
		}
	}

	if (Input.bGlideTogglePressed && T.bGlideUnlocked)
	{
		SetState(ETraversalState::Glide);
		TickGlide(Dt, Input, Body, World);
		return;
	}

	if (CanAttachSwing(Input, Body))
	{
		if (PreviewAnchor.bValid)
		{
			StartSwing(PreviewAnchor, Body, Input);
			TickSwing(Dt, Input, Body, World);
			return;
		}
		// Nothing to hit: after a short wait, a limited sky anchor so open spaces are never a dead end.
		NoAnchorTimer += Dt;
		if (NoAnchorTimer >= T.SkyAnchorDelay && GetSkyAnchorsLeft() > 0)
		{
			++SkyAnchorsUsed;
			Events.bSkyAnchor = true;
			StartSwing(MakeSkyAnchor(Body, Input), Body, Input);
			TickSwing(Dt, Input, Body, World);
			return;
		}
	}
	else
	{
		NoAnchorTimer = 0.0;
	}

	// Accessibility: catch a hard fall with a web before impact.
	if (T.bAutoWebCatch && !Input.bTraversalHeld && Body.Velocity.Z < -T.AutoWebCatchMinFallSpeed)
	{
		const double LookDown = -Body.Velocity.Z * T.AutoWebCatchTime + Body.HalfHeight;
		FTraversalHit Below;
		if (World.Trace(Body.Position, Body.Position - FVector::UpVector * LookDown, Body.Radius, Below))
		{
			FTraversalAnchor Catch = PreviewAnchor;
			if (!Catch.bValid && GetSkyAnchorsLeft() > 0)
			{
				++SkyAnchorsUsed;
				Catch = MakeSkyAnchor(Body, Input);
			}
			if (Catch.bValid)
			{
				StartSwing(Catch, Body, Input);
				bAutoCatchSwing = true;
				Events.bAutoCatch = true;
				TickSwing(Dt, Input, Body, World);
				return;
			}
		}
	}

	IntegrateAir(Dt, Input, Body);
}

void FTraversalSim::IntegrateAir(double Dt, const FTraversalInput& Input, FTraversalBody& Body) const
{
	const FTraversalTuning& T = *Tuning;
	FVector V = Body.Velocity;
	V.Z -= T.Gravity * T.AirGravityScale * Dt;
	ApplyAirControl(V, Input.Move, T.AirControlAccel, Dt);
	V.Z = FMath::Max(V.Z, -T.AirMaxFallSpeed);
	Body.Velocity = V;
	Body.Move = V * Dt;
}

void FTraversalSim::ApplyAirControl(FVector& Velocity, const FVector& Move, double Accel, double Dt) const
{
	// Steering can turn you, but never pushes horizontal speed past max(current, sprint).
	const FVector Hor = TSim::Horizontal(Velocity);
	const double Limit = FMath::Max(Hor.Size(), Tuning->GroundSprintSpeed);
	FVector NewHor = Hor + TSim::Horizontal(Move) * (Accel * Dt);
	const double NewSize = NewHor.Size();
	if (NewSize > Limit)
	{
		NewHor *= Limit / NewSize;
	}
	Velocity.X = NewHor.X;
	Velocity.Y = NewHor.Y;
}

bool FTraversalSim::CanAttachSwing(const FTraversalInput& Input, const FTraversalBody& Body) const
{
	return Input.bTraversalHeld
		&& TimeSinceRelease >= Tuning->SwingReattachDelay
		&& AirTime >= Tuning->SwingMinAirTime
		&& Body.Velocity.Z <= Tuning->SwingAttachMaxRiseSpeed;
}

// ------------------------------------------------------------------ anchors

FTraversalAnchor FTraversalSim::FindAnchor(const FTraversalBody& Body, const FTraversalInput& Input, const ITraversalWorld& World) const
{
	const FTraversalTuning& T = *Tuning;
	FTraversalAnchor Best;

	const FVector Origin = Body.Position + FVector::UpVector * (Body.HalfHeight * 0.6);
	const FVector CamForward = Input.CameraForward.GetSafeNormal(UE_SMALL_NUMBER, LastFacing);
	FVector ForwardH = TSim::Horizontal(CamForward).GetSafeNormal();
	if (ForwardH.IsNearlyZero())
	{
		ForwardH = TSim::Horizontal(Body.Velocity).GetSafeNormal(UE_SMALL_NUMBER, LastFacing);
	}
	const FVector Side(-ForwardH.Y, ForwardH.X, 0.0);

	// The cone points up-and-forward; looking up or down tilts it.
	const double CamPitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(CamForward.Z, -1.0, 1.0)));
	const double AxisPitch = FMath::Clamp(T.AnchorConePitch + CamPitch * T.AnchorCameraPitchInfluence, 10.0, 80.0);
	const double Half = T.AnchorConeAngle * 0.5;
	const int32 NumYaw = FMath::Max(1, T.AnchorSamplesYaw);
	const int32 NumPitch = FMath::Max(1, T.AnchorSamplesPitch);
	// Only sample elevations that could be used, so every ray counts.
	const double PitchLow = FMath::Max(T.AnchorMinElevation, AxisPitch - Half);
	const double PitchHigh = FMath::Max(PitchLow, FMath::Min(85.0, AxisPitch + Half));

	for (int32 YawIndex = 0; YawIndex < NumYaw; ++YawIndex)
	{
		const double YawOffset = NumYaw > 1 ? FMath::Lerp(-Half, Half, double(YawIndex) / double(NumYaw - 1)) : 0.0;
		const double YawRad = FMath::DegreesToRadians(YawOffset);
		const FVector DirH = ForwardH * FMath::Cos(YawRad) + Side * FMath::Sin(YawRad);

		for (int32 PitchIndex = 0; PitchIndex < NumPitch; ++PitchIndex)
		{
			const double Pitch = NumPitch > 1 ? FMath::Lerp(PitchLow, PitchHigh, double(PitchIndex) / double(NumPitch - 1)) : AxisPitch;
			const double PitchRad = FMath::DegreesToRadians(Pitch);
			const FVector Dir = DirH * FMath::Cos(PitchRad) + FVector::UpVector * FMath::Sin(PitchRad);

			FTraversalHit Hit;
			if (!World.Trace(Origin, Origin + Dir * T.AnchorReach, T.AnchorTraceRadius, Hit))
			{
				continue;
			}

			const FVector ToHit = Hit.Location - Origin;
			const double Dist = ToHit.Size();
			if (Dist < T.SwingRopeMinLength)
			{
				continue;
			}
			const double Elevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(ToHit.Z / Dist, -1.0, 1.0)));
			if (Elevation < T.AnchorMinElevation)
			{
				continue;
			}
			const FVector Pivot = Hit.Location + Hit.Normal * T.AnchorSurfaceOffset;
			if (FVector::Dist(Pivot, Body.Position) > T.SwingRopeMaxLength)
			{
				continue;
			}

			double ElevationScore = 1.0;
			if (Elevation < T.AnchorIdealElevationMin)
			{
				ElevationScore = (Elevation - T.AnchorMinElevation) / FMath::Max(1.0, T.AnchorIdealElevationMin - T.AnchorMinElevation);
			}
			else if (Elevation > T.AnchorIdealElevationMax)
			{
				ElevationScore = (90.0 - Elevation) / FMath::Max(1.0, 90.0 - T.AnchorIdealElevationMax);
			}
			const double DistanceScore = FMath::Max(0.0, 1.0 - FMath::Abs(Dist - T.AnchorIdealDistance) / FMath::Max(1.0, T.AnchorReach));
			const double AlignmentScore = FMath::Max(0.0, FVector::DotProduct(TSim::Horizontal(ToHit).GetSafeNormal(), ForwardH));

			const double Facing = FMath::Max(0.0, -FVector::DotProduct(TSim::Horizontal(Hit.Normal).GetSafeNormal(), ForwardH));

			double Score = T.AnchorWeightElevation * ElevationScore + T.AnchorWeightDistance * DistanceScore + T.AnchorWeightAlignment * AlignmentScore
				- T.AnchorFacingPenalty * Facing;
			if (Hit.bAnchorProp)
			{
				Score += T.AnchorPropBonus;
			}
			if (PreviewAnchor.bValid && FVector::Dist(Pivot, PreviewAnchor.Pivot) < T.AnchorHysteresisRadius)
			{
				Score += T.AnchorHysteresis;
			}

			if (!Best.bValid || Score > Best.Score)
			{
				Best.bValid = true;
				Best.bSky = false;
				Best.Pivot = Pivot;
				Best.SurfacePoint = Hit.Location;
				Best.Normal = Hit.Normal;
				Best.Score = Score;
			}
		}
	}
	return Best;
}

FTraversalAnchor FTraversalSim::MakeSkyAnchor(const FTraversalBody& Body, const FTraversalInput& Input) const
{
	const FTraversalTuning& T = *Tuning;
	FVector ForwardH = TSim::Horizontal(Input.CameraForward).GetSafeNormal();
	if (ForwardH.IsNearlyZero())
	{
		ForwardH = TSim::Horizontal(Body.Velocity).GetSafeNormal(UE_SMALL_NUMBER, LastFacing);
	}
	FTraversalAnchor Anchor;
	Anchor.bValid = true;
	Anchor.bSky = true;
	Anchor.Pivot = Body.Position + ForwardH * T.SkyAnchorForward + FVector::UpVector * T.SkyAnchorHeight;
	Anchor.SurfacePoint = Anchor.Pivot;
	Anchor.Normal = -FVector::UpVector;
	return Anchor;
}

// ------------------------------------------------------------------ swing

void FTraversalSim::StartSwing(const FTraversalAnchor& Anchor, const FTraversalBody& Body, const FTraversalInput& Input)
{
	const FTraversalTuning& T = *Tuning;
	const int32 OldTier = GetSpeedTier();
	ChainCount = TimeSinceRelease <= T.SwingChainWindow ? ChainCount + 1 : 1;

	// Travel direction: where we're already going, or where the camera looks when we're slow.
	FVector Travel = TSim::Horizontal(Body.Velocity);
	if (Travel.Size() < T.SwingMinSpeed * 0.5)
	{
		Travel = TSim::Horizontal(Input.CameraForward);
	}
	if (Travel.SizeSquared() < UE_KINDA_SMALL_NUMBER)
	{
		Travel = TSim::Horizontal(Anchor.Pivot - Body.Position);
	}
	SwingForward = Travel.GetSafeNormal(UE_SMALL_NUMBER, LastFacing);

	// The web sticks to the wall, but the swing pivots above our line of travel, so arcs carry us
	// along the street instead of across it and into the buildings on the other side.
	if (!Anchor.bSky)
	{
		SkyAnchorsUsed = 0;
	}
	SwingAnchor = Anchor;
	const FVector ToPivotH = TSim::Horizontal(Anchor.Pivot - Body.Position);
	const FVector Lateral = ToPivotH - SwingForward * FVector::DotProduct(ToPivotH, SwingForward);
	SwingAnchor.Pivot = Anchor.Pivot - Lateral * T.SwingPlaneAlign;
	RopeLength = FMath::Clamp(FVector::Dist(Body.Position, SwingAnchor.Pivot), T.SwingRopeMinLength, T.SwingRopeMaxLength);

	bAutoCatchSwing = false;
	NoAnchorTimer = 0.0;
	SetState(ETraversalState::Swing);
	Events.bSwingStarted = true;
	++Streak.Swings;
	if (GetSpeedTier() > OldTier)
	{
		Events.bTierUp = true;
	}
}

void FTraversalSim::Release(FTraversalBody& Body, bool bAllowBoost, bool bJumpOff)
{
	const FTraversalTuning& T = *Tuning;
	const double Angle = GetSwingAngle(Body.Position);
	if (bAllowBoost && Angle >= T.ReleaseWindowMinAngle && Angle <= T.ReleaseWindowMaxAngle)
	{
		const int32 OldTier = GetSpeedTier();
		Body.Velocity += Body.Velocity.GetSafeNormal() * T.ReleaseBoostSpeed + FVector::UpVector * T.ReleaseBoostUp;
		ChainCount += T.PerfectReleaseChainBonus;
		Events.bPerfectRelease = true;
		++Streak.PerfectReleases;
		if (GetSpeedTier() > OldTier)
		{
			Events.bTierUp = true;
		}
	}
	if (bJumpOff)
	{
		Body.Velocity.Z = FMath::Max(Body.Velocity.Z, 0.0) + T.ReleaseJumpUp;
	}
	TimeSinceRelease = 0.0;
	bAutoCatchSwing = false;
	Events.bReleased = true;
	SetState(ETraversalState::Air);
}

void FTraversalSim::TickSwing(double Dt, const FTraversalInput& Input, FTraversalBody& Body, const ITraversalWorld& World)
{
	const FTraversalTuning& T = *Tuning;

	if (JumpBufferTimer > 0.0)
	{
		JumpBufferTimer = 0.0;
		Release(Body, true, true);
		IntegrateAir(Dt, Input, Body);
		return;
	}
	if (Input.bGlideTogglePressed && T.bGlideUnlocked)
	{
		Release(Body, true, false);
		SetState(ETraversalState::Glide);
		IntegrateAir(Dt, Input, Body);
		return;
	}
	if (!Input.bTraversalHeld && !bAutoCatchSwing)
	{
		Release(Body, true, false);
		IntegrateAir(Dt, Input, Body);
		return;
	}

	const FVector Pivot = SwingAnchor.Pivot;
	const double Cap = GetSpeedCap();
	FVector V = Body.Velocity;
	V.Z -= T.Gravity * T.SwingGravityScale * Dt;
	V += TSim::Horizontal(Input.Move) * (T.SwingAirControl * Dt);

	const FVector R = (Body.Position - Pivot).GetSafeNormal(UE_SMALL_NUMBER, -FVector::UpVector);

	// Designer assists: a push through the bottom of the arc, then damping, a speed floor and a soft cap.
	FVector Tangent = V - R * FVector::DotProduct(V, R);
	if (FMath::Abs(GetSwingAngle(Body.Position)) <= T.SwingLowPointAngle && Tangent.Size() < Cap)
	{
		V += Tangent.GetSafeNormal() * (T.SwingLowPointBoost * Dt);
	}
	V *= FMath::Max(0.0, 1.0 - T.SwingDamping * Dt);

	Tangent = V - R * FVector::DotProduct(V, R);
	const double TangentSpeed = Tangent.Size();
	if (TangentSpeed < T.SwingMinSpeed)
	{
		const FVector Dir = TangentSpeed > 0.1 ? Tangent / TangentSpeed : FVector::VectorPlaneProject(SwingForward, R).GetSafeNormal();
		V = R * FVector::DotProduct(V, R) + Dir * T.SwingMinSpeed;
	}

	const double Speed = V.Size();
	if (Speed > Cap)
	{
		V *= FMath::Max(Cap / Speed, 1.0 - T.SpeedOverCapDrag * Dt);
	}

	// Reel in so the bottom of the arc clears whatever is under the anchor.
	FTraversalHit Ground;
	const double DownReach = RopeLength + Body.HalfHeight + T.SwingGroundClearance;
	if (World.Trace(Pivot, Pivot - FVector::UpVector * DownReach, 0.0, Ground))
	{
		const double MaxLength = Pivot.Z - (Ground.Location.Z + T.SwingGroundClearance + Body.HalfHeight);
		if (RopeLength > MaxLength)
		{
			RopeLength = FMath::Max(MaxLength, RopeLength - T.SwingRopeReelSpeed * Dt);
		}
	}
	RopeLength = FMath::Max(RopeLength, T.SwingRopeMinLength);

	// Integrate, then enforce the rope: it can go slack but never stretch.
	FVector NewPos = Body.Position + V * Dt;
	const FVector D = NewPos - Pivot;
	const double DLen = D.Size();
	if (DLen > RopeLength)
	{
		const FVector N = D / DLen;
		NewPos = Pivot + N * RopeLength;
		const double Radial = FVector::DotProduct(V, N);
		if (Radial > 0.0)
		{
			V -= N * Radial;
		}
	}
	Body.Velocity = V;
	Body.Move = NewPos - Body.Position;

	// Let go by ourselves at the top of the forward arc, or as soon as we start swinging back.
	const double NewAngle = GetSwingAngle(NewPos);
	const bool bPastApex = NewAngle > T.SwingApexMinAngle && V.Z <= 0.0;
	const bool bSwingingBack = NewAngle > 0.0 && FVector::DotProduct(TSim::Horizontal(V), SwingForward) < 0.0;
	const bool bAutoCatchDone = bAutoCatchSwing && NewAngle > T.SwingApexMinAngle;
	if (NewAngle >= T.SwingAutoReleaseAngle || bPastApex || bSwingingBack || bAutoCatchDone)
	{
		Release(Body, false, false);
	}
}

// ------------------------------------------------------------------ zip and point-launch

bool FTraversalSim::TryStartZip(const FTraversalBody& Body, const FTraversalInput& Input, const ITraversalWorld& World)
{
	const FTraversalTuning& T = *Tuning;
	const FVector Eye = Body.Position + FVector::UpVector * (Body.HalfHeight * 0.6);
	const FVector Dir = Input.CameraForward.GetSafeNormal(UE_SMALL_NUMBER, LastFacing);

	bZipToPerch = false;
	bZipToWall = false;
	bLaunchQueued = false;
	FVector Target = Eye + Dir * T.ZipNoTargetDistance;

	FTraversalHit Hit;
	if (World.Trace(Eye, Eye + Dir * T.ZipRange, 0.0, Hit))
	{
		if (Hit.Normal.Z > 0.7)
		{
			// A rooftop or ledge top: perch on it.
			Target = Hit.Location + FVector::UpVector * (Body.HalfHeight + T.ZipPerchClearance);
			bZipToPerch = true;
		}
		else if (FMath::Abs(Hit.Normal.Z) < 0.3)
		{
			// A wall: perch on its ledge if the hit is near the top, otherwise stick to the wall.
			const FVector WallN = TSim::Horizontal(Hit.Normal).GetSafeNormal();
			FVector Top;
			if (FindLedge(Hit.Location + WallN * Body.Radius, WallN, Body, World, Top))
			{
				Target = Top + FVector::UpVector * (Body.HalfHeight + T.ZipPerchClearance);
				bZipToPerch = true;
			}
			else
			{
				Target = Hit.Location + WallN * (Body.Radius + 0.05);
				ZipWallNormal = WallN;
				bZipToWall = true;
			}
		}
		else
		{
			Target = Hit.Location + Hit.Normal * (Body.Radius + Body.HalfHeight);
		}
	}

	const FVector Delta = Target - Body.Position;
	const double Dist = Delta.Size();
	if (Dist < 1.0)
	{
		return false;
	}

	ZipStart = Body.Position;
	ZipTarget = Target;
	ZipDirection = Delta / Dist;
	ZipDuration = FMath::Clamp(Dist / FMath::Max(1.0, T.ZipSpeed), T.ZipMinDuration, T.ZipMaxDuration);
	ZipElapsed = 0.0;
	ZipPhase = EZipPhase::Travel;
	ZipCooldownTimer = T.ZipCooldown;
	Events.bZip = true;
	SetState(ETraversalState::Zip);
	return true;
}

void FTraversalSim::TickZip(double Dt, const FTraversalInput& Input, FTraversalBody& Body)
{
	const FTraversalTuning& T = *Tuning;

	if (ZipPhase == EZipPhase::Travel)
	{
		ZipElapsed += Dt;
		// A jump pressed just before arriving at a perch queues the point-launch.
		if (JumpBufferTimer > 0.0 && bZipToPerch && ZipDuration - ZipElapsed <= T.LaunchWindow)
		{
			bLaunchQueued = true;
			JumpBufferTimer = 0.0;
		}

		// Ease out, with the vertical part leading when going up (clears ledge lips) and
		// trailing when going down (clears the wall we're leaving).
		const double Alpha = FMath::Clamp(ZipElapsed / ZipDuration, 0.0, 1.0);
		const double Remain = 1.0 - Alpha;
		const double Lead = 1.0 - Remain * Remain * Remain;
		const double Lag = 1.0 - Remain * Remain;
		const bool bRising = ZipTarget.Z > ZipStart.Z;
		const double HorizontalAlpha = bRising ? Lag : Lead;
		const double VerticalAlpha = bRising ? Lead : Lag;
		const FVector NewPos(
			FMath::Lerp(ZipStart.X, ZipTarget.X, HorizontalAlpha),
			FMath::Lerp(ZipStart.Y, ZipTarget.Y, HorizontalAlpha),
			FMath::Lerp(ZipStart.Z, ZipTarget.Z, VerticalAlpha));
		Body.Move = NewPos - Body.Position;
		Body.Velocity = Body.Move / Dt;

		if (Alpha >= 1.0)
		{
			if (bZipToWall)
			{
				WallNormal = ZipWallNormal;
				Body.Velocity = FVector::ZeroVector;
				Events.bWallAttach = true;
				SetState(ETraversalState::WallCling);
			}
			else if (bZipToPerch)
			{
				ZipPhase = EZipPhase::Perch;
				ZipElapsed = 0.0;
				Body.Velocity = FVector::ZeroVector;
			}
			else
			{
				Body.Velocity = ZipDirection * T.ZipExitSpeed;
				SetState(ETraversalState::Air);
			}
		}
		return;
	}

	// Perched at the end of a zip: this is the point-launch window.
	ZipElapsed += Dt;
	Body.Velocity = FVector::ZeroVector;
	Body.Move = FVector::ZeroVector;
	const FVector Forward = TSim::Horizontal(ZipDirection).GetSafeNormal(UE_SMALL_NUMBER, LastFacing);

	if (bLaunchQueued || JumpBufferTimer > 0.0)
	{
		bLaunchQueued = false;
		JumpBufferTimer = 0.0;
		Body.Velocity = Forward * T.LaunchForwardSpeed + FVector::UpVector * T.LaunchUpSpeed;
		Body.Move = Body.Velocity * Dt;
		Events.bLaunch = true;
		SetState(ETraversalState::Launch);
		return;
	}
	if (ZipElapsed >= T.LaunchWindow || Input.bTraversalHeld)
	{
		// Window missed: step onto the perch and let walking (or the next swing) take over.
		Body.Velocity = Forward - FVector::UpVector;
		Body.Move = Body.Velocity * Dt;
		SetState(ETraversalState::Air);
	}
}

// ------------------------------------------------------------------ walls and ledges

bool FTraversalSim::ProbeWall(const FVector& From, const FVector& Normal, const FTraversalBody& Body, const ITraversalWorld& World, FTraversalHit& OutHit) const
{
	const double Reach = Body.Radius + Tuning->WallStickDistance;
	return World.Trace(From, From - Normal * Reach, 0.0, OutHit) && FMath::Abs(OutHit.Normal.Z) < 0.3;
}

bool FTraversalSim::FindLedge(const FVector& From, const FVector& Normal, const FTraversalBody& Body, const ITraversalWorld& World, FVector& OutTop) const
{
	const FTraversalTuning& T = *Tuning;
	const FVector Above = From + FVector::UpVector * T.LedgeSearchHeight;
	const FVector Inward = -Normal * (Body.Radius + 0.6);

	// If the wall continues above the search height there is no ledge within reach.
	FTraversalHit Blocked;
	if (World.Trace(Above, Above + Inward, 0.0, Blocked))
	{
		return false;
	}

	// Look down onto the top surface just behind the wall face.
	FTraversalHit Top;
	const FVector Start = Above + Inward;
	const FVector End = From + Inward - FVector::UpVector * Body.HalfHeight;
	if (!World.Trace(Start, End, 0.0, Top) || Top.Normal.Z < 0.7)
	{
		return false;
	}

	// Room to stand up there.
	FTraversalHit Headroom;
	const FVector Feet = Top.Location + FVector::UpVector * 0.1;
	if (World.Trace(Feet, Feet + FVector::UpVector * (Body.HalfHeight * 2.0), 0.0, Headroom))
	{
		return false;
	}

	OutTop = Top.Location;
	return true;
}

void FTraversalSim::TickWall(double Dt, const FTraversalInput& Input, FTraversalBody& Body, const ITraversalWorld& World)
{
	const FTraversalTuning& T = *Tuning;

	FTraversalHit WallHit;
	if (!ProbeWall(Body.Position, WallNormal, Body, World, WallHit))
	{
		// The wall ended. Climbing: mantle onto the top if there is one. Otherwise let go.
		FVector Top;
		if (Body.Velocity.Z >= 0.0 && FindLedge(Body.Position, WallNormal, Body, World, Top))
		{
			StartVault(Body, Top, WallNormal);
			TickVault(Dt, Body);
		}
		else
		{
			SetState(ETraversalState::Air);
			IntegrateAir(Dt, Input, Body);
		}
		return;
	}
	WallNormal = TSim::Horizontal(WallHit.Normal).GetSafeNormal(UE_SMALL_NUMBER, WallNormal);

	if (JumpBufferTimer > 0.0)
	{
		JumpBufferTimer = 0.0;
		Body.Velocity = WallNormal * T.WallJumpOutSpeed + FVector::UpVector * T.WallJumpUpSpeed;
		Body.Move = Body.Velocity * Dt;
		SetState(ETraversalState::Air);
		return;
	}

	SetState(Input.bTraversalHeld ? ETraversalState::WallRun : ETraversalState::WallCling);

	// Stick input maps onto the wall: pushing into the wall climbs, sideways runs along it.
	const FVector WallRight = FVector::CrossProduct(WallNormal, FVector::UpVector).GetSafeNormal();
	const FVector Move = TSim::Horizontal(Input.Move);
	const double InUp = FVector::DotProduct(Move, -WallNormal);
	const double InRight = FVector::DotProduct(Move, WallRight);

	FVector Desired;
	if (State == ETraversalState::WallRun)
	{
		const bool bHasInput = FMath::Abs(InUp) > 0.1 || FMath::Abs(InRight) > 0.1;
		const FVector Dir = FVector::UpVector * (bHasInput ? InUp : 1.0) + WallRight * InRight;
		Desired = Dir.GetSafeNormal() * T.WallRunSpeed;
	}
	else
	{
		Desired = (FVector::UpVector * InUp + WallRight * InRight) * T.WallCrawlSpeed;
		if (Desired.Size() > T.WallCrawlSpeed)
		{
			Desired = Desired.GetSafeNormal() * T.WallCrawlSpeed;
		}
	}

	// At the top of the wall the head probe comes back empty: mantle over.
	FTraversalHit HeadHit;
	if (Desired.Z > 0.0 && !ProbeWall(Body.Position + FVector::UpVector * T.LedgeProbeHeight, WallNormal, Body, World, HeadHit))
	{
		FVector Top;
		if (FindLedge(Body.Position, WallNormal, Body, World, Top))
		{
			StartVault(Body, Top, WallNormal);
			TickVault(Dt, Body);
			return;
		}
	}

	Body.Velocity = Desired;
	const double Gap = FMath::Max(0.0, WallHit.Distance - Body.Radius - 0.02);
	Body.Move = Desired * Dt - WallNormal * Gap;
}

void FTraversalSim::StartVault(const FTraversalBody& Body, const FVector& LedgeTop, const FVector& FromNormal)
{
	VaultStart = Body.Position;
	VaultTarget = LedgeTop + FVector::UpVector * (Body.HalfHeight + 0.05);
	VaultNormal = FromNormal;
	VaultElapsed = 0.0;
	Events.bVault = true;
	SetState(ETraversalState::Vault);
}

void FTraversalSim::TickVault(double Dt, FTraversalBody& Body)
{
	const FTraversalTuning& T = *Tuning;
	VaultElapsed += Dt;
	const double Alpha = FMath::Clamp(VaultElapsed / T.VaultDuration, 0.0, 1.0);

	// Rise clear of the lip first, then move over it.
	const double Rise = FMath::Clamp(T.VaultRiseFraction, 0.05, 0.95);
	const double UpX = FMath::Clamp(Alpha / Rise, 0.0, 1.0);
	const double ForwardX = FMath::Clamp((Alpha - Rise) / (1.0 - Rise), 0.0, 1.0);
	const double UpAlpha = UpX * UpX * (3.0 - 2.0 * UpX);
	const double ForwardAlpha = ForwardX * ForwardX * (3.0 - 2.0 * ForwardX);
	const FVector NewPos(
		FMath::Lerp(VaultStart.X, VaultTarget.X, ForwardAlpha),
		FMath::Lerp(VaultStart.Y, VaultTarget.Y, ForwardAlpha),
		FMath::Lerp(VaultStart.Z, VaultTarget.Z, UpAlpha));
	Body.Move = NewPos - Body.Position;
	Body.Velocity = Body.Move / Dt;

	if (Alpha >= 1.0)
	{
		Body.Velocity = -VaultNormal * T.VaultExitSpeed;
		SetState(ETraversalState::Air);
	}
}

// ------------------------------------------------------------------ glide (stub)

void FTraversalSim::TickGlide(double Dt, const FTraversalInput& Input, FTraversalBody& Body, const ITraversalWorld& World)
{
	const FTraversalTuning& T = *Tuning;

	if ((Input.bGlideTogglePressed && StateTime > 0.0) || !T.bGlideUnlocked)
	{
		SetState(ETraversalState::Air);
		IntegrateAir(Dt, Input, Body);
		return;
	}
	// Seamless hand-off to swinging and zipping.
	if (CanAttachSwing(Input, Body) && PreviewAnchor.bValid)
	{
		StartSwing(PreviewAnchor, Body, Input);
		TickSwing(Dt, Input, Body, World);
		return;
	}
	if (JumpBufferTimer > 0.0 && ZipCooldownTimer <= 0.0 && TryStartZip(Body, Input, World))
	{
		JumpBufferTimer = 0.0;
		TickZip(Dt, Input, Body);
		return;
	}

	FVector V = Body.Velocity;
	const FVector Hor = TSim::Horizontal(V);
	const double HorSpeed = Hor.Size();
	FVector Heading = HorSpeed > 0.5 ? Hor / HorSpeed : TSim::Horizontal(Input.CameraForward).GetSafeNormal(UE_SMALL_NUMBER, LastFacing);
	const FVector MoveH = TSim::Horizontal(Input.Move);
	const FVector Wanted = MoveH.SizeSquared() > 0.01 ? MoveH.GetSafeNormal() : Heading;

	// Bank toward the stick direction at a limited turn rate.
	const double Cross = Heading.X * Wanted.Y - Heading.Y * Wanted.X;
	const double MaxTurn = FMath::DegreesToRadians(T.GlideTurnRate) * Dt;
	const double Turn = FMath::Clamp(FMath::Atan2(Cross, FVector::DotProduct(Heading, Wanted)), -MaxTurn, MaxTurn);
	Heading = TSim::RotateZ(Heading, Turn);

	const double NewSpeed = HorSpeed + FMath::Clamp(T.GlideSpeed - HorSpeed, -T.GlideAccel * Dt, T.GlideAccel * Dt);
	V.X = Heading.X * NewSpeed;
	V.Y = Heading.Y * NewSpeed;
	if (V.Z > 0.0)
	{
		V.Z -= T.Gravity * T.AirGravityScale * Dt;
	}
	else if (V.Z < -T.GlideSinkRate)
	{
		V.Z = FMath::Min(-T.GlideSinkRate, V.Z + T.GlideAccel * Dt);
	}
	else
	{
		V.Z = FMath::Max(-T.GlideSinkRate, V.Z - T.Gravity * T.GlideGravityScale * Dt);
	}

	Body.Velocity = V;
	Body.Move = V * Dt;
}

// ------------------------------------------------------------------ streaks (the "2 km without touching down" measure)

void FTraversalSim::UpdateStreak(const FTraversalBody& Body, double Dt)
{
	if (State == ETraversalState::Ground)
	{
		return;
	}
	const double Speed = Body.Velocity.Size();
	Streak.Distance += TSim::Horizontal(Body.Move).Size();
	Streak.Time += Dt;
	Streak.MaxSpeed = FMath::Max(Streak.MaxSpeed, Speed);
	Streak.MaxTier = FMath::Max(Streak.MaxTier, GetSpeedTier());
	if (Streak.Time > 1.0)
	{
		Streak.MinSpeed = bStreakMinSpeedSet ? FMath::Min(Streak.MinSpeed, Speed) : Speed;
		bStreakMinSpeedSet = true;
	}
}

void FTraversalSim::EndStreak()
{
	if (Streak.Time <= 0.0)
	{
		return;
	}
	LastStreak = Streak;
	if (Streak.Distance > BestStreak.Distance)
	{
		BestStreak = Streak;
	}
	Events.bStreakEnded = true;
	Streak = FTraversalStreak();
	bStreakMinSpeedSet = false;
}
