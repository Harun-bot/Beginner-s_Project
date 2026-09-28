#include "Traversal/HeroCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Traversal/HeroDebugHUD.h"
#include "Traversal/HeroMovementComponent.h"
#include "Traversal/TraversalSim.h"
#include "UObject/ConstructorHelpers.h"
#include "WebOfTheCity.h"

namespace HeroCharacter
{
	constexpr double MetersToUU = 100.0;
	constexpr double UUToMeters = 0.01;
	constexpr int32 MaxRecentEvents = 5;
}

AHeroCharacter::AHeroCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UHeroMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	// Same capsule as the Unreal mannequin, so the Third Person placeholder mesh fits.
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.f;
	CameraBoom->SocketOffset = FVector(0.0, 0.0, 60.0);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 10.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(80.f);

	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -96.0), FRotator(0.0, -90.0, 0.0));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	auto MakeMarker = [this](const TCHAR* ComponentName, UStaticMesh* ShapeMesh)
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
		Component->SetupAttachment(RootComponent);
		Component->SetUsingAbsoluteLocation(true);
		Component->SetUsingAbsoluteRotation(true);
		Component->SetUsingAbsoluteScale(true);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->SetHiddenInGame(true);
		Component->SetStaticMesh(ShapeMesh);
		return Component;
	};
	WebLine = MakeMarker(TEXT("WebLine"), CylinderMesh.Object);
	AnchorMarker = MakeMarker(TEXT("AnchorMarker"), SphereMesh.Object);
	AnchorMarker->SetWorldScale3D(FVector(0.4));
}

UHeroMovementComponent* AHeroCharacter::GetHeroMovement() const
{
	return Cast<UHeroMovementComponent>(GetCharacterMovement());
}

void AHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	// No mannequin assigned yet (no BP_Hero): show the capsule so there is something to look at.
	if (!GetMesh()->GetSkeletalMeshAsset())
	{
		GetCapsuleComponent()->SetHiddenInGame(false);
	}
	if (CameraBoom)
	{
		CurrentArmLength = CameraBoom->TargetArmLength;
	}
}

// ------------------------------------------------------------------ input

void AHeroCharacter::BuildInput()
{
	if (DefaultMappingContext)
	{
		return;
	}

	auto MakeAction = [this](const TCHAR* ActionName, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, FName(ActionName));
		Action->ValueType = Type;
		return Action;
	};
	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookMouseAction = MakeAction(TEXT("IA_LookMouse"), EInputActionValueType::Axis2D);
	LookStickAction = MakeAction(TEXT("IA_LookStick"), EInputActionValueType::Axis2D);
	TraversalAction = MakeAction(TEXT("IA_Traversal"), EInputActionValueType::Boolean);
	JumpAction = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	GlideAction = MakeAction(TEXT("IA_Glide"), EInputActionValueType::Boolean);
	FastTravelAction = MakeAction(TEXT("IA_FastTravel"), EInputActionValueType::Boolean);
	RespawnAction = MakeAction(TEXT("IA_Respawn"), EInputActionValueType::Boolean);
	ToggleStatsAction = MakeAction(TEXT("IA_DebugStats"), EInputActionValueType::Boolean);
	TogglePanelAction = MakeAction(TEXT("IA_DebugPanel"), EInputActionValueType::Boolean);
	PanelUpAction = MakeAction(TEXT("IA_PanelUp"), EInputActionValueType::Boolean);
	PanelDownAction = MakeAction(TEXT("IA_PanelDown"), EInputActionValueType::Boolean);
	PanelLeftAction = MakeAction(TEXT("IA_PanelLeft"), EInputActionValueType::Boolean);
	PanelRightAction = MakeAction(TEXT("IA_PanelRight"), EInputActionValueType::Boolean);
	PanelResetAction = MakeAction(TEXT("IA_PanelReset"), EInputActionValueType::Boolean);

	UInputMappingContext* Context = NewObject<UInputMappingContext>(this, TEXT("IMC_Hero_Default"));

	// Move: X = right, Y = forward. W/S go through a swizzle so they drive Y.
	Context->MapKey(MoveAction, EKeys::W).Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Context));
	{
		FEnhancedActionKeyMapping& Back = Context->MapKey(MoveAction, EKeys::S);
		Back.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Context));
		Back.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}
	Context->MapKey(MoveAction, EKeys::A).Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	Context->MapKey(MoveAction, EKeys::D);
	Context->MapKey(MoveAction, EKeys::Gamepad_Left2D).Modifiers.Add(NewObject<UInputModifierDeadZone>(Context));

	Context->MapKey(LookMouseAction, EKeys::Mouse2D);
	Context->MapKey(LookStickAction, EKeys::Gamepad_Right2D).Modifiers.Add(NewObject<UInputModifierDeadZone>(Context));

	// Approved default layout (Docs/Phase0_PreProduction.md, Section 11).
	Context->MapKey(TraversalAction, EKeys::LeftShift);
	Context->MapKey(TraversalAction, EKeys::Gamepad_RightTrigger);
	Context->MapKey(TraversalAction, EKeys::Gamepad_LeftThumbstick);
	Context->MapKey(JumpAction, EKeys::SpaceBar);
	Context->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	Context->MapKey(GlideAction, EKeys::G);
	Context->MapKey(GlideAction, EKeys::Gamepad_DPad_Up);

	// Prototype and debug keys.
	Context->MapKey(FastTravelAction, EKeys::T);
	Context->MapKey(FastTravelAction, EKeys::Gamepad_DPad_Right);
	Context->MapKey(RespawnAction, EKeys::F5);
	Context->MapKey(ToggleStatsAction, EKeys::F1);
	Context->MapKey(TogglePanelAction, EKeys::F2);
	Context->MapKey(PanelUpAction, EKeys::PageUp);
	Context->MapKey(PanelDownAction, EKeys::PageDown);
	Context->MapKey(PanelLeftAction, EKeys::Left);
	Context->MapKey(PanelRightAction, EKeys::Right);
	Context->MapKey(PanelResetAction, EKeys::BackSpace);

	DefaultMappingContext = Context;
}

void AHeroCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	BuildInput();
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogWebOfTheCity, Error, TEXT("HeroCharacter needs Enhanced Input. Check Config/DefaultInput.ini (DefaultInputComponentClass)."));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHeroCharacter::OnMove);
	Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &AHeroCharacter::OnMoveStopped);
	Input->BindAction(LookMouseAction, ETriggerEvent::Triggered, this, &AHeroCharacter::OnLookMouse);
	Input->BindAction(LookStickAction, ETriggerEvent::Triggered, this, &AHeroCharacter::OnLookStick);
	Input->BindAction(LookStickAction, ETriggerEvent::Completed, this, &AHeroCharacter::OnLookStickStopped);
	Input->BindAction(TraversalAction, ETriggerEvent::Started, this, &AHeroCharacter::OnTraversalStarted);
	Input->BindAction(TraversalAction, ETriggerEvent::Completed, this, &AHeroCharacter::OnTraversalCompleted);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &AHeroCharacter::OnJumpStarted);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &AHeroCharacter::OnJumpCompleted);
	Input->BindAction(GlideAction, ETriggerEvent::Started, this, &AHeroCharacter::OnGlideStarted);
	Input->BindAction(FastTravelAction, ETriggerEvent::Started, this, &AHeroCharacter::FastTravelToNextStation);
	Input->BindAction(RespawnAction, ETriggerEvent::Started, this, &AHeroCharacter::Respawn);
	Input->BindAction(ToggleStatsAction, ETriggerEvent::Started, this, &AHeroCharacter::OnToggleStats);
	Input->BindAction(TogglePanelAction, ETriggerEvent::Started, this, &AHeroCharacter::OnTogglePanel);
	Input->BindAction(PanelUpAction, ETriggerEvent::Started, this, &AHeroCharacter::OnPanelUp);
	Input->BindAction(PanelDownAction, ETriggerEvent::Started, this, &AHeroCharacter::OnPanelDown);
	Input->BindAction(PanelLeftAction, ETriggerEvent::Started, this, &AHeroCharacter::OnPanelLeft);
	Input->BindAction(PanelRightAction, ETriggerEvent::Started, this, &AHeroCharacter::OnPanelRight);
	Input->BindAction(PanelResetAction, ETriggerEvent::Started, this, &AHeroCharacter::OnPanelReset);
}

void AHeroCharacter::OnMove(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>();
	PushTraversalInput();

	// On the ground Unreal's walking does the moving; in the air the traversal sim reads MoveInput.
	UHeroMovementComponent* Move = GetHeroMovement();
	if (Move && Move->IsMovingOnGround() && Controller)
	{
		const FRotator YawRotation(0.0, Controller->GetControlRotation().Yaw, 0.0);
		const FRotationMatrix Basis(YawRotation);
		AddMovementInput(Basis.GetUnitAxis(EAxis::X), float(MoveInput.Y));
		AddMovementInput(Basis.GetUnitAxis(EAxis::Y), float(MoveInput.X));
	}
}

void AHeroCharacter::OnMoveStopped(const FInputActionValue& Value)
{
	MoveInput = FVector2D::ZeroVector;
	PushTraversalInput();
}

void AHeroCharacter::OnLookMouse(const FInputActionValue& Value)
{
	const FVector2D Delta = Value.Get<FVector2D>();
	AddLook(Delta.X * MouseSensitivity, Delta.Y * MouseSensitivity);
}

void AHeroCharacter::OnLookStick(const FInputActionValue& Value)
{
	StickLook = Value.Get<FVector2D>();
}

void AHeroCharacter::OnLookStickStopped(const FInputActionValue& Value)
{
	StickLook = FVector2D::ZeroVector;
}

void AHeroCharacter::AddLook(double YawDelta, double PitchDelta)
{
	if (!Controller)
	{
		return;
	}
	// Set the control rotation directly so "mouse up = look up" doesn't depend on legacy input scales.
	FRotator Rotation = Controller->GetControlRotation();
	Rotation.Yaw += YawDelta;
	Rotation.Pitch = FMath::ClampAngle(Rotation.Pitch + (bInvertLookY ? -PitchDelta : PitchDelta), double(CameraPitchMin), double(CameraPitchMax));
	Controller->SetControlRotation(Rotation);
	PushTraversalInput();
}

void AHeroCharacter::OnTraversalStarted()
{
	bTraversalHeld = true;
	LatencyFlashFrame = GFrameCounter;
	PushTraversalInput();
}

void AHeroCharacter::OnTraversalCompleted()
{
	bTraversalHeld = false;
	PushTraversalInput();
}

void AHeroCharacter::OnJumpStarted()
{
	bJumpHeld = true;
	LatencyFlashFrame = GFrameCounter;
	UHeroMovementComponent* Move = GetHeroMovement();
	if (Move && Move->IsMovingOnGround())
	{
		Jump();
	}
	if (Move)
	{
		Move->QueueJumpPressed();
	}
	PushTraversalInput();
}

void AHeroCharacter::OnJumpCompleted()
{
	bJumpHeld = false;
	StopJumping();
	PushTraversalInput();
}

void AHeroCharacter::OnGlideStarted()
{
	if (UHeroMovementComponent* Move = GetHeroMovement())
	{
		Move->QueueGlideToggle();
	}
}

void AHeroCharacter::PushTraversalInput()
{
	UHeroMovementComponent* Move = GetHeroMovement();
	if (!Move || !Controller)
	{
		return;
	}
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotationMatrix Basis(FRotator(0.0, ControlRotation.Yaw, 0.0));

	FTraversalInput Input;
	Input.Move = Basis.GetUnitAxis(EAxis::X) * MoveInput.Y + Basis.GetUnitAxis(EAxis::Y) * MoveInput.X;
	if (Input.Move.SizeSquared() > 1.0)
	{
		Input.Move = Input.Move.GetSafeNormal();
	}
	Input.CameraForward = ControlRotation.Vector();
	Input.bTraversalHeld = bTraversalHeld;
	Input.bJumpHeld = bJumpHeld;
	Move->SetTraversalInput(Input);
}

// ------------------------------------------------------------------ debug HUD keys

AHeroDebugHUD* AHeroCharacter::GetDebugHUD() const
{
	const APlayerController* PC = Cast<APlayerController>(Controller);
	return PC ? Cast<AHeroDebugHUD>(PC->GetHUD()) : nullptr;
}

void AHeroCharacter::OnToggleStats()
{
	if (AHeroDebugHUD* HUD = GetDebugHUD()) HUD->ToggleStats();
}

void AHeroCharacter::OnTogglePanel()
{
	if (AHeroDebugHUD* HUD = GetDebugHUD()) HUD->TogglePanel();
}

void AHeroCharacter::OnPanelUp()
{
	if (AHeroDebugHUD* HUD = GetDebugHUD()) HUD->PanelSelect(-1);
}

void AHeroCharacter::OnPanelDown()
{
	if (AHeroDebugHUD* HUD = GetDebugHUD()) HUD->PanelSelect(1);
}

void AHeroCharacter::OnPanelLeft()
{
	if (AHeroDebugHUD* HUD = GetDebugHUD()) HUD->PanelAdjust(-1);
}

void AHeroCharacter::OnPanelRight()
{
	if (AHeroDebugHUD* HUD = GetDebugHUD()) HUD->PanelAdjust(1);
}

void AHeroCharacter::OnPanelReset()
{
	if (AHeroDebugHUD* HUD = GetDebugHUD()) HUD->PanelReset();
}

// ------------------------------------------------------------------ per frame

void AHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!StickLook.IsNearlyZero())
	{
		AddLook(StickLook.X * StickYawRate * DeltaSeconds, StickLook.Y * StickPitchRate * DeltaSeconds);
	}
	PushTraversalInput();
	UpdateCamera(DeltaSeconds);
	UpdateWebVisuals();
	HandleTraversalEvents();
}

void AHeroCharacter::UpdateCamera(float DeltaSeconds)
{
	const UHeroMovementComponent* Move = GetHeroMovement();
	if (!Move || !CameraBoom || !FollowCamera)
	{
		return;
	}
	const FTraversalTuning& T = Move->GetTuning();
	const FTraversalCameraTarget Target = FTraversalSim::ComputeCameraTarget(T, GetVelocity() * HeroCharacter::UUToMeters);
	const float InterpSpeed = float(T.CameraInterpSpeed);

	CurrentFov = FMath::FInterpTo(CurrentFov, float(Target.Fov) + FovOffset, DeltaSeconds, InterpSpeed);
	CurrentArmLength = FMath::FInterpTo(CurrentArmLength, float(Target.ArmLength * HeroCharacter::MetersToUU), DeltaSeconds, InterpSpeed);
	CurrentLookAhead = FMath::VInterpTo(CurrentLookAhead, Target.LookAhead * HeroCharacter::MetersToUU, DeltaSeconds, InterpSpeed);

	FollowCamera->SetFieldOfView(FMath::Clamp(CurrentFov, 40.f, 140.f));
	CameraBoom->TargetArmLength = CurrentArmLength;
	CameraBoom->TargetOffset = CurrentLookAhead;
	CameraBoom->CameraLagSpeed = float(T.CameraLagSpeed);
	FollowCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
	FollowCamera->PostProcessSettings.MotionBlurAmount = float(Target.MotionBlur);
}

void AHeroCharacter::UpdateWebVisuals()
{
	const UHeroMovementComponent* Move = GetHeroMovement();
	if (!Move || !WebLine || !AnchorMarker)
	{
		return;
	}
	const FTraversalSim& Sim = Move->GetSim();
	const FVector Hand = GetActorLocation() + FVector(0.0, 0.0, 60.0);

	bool bShowWeb = false;
	FVector WebEnd = FVector::ZeroVector;
	if (Sim.GetState() == ETraversalState::Swing && Sim.GetSwingAnchor().bValid)
	{
		bShowWeb = true;
		WebEnd = Sim.GetSwingAnchor().SurfacePoint * HeroCharacter::MetersToUU;
	}
	else if (Sim.IsZipping())
	{
		bShowWeb = true;
		WebEnd = Sim.GetZipTarget() * HeroCharacter::MetersToUU;
	}

	const FVector Span = WebEnd - Hand;
	const double Length = Span.Size();
	bShowWeb = bShowWeb && Length > 1.0;
	WebLine->SetHiddenInGame(!bShowWeb);
	if (bShowWeb)
	{
		// The engine cylinder is 1 m tall and 1 m wide, centred on its pivot.
		WebLine->SetWorldLocationAndRotation(Hand + Span * 0.5, FRotationMatrix::MakeFromZ(Span / Length).Rotator());
		WebLine->SetWorldScale3D(FVector(WebThickness, WebThickness, Length * HeroCharacter::UUToMeters));
	}

	const FTraversalAnchor& Preview = Sim.GetPreviewAnchor();
	const bool bShowPreview = Preview.bValid && Sim.GetState() != ETraversalState::Swing && !Move->IsMovingOnGround();
	AnchorMarker->SetHiddenInGame(!bShowPreview);
	if (bShowPreview)
	{
		AnchorMarker->SetWorldLocation(Preview.SurfacePoint * HeroCharacter::MetersToUU);
	}
}

void AHeroCharacter::HandleTraversalEvents()
{
	UHeroMovementComponent* Move = GetHeroMovement();
	if (!Move)
	{
		return;
	}
	const FTraversalEvents Events = Move->ConsumeTraversalEvents();
	if (Events.bPerfectRelease) AddRecentEvent(TEXT("PERFECT RELEASE"));
	if (Events.bTierUp) AddRecentEvent(FString::Printf(TEXT("SPEED TIER %d"), Move->GetSim().GetSpeedTier() + 1));
	if (Events.bSkyAnchor) AddRecentEvent(TEXT("sky anchor"));
	if (Events.bAutoCatch) AddRecentEvent(TEXT("auto web-catch"));
	if (Events.bLaunch) AddRecentEvent(TEXT("POINT LAUNCH"));
	if (Events.bVault) AddRecentEvent(TEXT("vault"));
	if (Events.bStreakEnded)
	{
		const FTraversalStreak& Streak = Move->GetSim().GetLastStreak();
		AddRecentEvent(FString::Printf(TEXT("touched down after %.0f m"), Streak.Distance));
		LogStreak(Streak);
	}
}

void AHeroCharacter::AddRecentEvent(const FString& Text)
{
	FHeroRecentEvent Event;
	Event.Text = Text;
	Event.Time = GetWorld()->GetTimeSeconds();
	RecentEvents.Insert(Event, 0);
	if (RecentEvents.Num() > HeroCharacter::MaxRecentEvents)
	{
		RecentEvents.SetNum(HeroCharacter::MaxRecentEvents);
	}
}

void AHeroCharacter::LogStreak(const FTraversalStreak& Streak) const
{
	if (Streak.Distance < MinLoggedStreakDistance)
	{
		return;
	}
	// Recorded test runs (Section 6.1 "log time and minimum speed"): one row per airborne stretch.
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Traversal");
	const FString Path = Dir / TEXT("Runs.csv");
	IFileManager::Get().MakeDirectory(*Dir, true);

	FString Text;
	if (!IFileManager::Get().FileExists(*Path))
	{
		Text += TEXT("date,map,distance_m,time_s,avg_speed_mps,min_speed_mps,max_speed_mps,max_tier,swings,perfect_releases\n");
	}
	Text += FString::Printf(TEXT("%s,%s,%.1f,%.2f,%.2f,%.2f,%.2f,%d,%d,%d\n"),
		*FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")), *GetWorld()->GetMapName(),
		Streak.Distance, Streak.Time, Streak.Time > 0.0 ? Streak.Distance / Streak.Time : 0.0,
		Streak.MinSpeed, Streak.MaxSpeed, Streak.MaxTier + 1, Streak.Swings, Streak.PerfectReleases);
	FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);

	UE_LOG(LogWebOfTheCity, Log, TEXT("Traversal run: %.0f m in %.1f s (min %.1f, max %.1f m/s, %d swings, %d perfect releases)"),
		Streak.Distance, Streak.Time, Streak.MinSpeed, Streak.MaxSpeed, Streak.Swings, Streak.PerfectReleases);
}

// ------------------------------------------------------------------ fast travel and respawn

void AHeroCharacter::FastTravelToNextStation()
{
	TArray<AActor*> Stations;
	UGameplayStatics::GetAllActorsWithTag(this, FName(TEXT("FastTravel")), Stations);
	if (Stations.Num() == 0)
	{
		UE_LOG(LogWebOfTheCity, Warning, TEXT("Fast travel: no actors tagged 'FastTravel' in this map (run Tools/Editor/make_greybox_city.py)."));
		return;
	}
	Stations.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
	const AActor* Station = Stations[NextStationIndex % Stations.Num()];
	++NextStationIndex;
	TeleportWithFade(Station->GetActorLocation() + FVector(0.0, 0.0, 100.0));
}

void AHeroCharacter::Respawn()
{
	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	const AActor* Start = GameMode ? GameMode->FindPlayerStart(Controller) : nullptr;
	if (Start)
	{
		TeleportWithFade(Start->GetActorLocation());
	}
}

void AHeroCharacter::TeleportWithFade(const FVector& Location)
{
	PendingTeleport = Location;
	const APlayerController* PC = Cast<APlayerController>(Controller);
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, FastTravelFadeTime, FLinearColor::Black, false, true);
	}
	GetWorldTimerManager().SetTimer(TeleportTimer, this, &AHeroCharacter::FinishTeleport, FMath::Max(FastTravelFadeTime, 0.01f), false);
}

void AHeroCharacter::FinishTeleport()
{
	SetActorLocation(PendingTeleport, false, nullptr, ETeleportType::TeleportPhysics);
	if (UHeroMovementComponent* Move = GetHeroMovement())
	{
		Move->ResetTraversal();
	}
	const APlayerController* PC = Cast<APlayerController>(Controller);
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, FastTravelFadeTime, FLinearColor::Black, false, false);
	}
}
