#include "Perf/PerfRouteRunner.h"

#include "Camera/CameraComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderCore.h"
#include "RHI.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogPerfRoute, Log, All);

namespace PerfRoute
{
	// Section 4.3: no hitch longer than 50 ms while swinging at top speed.
	constexpr float HitchThresholdMs = 50.f;

	// Column order is the contract with Tools/Perf/analyze_perf.py. Append new columns at the end.
	constexpr const TCHAR* CsvHeader = TEXT("frame,time_s,phase,distance_m,frame_ms,game_ms,render_ms,gpu_ms,ram_mb");

	FString GetCVarString(const TCHAR* Name)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
		return CVar ? CVar->GetString() : FString(TEXT("n/a"));
	}

	void SetCVarString(const TCHAR* Name, const FString& Value)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(*Value, ECVF_SetByConsole);
		}
	}

	// Metadata is written as "key=value" pairs joined by ';', so strip those characters from values.
	FString CleanMetadataValue(FString Value)
	{
		Value.TrimStartAndEndInline();
		Value.ReplaceInline(TEXT(";"), TEXT(","));
		Value.ReplaceInline(TEXT("="), TEXT("-"));
		return Value;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GPerfRouteStartCommand(
	TEXT("PerfRoute.Start"),
	TEXT("Start the PerfRouteRunner whose RouteName matches the argument (or the first one found). Usage: PerfRoute.Start Swing"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		const FName Wanted = Args.Num() > 0 ? FName(*Args[0]) : NAME_None;
		for (TActorIterator<APerfRouteRunner> It(World); It; ++It)
		{
			if (Wanted.IsNone() || It->RouteName == Wanted)
			{
				It->StartRun();
				return;
			}
		}
		UE_LOG(LogPerfRoute, Warning, TEXT("PerfRoute.Start: no PerfRouteRunner with RouteName '%s' in this map."), *Wanted.ToString());
	}));

APerfRouteRunner::APerfRouteRunner()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// The spline stays put; only the camera moves along it.
	Route = CreateDefaultSubobject<USplineComponent>(TEXT("Route"));
	Route->SetupAttachment(SceneRoot);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);
	Camera->SetFieldOfView(FieldOfView);
}

void APerfRouteRunner::BeginPlay()
{
	Super::BeginPlay();

	FString RequestedRoute;
	if (FParse::Value(FCommandLine::Get(), TEXT("PerfRoute="), RequestedRoute) && FName(*RequestedRoute) == RouteName)
	{
		bQuitWhenDone = FParse::Param(FCommandLine::Get(), TEXT("PerfRouteQuit"));
		StartRun();
	}
	else if (bAutoStartInEditor && GetWorld()->IsPlayInEditor())
	{
		StartRun();
	}
}

void APerfRouteRunner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Stopping PIE mid-run still writes what was captured.
	if (IsRunning())
	{
		bQuitWhenDone = false;
		FinishRun();
	}
	Super::EndPlay(EndPlayReason);
}

void APerfRouteRunner::StartRun()
{
	if (IsRunning())
	{
		return;
	}

	if (Route->GetSplineLength() < 100.f)
	{
		UE_LOG(LogPerfRoute, Error, TEXT("Route '%s' is shorter than 1 m. Add spline points before running."), *RouteName.ToString());
		return;
	}

	Phase = EPerfRoutePhase::Warmup;
	PhaseTime = 0.f;
	RunTime = 0.f;
	Distance = 0.f;
	FrameIndex = 0;
	HitchCount = 0;
	Lines.Reset();
	Lines.Add(PerfRoute::CsvHeader);

	Camera->SetFieldOfView(FieldOfView);
	MoveCameraTo(0.f);
	if (bUncapFrameRate)
	{
		SetFrameRateUncapped(true);
	}
	SetActorTickEnabled(true);

	UE_LOG(LogPerfRoute, Log, TEXT("Route '%s' started: %.0f m at %.0f m/s after %.1f s warm-up."),
		*RouteName.ToString(), Route->GetSplineLength() / 100.f, SpeedMetersPerSecond, WarmupSeconds);
}

void APerfRouteRunner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsRunning())
	{
		return;
	}

	// Pawn possession can take the view after we start; keep the harness camera active.
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->GetViewTarget() != this)
		{
			PC->SetViewTarget(this);
		}
	}

	// Record the frame that just finished, tagged with the phase it was rendered in.
	RecordFrame();

	PhaseTime += DeltaSeconds;
	RunTime += DeltaSeconds;

	switch (Phase)
	{
	case EPerfRoutePhase::Warmup:
		if (PhaseTime >= WarmupSeconds)
		{
			Phase = EPerfRoutePhase::Route;
			PhaseTime = 0.f;
			if (bCaptureCsvProfile && GEngine)
			{
				GEngine->Exec(GetWorld(), TEXT("CsvProfile Start"));
			}
		}
		break;

	case EPerfRoutePhase::Route:
		Distance += SpeedMetersPerSecond * 100.f * DeltaSeconds;
		if (Distance >= Route->GetSplineLength())
		{
			Distance = Route->GetSplineLength();
			Phase = EPerfRoutePhase::Hold;
			PhaseTime = 0.f;
		}
		MoveCameraTo(Distance);
		break;

	case EPerfRoutePhase::Hold:
		if (PhaseTime >= HoldAtEndSeconds)
		{
			FinishRun();
		}
		break;

	default:
		break;
	}
}

void APerfRouteRunner::MoveCameraTo(float InDistance)
{
	const FVector Location = Route->GetLocationAtDistanceAlongSpline(InDistance, ESplineCoordinateSpace::World);
	const FRotator Rotation = Route->GetRotationAtDistanceAlongSpline(InDistance, ESplineCoordinateSpace::World);
	Camera->SetWorldLocationAndRotation(Location, Rotation);
}

void APerfRouteRunner::RecordFrame()
{
	// Same sources as "stat unit". Thread and GPU times lag the frame time by one or two frames,
	// which does not matter for averages and percentiles over a whole route.
	const float FrameMs = static_cast<float>(FApp::GetDeltaTime() * 1000.0);
	const float GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	const float RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	const float GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
	const double RamMb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);

	const TCHAR* PhaseName = Phase == EPerfRoutePhase::Warmup ? TEXT("warmup") : (Phase == EPerfRoutePhase::Route ? TEXT("route") : TEXT("hold"));

	if (Phase != EPerfRoutePhase::Warmup && FrameMs > PerfRoute::HitchThresholdMs)
	{
		++HitchCount;
		UE_LOG(LogPerfRoute, Warning, TEXT("Hitch: %.1f ms at %.0f m along route '%s'."), FrameMs, Distance / 100.f, *RouteName.ToString());
	}

	Lines.Add(FString::Printf(TEXT("%d,%.3f,%s,%.1f,%.2f,%.2f,%.2f,%.2f,%.0f"),
		FrameIndex++, RunTime, PhaseName, Distance / 100.f, FrameMs, GameMs, RenderMs, GpuMs, RamMb));
}

void APerfRouteRunner::FinishRun()
{
	const bool bReachedRoute = Phase == EPerfRoutePhase::Route || Phase == EPerfRoutePhase::Hold;
	Phase = EPerfRoutePhase::Done;
	SetActorTickEnabled(false);

	if (bCaptureCsvProfile && bReachedRoute && GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("CsvProfile Stop"));
	}
	if (bUncapFrameRate)
	{
		SetFrameRateUncapped(false);
	}

	Lines.Insert(BuildMetadataLine(), 0);

	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Perf");
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Path = FPaths::ConvertRelativePathToFull(Dir / FString::Printf(TEXT("PerfRoute_%s_%s.csv"),
		*RouteName.ToString(), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));

	if (FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")) + TEXT("\n"), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogPerfRoute, Log, TEXT("Route '%s' finished: %d frames, %d hitches over %.0f ms. Wrote %s"),
			*RouteName.ToString(), FrameIndex, HitchCount, PerfRoute::HitchThresholdMs, *Path);
	}
	else
	{
		UE_LOG(LogPerfRoute, Error, TEXT("Route '%s' finished but could not write %s"), *RouteName.ToString(), *Path);
	}
	Lines.Empty();

	if (bQuitWhenDone && !GetWorld()->IsPlayInEditor())
	{
		FPlatformMisc::RequestExit(false);
	}
}

void APerfRouteRunner::SetFrameRateUncapped(bool bUncapped)
{
	if (bUncapped)
	{
		SavedMaxFPS = PerfRoute::GetCVarString(TEXT("t.MaxFPS"));
		SavedVSync = PerfRoute::GetCVarString(TEXT("r.VSync"));
		PerfRoute::SetCVarString(TEXT("t.MaxFPS"), TEXT("0"));
		PerfRoute::SetCVarString(TEXT("r.VSync"), TEXT("0"));
	}
	else
	{
		PerfRoute::SetCVarString(TEXT("t.MaxFPS"), SavedMaxFPS);
		PerfRoute::SetCVarString(TEXT("r.VSync"), SavedVSync);
	}
}

FString APerfRouteRunner::BuildMetadataLine() const
{
	FIntPoint Resolution(0, 0);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		Resolution = GEngine->GameViewport->Viewport->GetSizeXY();
	}

	TArray<FString> Pairs;
	auto Add = [&Pairs](const TCHAR* Key, const FString& Value)
	{
		Pairs.Add(FString::Printf(TEXT("%s=%s"), Key, *PerfRoute::CleanMetadataValue(Value)));
	};

	Add(TEXT("route"), RouteName.ToString());
	Add(TEXT("map"), GetWorld() ? GetWorld()->GetMapName() : FString());
	Add(TEXT("speed_mps"), FString::SanitizeFloat(SpeedMetersPerSecond));
	Add(TEXT("fov"), FString::SanitizeFloat(FieldOfView));
	Add(TEXT("resolution"), FString::Printf(TEXT("%dx%d"), Resolution.X, Resolution.Y));
	Add(TEXT("screen_percentage"), PerfRoute::GetCVarString(TEXT("r.ScreenPercentage")));
	Add(TEXT("cpu"), FPlatformMisc::GetCPUBrand());
	Add(TEXT("cores"), FString::FromInt(FPlatformMisc::NumberOfCores()));
	Add(TEXT("gpu"), FPlatformMisc::GetPrimaryGPUBrand());
	Add(TEXT("ram_gb"), FString::FromInt(static_cast<int32>(FPlatformMemory::GetConstants().TotalPhysicalGB)));
	Add(TEXT("rhi"), FApp::GetGraphicsRHI());
	Add(TEXT("build"), LexToString(FApp::GetBuildConfiguration()));
	Add(TEXT("engine"), FEngineVersion::Current().ToString());
	Add(TEXT("machine"), FPlatformProcess::ComputerName());
	Add(TEXT("date"), FDateTime::Now().ToIso8601());

	return TEXT("# ") + FString::Join(Pairs, TEXT(";"));
}
