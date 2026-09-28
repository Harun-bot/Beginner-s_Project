#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Traversal/TraversalTypes.h"
#include "TraversalTuningAsset.generated.h"

/**
 * DA_TraversalTuning: every traversal number in one asset (Section 6.1).
 * Create it at /Game/WebOfTheCity/Traversal/Data/DA_TraversalTuning and the hero picks it up
 * automatically. Edits made with the in-game tuning panel (F2) land here; save the asset to keep them.
 */
UCLASS(BlueprintType)
class WEBOFTHECITY_API UTraversalTuningAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal", meta = (ShowOnlyInnerProperties))
	FTraversalTuning Tuning;
};
