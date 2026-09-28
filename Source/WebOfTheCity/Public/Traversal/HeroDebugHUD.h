#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Traversal/TraversalTypes.h"
#include "HeroDebugHUD.generated.h"

class AHeroCharacter;
class FProperty;

/**
 * Phase 1 debug HUD and live-tuning panel (Section 6.1).
 *
 * F1: stats (state, speed, tier, anchor, airborne distance, recent events).
 * F2: tuning panel. PageUp/PageDown select a value, Left/Right change it, Backspace restores it.
 * Changes apply immediately. In the editor they edit DA_TraversalTuning, so saving the asset keeps them.
 */
UCLASS()
class WEBOFTHECITY_API AHeroDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	void ToggleStats() { bShowStats = !bShowStats; }
	void TogglePanel();
	void PanelSelect(int32 Delta);
	void PanelAdjust(int32 Direction);
	void PanelReset();

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bShowStats = true;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bShowPanel = false;

	UPROPERTY(EditAnywhere, Category = "Debug", meta = (ClampMin = "0.5", ClampMax = "4"))
	float TextScale = 1.2f;

	/** Each Left/Right press changes a number by this fraction of its current value. */
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (ClampMin = "0.001", ClampMax = "1"))
	float PanelStepFraction = 0.05f;

	/** Rows of the tuning panel shown at once. */
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (ClampMin = "5"))
	int32 PanelRows = 24;

	/** Seconds a traversal event stays on screen. */
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (ClampMin = "0.1"))
	float EventDisplayTime = 1.5f;

	/** Formats one tuning value for display (shared with the Trav.* console commands). */
	static FString FormatTuningValue(const FProperty* Property, const FTraversalTuning& Tuning);

private:
	FTraversalTuning* GetLiveTuning() const;
	void MarkTuningEdited() const;
	void DrawStats(const AHeroCharacter& Hero);
	void DrawPanel();
	void Line(const FString& Text, float X, float& Y, const FLinearColor& Color = FLinearColor::White);

	TArray<FProperty*> Fields;
	int32 Selected = 0;
	FTraversalTuning Snapshot;
	bool bHasSnapshot = false;
};
