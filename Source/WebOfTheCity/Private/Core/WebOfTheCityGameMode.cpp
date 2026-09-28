#include "Core/WebOfTheCityGameMode.h"

#include "Misc/PackageName.h"
#include "Traversal/HeroCharacter.h"
#include "Traversal/HeroDebugHUD.h"

AWebOfTheCityGameMode::AWebOfTheCityGameMode()
{
	DefaultPawnClass = AHeroCharacter::StaticClass();
	HUDClass = AHeroDebugHUD::StaticClass();
	HeroBlueprint = FSoftClassPath(TEXT("/Game/WebOfTheCity/Characters/Hero/BP_Hero.BP_Hero_C"));
}

UClass* AWebOfTheCityGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (HeroBlueprint.IsValid() && FPackageName::DoesPackageExist(HeroBlueprint.GetLongPackageName()))
	{
		if (UClass* HeroClass = HeroBlueprint.TryLoadClass<APawn>())
		{
			return HeroClass;
		}
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}
