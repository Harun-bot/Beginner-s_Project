#include "Traversal/HeroMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/PackageName.h"
#include "Traversal/TraversalTuningAsset.h"
#include "WebOfTheCity.h"

namespace HeroMovement
{
	// The traversal core works in metres; Unreal works in centimetres.
	constexpr double MetersToUU = 100.0;
	constexpr double UUToMeters = 0.01;

	/** The sim's view of the world, answered with Unreal traces on the Visibility channel. */
	class FWorldAdapter : public ITraversalWorld
	{
	public:
		FWorldAdapter(const UWorld* InWorld, const AActor* InIgnore)
			: World(InWorld)
			, Ignore(InIgnore)
		{
		}

		virtual bool Trace(const FVector& Start, const FVector& End, double Radius, FTraversalHit& OutHit) const override
		{
			static const FName AnchorTag(TEXT("WebAnchor"));

			FCollisionQueryParams Params(SCENE_QUERY_STAT(TraversalTrace), false, Ignore);
			FHitResult Hit;
			const FVector StartUU = Start * MetersToUU;
			const FVector EndUU = End * MetersToUU;
			const bool bHit = Radius > 0.0
				? World->SweepSingleByChannel(Hit, StartUU, EndUU, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(float(Radius * MetersToUU)), Params)
				: World->LineTraceSingleByChannel(Hit, StartUU, EndUU, ECC_Visibility, Params);
			if (!bHit)
			{
				return false;
			}

			OutHit.Location = FVector(Hit.ImpactPoint) * UUToMeters;
			OutHit.Normal = Hit.bStartPenetrating ? FVector(Hit.Normal) : FVector(Hit.ImpactNormal);
			OutHit.Distance = double(Hit.Distance) * UUToMeters;
			const AActor* HitActor = Hit.GetActor();
			OutHit.bAnchorProp = HitActor && HitActor->ActorHasTag(AnchorTag);
			return true;
		}

	private:
		const UWorld* World;
		const AActor* Ignore;
	};
}

UHeroMovementComponent::UHeroMovementComponent()
{
	DefaultTuningAsset = TSoftObjectPtr<UTraversalTuningAsset>(FSoftObjectPath(TEXT("/Game/WebOfTheCity/Traversal/Data/DA_TraversalTuning.DA_TraversalTuning")));

	// Ground feel; the traversal numbers live in the tuning struct.
	bOrientRotationToMovement = true;
	RotationRate = FRotator(0.0, 720.0, 0.0);
	MaxAcceleration = 4000.f;
	BrakingDecelerationWalking = 3000.f;

	Sim.SetTuning(&DefaultTuning);
}

void UHeroMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!TuningAsset && !DefaultTuningAsset.IsNull())
	{
		const FString PackageName = DefaultTuningAsset.ToSoftObjectPath().GetLongPackageName();
		if (FPackageName::DoesPackageExist(PackageName))
		{
			TuningAsset = DefaultTuningAsset.LoadSynchronous();
		}
	}
	UE_LOG(LogWebOfTheCity, Log, TEXT("Traversal tuning: %s"), TuningAsset ? *TuningAsset->GetPathName() : TEXT("built-in defaults (no DA_TraversalTuning found)"));
	Sim.SetTuning(&GetTuning());
}

const FTraversalTuning& UHeroMovementComponent::GetTuning() const
{
	return TuningAsset ? TuningAsset->Tuning : DefaultTuning;
}

FTraversalTuning& UHeroMovementComponent::GetMutableTuning()
{
	return TuningAsset ? TuningAsset->Tuning : DefaultTuning;
}

UTraversalTuningAsset* UHeroMovementComponent::GetTuningAsset() const
{
	return TuningAsset;
}

void UHeroMovementComponent::SetTraversalInput(const FTraversalInput& Input)
{
	// Keep presses that haven't been used yet.
	const bool bJump = PendingInput.bJumpPressed || Input.bJumpPressed;
	const bool bGlide = PendingInput.bGlideTogglePressed || Input.bGlideTogglePressed;
	PendingInput = Input;
	PendingInput.bJumpPressed = bJump;
	PendingInput.bGlideTogglePressed = bGlide;
}

void UHeroMovementComponent::QueueJumpPressed()
{
	PendingInput.bJumpPressed = true;
}

void UHeroMovementComponent::QueueGlideToggle()
{
	PendingInput.bGlideTogglePressed = true;
}

FTraversalEvents UHeroMovementComponent::ConsumeTraversalEvents()
{
	return Sim.ConsumeEvents();
}

void UHeroMovementComponent::ResetTraversal()
{
	Sim.ResetToAir();
	Velocity = FVector::ZeroVector;
	TGuardValue<bool> Guard(bSyncingMode, true);
	SetMovementMode(MOVE_Falling);
}

float UHeroMovementComponent::GetMaxSpeed() const
{
	if (IsMovingOnGround())
	{
		const FTraversalTuning& T = GetTuning();
		return float((PendingInput.bTraversalHeld ? T.GroundSprintSpeed : T.GroundRunSpeed) * HeroMovement::MetersToUU);
	}
	return Super::GetMaxSpeed();
}

void UHeroMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	// Presses apply to exactly one frame.
	FrameInput = PendingInput;
	PendingInput.bJumpPressed = false;
	PendingInput.bGlideTogglePressed = false;

	Sim.SetTuning(&GetTuning());
	JumpZVelocity = float(GetTuning().GroundJumpSpeed * HeroMovement::MetersToUU);

	if (IsMovingOnGround() && CharacterOwner)
	{
		const HeroMovement::FWorldAdapter World(GetWorld(), CharacterOwner);
		FTraversalBody Body = MakeBody();
		Sim.TickGround(DeltaSeconds, FrameInput, Body, World);
		SyncMovementModeFromSim();
	}
}

void UHeroMovementComponent::PhysFalling(float DeltaTime, int32 Iterations)
{
	PhysTraversal(DeltaTime, Iterations);
}

void UHeroMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	PhysTraversal(DeltaTime, Iterations);
}

void UHeroMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	if (bSyncingMode)
	{
		return;
	}

	// Unreal changed the mode on its own: landing, jumping, or walking off a ledge.
	if (IsMovingOnGround())
	{
		Sim.NotifyLanded();
	}
	else if (MovementMode == MOVE_Falling)
	{
		const bool bJumped = CharacterOwner && CharacterOwner->bPressedJump;
		Sim.NotifyLeftGround(bJumped);
		if (bJumped)
		{
			// The press was a ground jump; don't let it also fire a web-zip.
			PendingInput.bJumpPressed = false;
			FrameInput.bJumpPressed = false;
		}
	}
}

FTraversalBody UHeroMovementComponent::MakeBody() const
{
	FTraversalBody Body;
	Body.Position = UpdatedComponent->GetComponentLocation() * HeroMovement::UUToMeters;
	Body.Velocity = Velocity * HeroMovement::UUToMeters;
	if (CharacterOwner && CharacterOwner->GetCapsuleComponent())
	{
		Body.Radius = double(CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius()) * HeroMovement::UUToMeters;
		Body.HalfHeight = double(CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) * HeroMovement::UUToMeters;
	}
	return Body;
}

void UHeroMovementComponent::PhysTraversal(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME || !CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	const HeroMovement::FWorldAdapter World(GetWorld(), CharacterOwner);
	FTraversalBody Body = MakeBody();
	Sim.Tick(DeltaTime, FrameInput, Body, World);
	// Presses are used by the first physics step of the frame only.
	FrameInput.bJumpPressed = false;
	FrameInput.bGlideTogglePressed = false;

	const FVector Delta = Body.Move * HeroMovement::MetersToUU;
	const FVector Facing = Sim.GetFacing(Body);
	const FQuat Rotation = Facing.IsNearlyZero() ? UpdatedComponent->GetComponentQuat() : Facing.ToOrientationQuat();

	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, Rotation, true, Hit);

	if (Hit.IsValidBlockingHit())
	{
		FTraversalHit TraversalHit;
		TraversalHit.Location = FVector(Hit.ImpactPoint) * HeroMovement::UUToMeters;
		TraversalHit.Normal = Hit.Normal;
		TraversalHit.Distance = double(Hit.Distance) * HeroMovement::UUToMeters;
		Body.Position = UpdatedComponent->GetComponentLocation() * HeroMovement::UUToMeters;

		switch (Sim.OnBlocked(TraversalHit, IsWalkable(Hit), Body))
		{
		case ETraversalBlockResponse::Land:
		{
			Velocity = Body.Velocity * HeroMovement::MetersToUU;
			// ProcessLanded only switches to walking from Falling.
			if (MovementMode != MOVE_Falling)
			{
				TGuardValue<bool> Guard(bSyncingMode, true);
				SetMovementMode(MOVE_Falling);
			}
			ProcessLanded(Hit, DeltaTime * (1.f - Hit.Time), Iterations);
			return;
		}
		case ETraversalBlockResponse::Attach:
			break;
		case ETraversalBlockResponse::Slide:
			SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
			break;
		}
	}

	Velocity = Body.Velocity * HeroMovement::MetersToUU;
	SyncMovementModeFromSim();
}

void UHeroMovementComponent::SyncMovementModeFromSim()
{
	TGuardValue<bool> Guard(bSyncingMode, true);
	const ETraversalState State = Sim.GetState();
	switch (State)
	{
	case ETraversalState::Ground:
		// Walking only ever starts from a landing, which Unreal handles.
		break;
	case ETraversalState::Air:
	case ETraversalState::Launch:
		if (MovementMode != MOVE_Falling)
		{
			SetMovementMode(MOVE_Falling);
		}
		break;
	default:
		if (MovementMode != MOVE_Custom || CustomMovementMode != uint8(State))
		{
			SetMovementMode(MOVE_Custom, uint8(State));
		}
		break;
	}
}
