#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Traversal/TraversalTypes.h"
#include "HeroCharacter.generated.h"

class AHeroDebugHUD;
class UCameraComponent;
class UHeroMovementComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/** A recent traversal event for the HUD ("PERFECT RELEASE", "TIER 2"...). */
struct FHeroRecentEvent
{
	FString Text;
	double Time = 0.0;
};

/**
 * The playable hero for the traversal prototype (Phase 1).
 *
 * Controls are built in code from the approved default layout (Section 11), so no input assets are
 * needed yet; rebinding arrives with the settings menu in Phase 3. The camera follows Section 11:
 * speed-scaled FOV, look-ahead, lag. A placeholder web line and anchor marker show what the swing is doing.
 */
UCLASS()
class WEBOFTHECITY_API AHeroCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHeroCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

	UFUNCTION(BlueprintPure, Category = "Traversal")
	UHeroMovementComponent* GetHeroMovement() const;

	/** Fast-travel stub: fade, jump to the next actor tagged "FastTravel", fade back. */
	UFUNCTION(BlueprintCallable, Category = "Traversal")
	void FastTravelToNextStation();

	/** Back to the player start. */
	UFUNCTION(BlueprintCallable, Category = "Traversal")
	void Respawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Placeholder web: a thin cylinder from the hand to the anchor. Real VFX come with the art pass. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	TObjectPtr<UStaticMeshComponent> WebLine;

	/** Placeholder anchor preview: a small sphere where the next web would stick. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal")
	TObjectPtr<UStaticMeshComponent> AnchorMarker;

	/** Degrees of turn per mouse count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls", meta = (ClampMin = "0.001"))
	float MouseSensitivity = 0.07f;

	/** Stick look speed at full deflection, degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls", meta = (ClampMin = "1"))
	float StickYawRate = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls", meta = (ClampMin = "1"))
	float StickPitchRate = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls")
	bool bInvertLookY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls", meta = (ClampMin = "-89", ClampMax = "0"))
	float CameraPitchMin = -75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls", meta = (ClampMin = "0", ClampMax = "89"))
	float CameraPitchMax = 70.f;

	/** Player FOV preference, added to the speed-scaled FOV (Section 11: adjustable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "-10", ClampMax = "40"))
	float FovOffset = 0.f;

	/** Web line thickness in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal", meta = (ClampMin = "0.005"))
	float WebThickness = 0.04f;

	/** Fade out and in for fast travel and respawn, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal", meta = (ClampMin = "0"))
	float FastTravelFadeTime = 0.35f;

	/** Airborne stretches at least this long (metres) are appended to Saved/Traversal/Runs.csv. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal", meta = (ClampMin = "0"))
	float MinLoggedStreakDistance = 50.f;

	/** Input latency test: the HUD flashes a white square on the frame a swing or jump press arrives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bLatencyFlash = false;

	uint64 GetLatencyFlashFrame() const { return LatencyFlashFrame; }

	const TArray<FHeroRecentEvent>& GetRecentEvents() const { return RecentEvents; }

protected:
	virtual void BeginPlay() override;

private:
	void BuildInput();
	AHeroDebugHUD* GetDebugHUD() const;

	void OnMove(const FInputActionValue& Value);
	void OnMoveStopped(const FInputActionValue& Value);
	void OnLookMouse(const FInputActionValue& Value);
	void OnLookStick(const FInputActionValue& Value);
	void OnLookStickStopped(const FInputActionValue& Value);
	void OnTraversalStarted();
	void OnTraversalCompleted();
	void OnJumpStarted();
	void OnJumpCompleted();
	void OnGlideStarted();
	void OnToggleStats();
	void OnTogglePanel();
	void OnPanelUp();
	void OnPanelDown();
	void OnPanelLeft();
	void OnPanelRight();
	void OnPanelReset();

	void AddLook(double YawDelta, double PitchDelta);
	void PushTraversalInput();
	void UpdateCamera(float DeltaSeconds);
	void UpdateWebVisuals();
	void HandleTraversalEvents();
	void AddRecentEvent(const FString& Text);
	void LogStreak(const FTraversalStreak& Streak) const;
	void TeleportWithFade(const FVector& Location);
	void FinishTeleport();

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookMouseAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookStickAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TraversalAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> GlideAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FastTravelAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RespawnAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ToggleStatsAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TogglePanelAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PanelUpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PanelDownAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PanelLeftAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PanelRightAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PanelResetAction;

	FVector2D MoveInput = FVector2D::ZeroVector;
	FVector2D StickLook = FVector2D::ZeroVector;
	bool bTraversalHeld = false;
	bool bJumpHeld = false;
	uint64 LatencyFlashFrame = 0;

	float CurrentFov = 80.f;
	float CurrentArmLength = 350.f;
	FVector CurrentLookAhead = FVector::ZeroVector;

	TArray<FHeroRecentEvent> RecentEvents;
	int32 NextStationIndex = 0;
	FVector PendingTeleport = FVector::ZeroVector;
	FTimerHandle TeleportTimer;
};
