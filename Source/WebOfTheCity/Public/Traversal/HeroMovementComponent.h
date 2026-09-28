#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Traversal/TraversalSim.h"
#include "HeroMovementComponent.generated.h"

class UTraversalTuningAsset;

/**
 * Character movement with the traversal core plugged in (Section 6.1).
 *
 * Walking stays Unreal's. Falling and every traversal state (swing, zip, wall-run, ...) run
 * FTraversalSim, the engine-independent core that Tools/TraversalSim tests outside the engine.
 * Mapping: Ground = Walking, Air/Launch = Falling, everything else = Custom (custom mode = ETraversalState).
 */
UCLASS()
class WEBOFTHECITY_API UHeroMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UHeroMovementComponent();

	/** DA_TraversalTuning. Left empty, the asset at DefaultTuningAsset is loaded if it exists, else DefaultTuning is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	TObjectPtr<UTraversalTuningAsset> TuningAsset;

	UPROPERTY(EditAnywhere, Category = "Traversal")
	TSoftObjectPtr<UTraversalTuningAsset> DefaultTuningAsset;

	/** Used when there is no tuning asset at all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	FTraversalTuning DefaultTuning;

	const FTraversalTuning& GetTuning() const;
	/** The live tuning; the tuning panel edits it in place. */
	FTraversalTuning& GetMutableTuning();
	UTraversalTuningAsset* GetTuningAsset() const;

	/** Held buttons, stick and camera for this frame. Called by the character whenever input changes. */
	void SetTraversalInput(const FTraversalInput& Input);
	/** One-frame button presses. Each is used by exactly one movement update. */
	void QueueJumpPressed();
	void QueueGlideToggle();

	const FTraversalSim& GetSim() const { return Sim; }
	ETraversalState GetTraversalState() const { return Sim.GetState(); }
	/** Traversal events since the last call (swing started, perfect release, landed...). */
	FTraversalEvents ConsumeTraversalEvents();

	/** After a teleport: forget ropes and walls and start falling. */
	void ResetTraversal();

	virtual void BeginPlay() override;
	virtual float GetMaxSpeed() const override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

protected:
	virtual void PhysFalling(float DeltaTime, int32 Iterations) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
	void PhysTraversal(float DeltaTime, int32 Iterations);
	void SyncMovementModeFromSim();
	FTraversalBody MakeBody() const;

	FTraversalSim Sim;
	/** Written by the character. */
	FTraversalInput PendingInput;
	/** What this frame's movement uses (edges cleared after the first physics step). */
	FTraversalInput FrameInput;
	/** True while we change the movement mode ourselves, so we don't echo it back into the sim. */
	bool bSyncingMode = false;
};
