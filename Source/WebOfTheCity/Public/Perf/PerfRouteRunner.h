#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PerfRouteRunner.generated.h"

class UCameraComponent;
class USplineComponent;

enum class EPerfRoutePhase : uint8
{
	Idle,
	Warmup,
	Route,
	Hold,
	Done
};

/**
 * Performance harness route (Phase 0, Sections 4.3 and 17).
 *
 * Flies a camera along a spline at a fixed speed and writes one CSV row per frame to
 * Saved/Perf/PerfRoute_<Route>_<Timestamp>.csv. Tools/Perf/analyze_perf.py checks that file
 * against the Section 4.3 budgets and against a stored baseline.
 *
 * Start it in one of three ways:
 *   - Packaged/standalone:  WebOfTheCity.exe <MapPath> -PerfRoute=<RouteName> [-PerfRouteQuit]
 *   - Console (PIE or game): PerfRoute.Start <RouteName>
 *   - PIE: tick bAutoStartInEditor on the placed actor.
 *
 * In a World Partition map, untick "Is Spatially Loaded" on this actor so it is always present.
 */
UCLASS()
class WEBOFTHECITY_API APerfRouteRunner : public AActor
{
	GENERATED_BODY()

public:
	APerfRouteRunner();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Warm-up, then fly the route, then write the CSV. Does nothing if a run is already active. */
	UFUNCTION(BlueprintCallable, Category = "Perf")
	void StartRun();

	UFUNCTION(BlueprintPure, Category = "Perf")
	bool IsRunning() const { return Phase != EPerfRoutePhase::Idle && Phase != EPerfRoutePhase::Done; }

	/** Matched against -PerfRoute=<Name> and "PerfRoute.Start <Name>". One map may hold several routes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perf")
	FName RouteName = TEXT("Swing");

	/** Camera speed along the spline in m/s. 60 m/s is the Section 4.3 top swing speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perf", meta = (ClampMin = "0.0"))
	float SpeedMetersPerSecond = 60.f;

	/** Seconds parked at the start before moving. Rows are tagged "warmup" and not gated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perf", meta = (ClampMin = "0.0"))
	float WarmupSeconds = 5.f;

	/** Seconds parked at the end of the spline, for stationary scenes (combat arena, crowd plaza). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perf", meta = (ClampMin = "0.0"))
	float HoldAtEndSeconds = 0.f;

	/** Horizontal FOV during the run. 105 matches the top-speed swing camera (Section 11), the worst case. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perf", meta = (ClampMin = "60.0", ClampMax = "130.0"))
	float FieldOfView = 105.f;

	/** Uncap the frame rate and turn VSync off during the run, so we measure headroom rather than the cap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perf")
	bool bUncapFrameRate = true;

	/** Also run Unreal's CSV profiler (Saved/Profiling/CSV) for deep dives. Non-Shipping builds only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perf")
	bool bCaptureCsvProfile = true;

	/** Start on BeginPlay when playing in the editor. Handy while authoring a route. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perf")
	bool bAutoStartInEditor = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perf")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perf")
	TObjectPtr<USplineComponent> Route;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perf")
	TObjectPtr<UCameraComponent> Camera;

private:
	void MoveCameraTo(float InDistance);
	void RecordFrame();
	void FinishRun();
	void SetFrameRateUncapped(bool bUncapped);
	FString BuildMetadataLine() const;

	EPerfRoutePhase Phase = EPerfRoutePhase::Idle;
	float PhaseTime = 0.f;
	float RunTime = 0.f;
	float Distance = 0.f;
	int32 FrameIndex = 0;
	int32 HitchCount = 0;
	bool bQuitWhenDone = false;
	TArray<FString> Lines;

	// Console variable values from before the run, restored when it ends.
	FString SavedMaxFPS;
	FString SavedVSync;
};
