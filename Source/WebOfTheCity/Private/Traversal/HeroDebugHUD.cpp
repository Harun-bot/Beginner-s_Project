#include "Traversal/HeroDebugHUD.h"

#include "Camera/CameraComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Traversal/HeroCharacter.h"
#include "Traversal/HeroMovementComponent.h"
#include "Traversal/TraversalSim.h"
#include "Traversal/TraversalTuningAsset.h"
#include "UObject/UnrealType.h"
#include "WebOfTheCity.h"

namespace HeroHUD
{
	AHeroCharacter* FindHero(UWorld* World)
	{
		return World ? Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
	}

	void MarkDirty(const UHeroMovementComponent* Move)
	{
#if WITH_EDITOR
		if (UTraversalTuningAsset* Asset = Move ? Move->GetTuningAsset() : nullptr)
		{
			Asset->MarkPackageDirty();
		}
#endif
	}
}

// ------------------------------------------------------------------ console commands

static FAutoConsoleCommandWithWorld GTravDumpCommand(
	TEXT("Trav.Dump"),
	TEXT("Print every traversal tuning value to the log."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const AHeroCharacter* Hero = HeroHUD::FindHero(World);
		const UHeroMovementComponent* Move = Hero ? Hero->GetHeroMovement() : nullptr;
		if (!Move)
		{
			return;
		}
		for (TFieldIterator<FProperty> It(FTraversalTuning::StaticStruct()); It; ++It)
		{
			UE_LOG(LogWebOfTheCity, Display, TEXT("%s = %s"), *It->GetName(), *AHeroDebugHUD::FormatTuningValue(*It, Move->GetTuning()));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GTravSetCommand(
	TEXT("Trav.Set"),
	TEXT("Set one traversal tuning value. Usage: Trav.Set SwingGravityScale 2.2"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const AHeroCharacter* Hero = HeroHUD::FindHero(World);
		UHeroMovementComponent* Move = Hero ? Hero->GetHeroMovement() : nullptr;
		if (!Move || Args.Num() < 2)
		{
			UE_LOG(LogWebOfTheCity, Warning, TEXT("Usage: Trav.Set <Name> <Value>   (Trav.Dump lists the names)"));
			return;
		}
		FProperty* Property = FTraversalTuning::StaticStruct()->FindPropertyByName(FName(*Args[0]));
		if (!Property)
		{
			UE_LOG(LogWebOfTheCity, Warning, TEXT("Trav.Set: no tuning value called '%s'."), *Args[0]);
			return;
		}
		FTraversalTuning& Tuning = Move->GetMutableTuning();
		if (!Property->ImportText_Direct(*Args[1], Property->ContainerPtrToValuePtr<void>(&Tuning), nullptr, PPF_None))
		{
			UE_LOG(LogWebOfTheCity, Warning, TEXT("Trav.Set: '%s' is not a valid value for %s."), *Args[1], *Args[0]);
			return;
		}
		HeroHUD::MarkDirty(Move);
		UE_LOG(LogWebOfTheCity, Display, TEXT("%s = %s"), *Args[0], *AHeroDebugHUD::FormatTuningValue(Property, Tuning));
	}));

static FAutoConsoleCommandWithWorldAndArgs GTravLatencyFlashCommand(
	TEXT("Trav.LatencyFlash"),
	TEXT("1 = flash a white square on the frame a swing or jump press arrives. Film screen and keyboard at 240 fps to measure input latency."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (AHeroCharacter* Hero = HeroHUD::FindHero(World))
		{
			Hero->bLatencyFlash = Args.Num() == 0 || Args[0] != TEXT("0");
		}
	}));

static FAutoConsoleCommandWithWorld GTravStatsCommand(
	TEXT("Trav.Stats"),
	TEXT("Print the current, last and best airborne runs."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const AHeroCharacter* Hero = HeroHUD::FindHero(World);
		const UHeroMovementComponent* Move = Hero ? Hero->GetHeroMovement() : nullptr;
		if (!Move)
		{
			return;
		}
		auto Print = [](const TCHAR* Label, const FTraversalStreak& S)
		{
			UE_LOG(LogWebOfTheCity, Display, TEXT("%s: %.0f m in %.1f s, speed min %.1f max %.1f m/s, top tier %d, %d swings, %d perfect releases"),
				Label, S.Distance, S.Time, S.MinSpeed, S.MaxSpeed, S.MaxTier + 1, S.Swings, S.PerfectReleases);
		};
		Print(TEXT("Current"), Move->GetSim().GetStreak());
		Print(TEXT("Last"), Move->GetSim().GetLastStreak());
		Print(TEXT("Best"), Move->GetSim().GetBestStreak());
	}));

// ------------------------------------------------------------------ HUD

void AHeroDebugHUD::BeginPlay()
{
	Super::BeginPlay();
	Fields.Reset();
	for (TFieldIterator<FProperty> It(FTraversalTuning::StaticStruct()); It; ++It)
	{
		Fields.Add(*It);
	}
}

FString AHeroDebugHUD::FormatTuningValue(const FProperty* Property, const FTraversalTuning& Tuning)
{
	const void* Value = Property->ContainerPtrToValuePtr<void>(&Tuning);
	if (const FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
	{
		return FString::Printf(TEXT("%.3f"), DoubleProperty->GetPropertyValue(Value));
	}
	if (const FIntProperty* IntProperty = CastField<FIntProperty>(Property))
	{
		return FString::FromInt(IntProperty->GetPropertyValue(Value));
	}
	if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		return BoolProperty->GetPropertyValue(Value) ? TEXT("true") : TEXT("false");
	}
	FString Text;
	Property->ExportTextItem_Direct(Text, Value, nullptr, nullptr, PPF_None);
	return Text;
}

FTraversalTuning* AHeroDebugHUD::GetLiveTuning() const
{
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwningPawn());
	UHeroMovementComponent* Move = Hero ? Hero->GetHeroMovement() : nullptr;
	return Move ? &Move->GetMutableTuning() : nullptr;
}

void AHeroDebugHUD::MarkTuningEdited() const
{
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwningPawn());
	HeroHUD::MarkDirty(Hero ? Hero->GetHeroMovement() : nullptr);
}

void AHeroDebugHUD::TogglePanel()
{
	bShowPanel = !bShowPanel;
	if (bShowPanel && !bHasSnapshot)
	{
		if (const FTraversalTuning* Tuning = GetLiveTuning())
		{
			Snapshot = *Tuning;
			bHasSnapshot = true;
		}
	}
}

void AHeroDebugHUD::PanelSelect(int32 Delta)
{
	if (bShowPanel && Fields.Num() > 0)
	{
		Selected = (Selected + Delta + Fields.Num()) % Fields.Num();
	}
}

void AHeroDebugHUD::PanelAdjust(int32 Direction)
{
	FTraversalTuning* Tuning = GetLiveTuning();
	if (!bShowPanel || !Tuning || !Fields.IsValidIndex(Selected))
	{
		return;
	}
	FProperty* Property = Fields[Selected];
	void* Value = Property->ContainerPtrToValuePtr<void>(Tuning);

	if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
	{
		const double Old = DoubleProperty->GetPropertyValue(Value);
		double New = Old + Direction * FMath::Max(FMath::Abs(Old) * double(PanelStepFraction), 0.01);
#if WITH_EDITOR
		if (Property->HasMetaData(TEXT("ClampMin")))
		{
			New = FMath::Max(New, FCString::Atod(*Property->GetMetaData(TEXT("ClampMin"))));
		}
		if (Property->HasMetaData(TEXT("ClampMax")))
		{
			New = FMath::Min(New, FCString::Atod(*Property->GetMetaData(TEXT("ClampMax"))));
		}
#endif
		DoubleProperty->SetPropertyValue(Value, New);
	}
	else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
	{
		IntProperty->SetPropertyValue(Value, FMath::Max(0, IntProperty->GetPropertyValue(Value) + Direction));
	}
	else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		BoolProperty->SetPropertyValue(Value, !BoolProperty->GetPropertyValue(Value));
	}
	MarkTuningEdited();
}

void AHeroDebugHUD::PanelReset()
{
	FTraversalTuning* Tuning = GetLiveTuning();
	if (!bShowPanel || !Tuning || !bHasSnapshot || !Fields.IsValidIndex(Selected))
	{
		return;
	}
	const FProperty* Property = Fields[Selected];
	Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Tuning), Property->ContainerPtrToValuePtr<void>(&Snapshot));
	MarkTuningEdited();
}

void AHeroDebugHUD::Line(const FString& Text, float X, float& Y, const FLinearColor& Color)
{
	DrawText(Text, Color, X, Y, GEngine ? GEngine->GetSmallFont() : nullptr, TextScale);
	Y += 14.f * TextScale;
}

void AHeroDebugHUD::DrawHUD()
{
	Super::DrawHUD();

	const AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwningPawn());
	if (!Hero || !Canvas)
	{
		return;
	}
	if (Hero->bLatencyFlash && Hero->GetLatencyFlashFrame() == GFrameCounter)
	{
		DrawRect(FLinearColor::White, Canvas->ClipX - 140.f, 20.f, 120.f, 120.f);
	}
	if (bShowStats)
	{
		DrawStats(*Hero);
	}
	if (bShowPanel)
	{
		DrawPanel();
	}
}

void AHeroDebugHUD::DrawStats(const AHeroCharacter& Hero)
{
	const UHeroMovementComponent* Move = Hero.GetHeroMovement();
	if (!Move)
	{
		return;
	}
	const FTraversalSim& Sim = Move->GetSim();
	const FTraversalTuning& T = Move->GetTuning();
	const FVector Position = Hero.GetActorLocation() * 0.01;
	const double Speed = Hero.GetVelocity().Size() * 0.01;
	const FLinearColor Grey(0.65f, 0.65f, 0.65f);

	float X = 20.f;
	float Y = 20.f;
	Line(FString::Printf(TEXT("%s  (%.1f s)"), FTraversalSim::StateName(Sim.GetState()), Sim.GetStateTime()), X, Y, FLinearColor::Yellow);
	Line(FString::Printf(TEXT("Speed %.1f m/s (%.0f km/h)   height %.0f m"), Speed, Speed * 3.6, Position.Z), X, Y);
	Line(FString::Printf(TEXT("Speed tier %d/3 (cap %.0f m/s)   chain %d"), Sim.GetSpeedTier() + 1, Sim.GetSpeedCap(), Sim.GetChainCount()), X, Y);

	if (Sim.GetState() == ETraversalState::Swing)
	{
		const double Angle = Sim.GetSwingAngle(Position);
		const bool bInWindow = Angle >= T.ReleaseWindowMinAngle && Angle <= T.ReleaseWindowMaxAngle;
		Line(FString::Printf(TEXT("Rope %.0f m (%s anchor)   swing angle %+.0f deg%s"), Sim.GetRopeLength(),
			Sim.GetSwingAnchor().bSky ? TEXT("sky") : TEXT("wall"), Angle, bInWindow ? TEXT("   << release now") : TEXT("")),
			X, Y, bInWindow ? FLinearColor::Green : FLinearColor::White);
	}
	else if (Sim.GetPreviewAnchor().bValid)
	{
		const FVector To = Sim.GetPreviewAnchor().SurfacePoint - Position;
		const double Distance = FMath::Max(To.Size(), 0.01);
		const double Elevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(To.Z / Distance, -1.0, 1.0)));
		Line(FString::Printf(TEXT("Next anchor %.0f m away, %.0f deg up"), Distance, Elevation), X, Y);
	}
	else if (!Move->IsMovingOnGround())
	{
		Line(FString::Printf(TEXT("No anchor in reach   (sky anchors left: %d)"), Sim.GetSkyAnchorsLeft()), X, Y, FLinearColor(1.f, 0.5f, 0.2f));
	}

	// The Phase 1 Definition of Done measure: 2 km without touching down.
	const FTraversalStreak& Streak = Sim.GetStreak();
	Line(FString::Printf(TEXT("Airborne %.0f m in %.0f s   (best %.0f m)"), Streak.Distance, Streak.Time, Sim.GetBestStreak().Distance),
		X, Y, Streak.Distance >= 2000.0 ? FLinearColor::Green : FLinearColor::White);
	const FTraversalStreak& Last = Sim.GetLastStreak();
	if (Last.Time > 0.0)
	{
		Line(FString::Printf(TEXT("Last run %.0f m, %.0f s, speed %.0f-%.0f m/s, %d swings, %d perfect"),
			Last.Distance, Last.Time, Last.MinSpeed, Last.MaxSpeed, Last.Swings, Last.PerfectReleases), X, Y, Grey);
	}
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
	Line(FString::Printf(TEXT("FOV %.0f   %.0f FPS"), Hero.FollowCamera ? Hero.FollowCamera->FieldOfView : 0.f, DeltaSeconds > 0.f ? 1.f / DeltaSeconds : 0.f), X, Y, Grey);

	Y += 8.f;
	const double Now = GetWorld()->GetTimeSeconds();
	for (const FHeroRecentEvent& Event : Hero.GetRecentEvents())
	{
		if (Now - Event.Time < EventDisplayTime)
		{
			Line(Event.Text, X, Y, FLinearColor(0.3f, 1.f, 1.f));
		}
	}

	Y = Canvas->ClipY - 30.f;
	Line(TEXT("Shift swing/sprint/wall-run   Space jump/zip   G glide   T fast travel   F5 respawn   F1 stats   F2 tuning"), X, Y, Grey);
}

void AHeroDebugHUD::DrawPanel()
{
	FTraversalTuning* Tuning = GetLiveTuning();
	if (!Tuning || Fields.Num() == 0)
	{
		return;
	}
	const float Width = 470.f * TextScale;
	const float RowHeight = 14.f * TextScale;
	const float X = Canvas->ClipX - Width - 20.f;
	float Y = 20.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), X - 10.f, Y - 6.f, Width + 20.f, RowHeight * float(PanelRows + 3) + 12.f);

	const AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwningPawn());
	const UTraversalTuningAsset* Asset = Hero && Hero->GetHeroMovement() ? Hero->GetHeroMovement()->GetTuningAsset() : nullptr;
	Line(Asset ? FString::Printf(TEXT("Tuning: %s  (save the asset to keep changes)"), *Asset->GetName())
		: FString(TEXT("Tuning: built-in defaults  (create DA_TraversalTuning to keep changes)")), X, Y, FLinearColor::Yellow);
	Line(TEXT("PgUp/PgDn select   Left/Right change   Backspace restore"), X, Y, FLinearColor(0.65f, 0.65f, 0.65f));
	Y += 4.f;

	const int32 First = FMath::Clamp(Selected - PanelRows / 2, 0, FMath::Max(0, Fields.Num() - PanelRows));
	const int32 Last = FMath::Min(Fields.Num(), First + PanelRows);
	for (int32 Index = First; Index < Last; ++Index)
	{
		const FProperty* Property = Fields[Index];
		const bool bSelected = Index == Selected;
		const bool bChanged = bHasSnapshot && !Property->Identical(Property->ContainerPtrToValuePtr<void>(Tuning), Property->ContainerPtrToValuePtr<void>(&Snapshot));
		Line(FString::Printf(TEXT("%s%s = %s"), bSelected ? TEXT("> ") : TEXT("   "), *Property->GetName(), *FormatTuningValue(Property, *Tuning)),
			X, Y, bSelected ? FLinearColor::Yellow : (bChanged ? FLinearColor(0.4f, 1.f, 0.4f) : FLinearColor::White));
	}
}
