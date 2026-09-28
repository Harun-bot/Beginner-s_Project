#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WebOfTheCityGameMode.generated.h"

/**
 * Default game mode (set in Config/DefaultEngine.ini): the hero pawn and the debug HUD.
 * If BP_Hero exists (the mannequin and animations set up in the editor), it is used; otherwise the bare
 * C++ hero, shown as its capsule, so the prototype works before any Blueprint is made.
 */
UCLASS()
class WEBOFTHECITY_API AWebOfTheCityGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWebOfTheCityGameMode();

	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	UPROPERTY(EditAnywhere, Category = "Classes")
	FSoftClassPath HeroBlueprint;
};
