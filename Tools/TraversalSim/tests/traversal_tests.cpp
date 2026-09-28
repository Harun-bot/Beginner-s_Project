// Tests for the traversal core (Source/WebOfTheCity/.../Traversal/TraversalSim), run outside Unreal.
//
// - Unit tests: rope, assists, release timing, anchors, zip/launch, walls, vault, glide, camera.
// - Bots: swing the Phase 0 perf route through the exported greybox city, the Phase 1 Definition of
//   Done ("2 km of continuous swinging, no traversal dead ends"), with a novice and a skilled bot.
//
// Build and run with Tools/TraversalSim/run_tests.sh. Usage: traversal_tests <greybox_layout.csv> [-v]

#include "Traversal/TraversalSim.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// ------------------------------------------------------------------ tiny test framework

namespace
{
	int GChecks = 0;
	int GFailures = 0;
	bool GVerbose = false;
	std::string GLayoutPath;

	struct FTestCase
	{
		const char* Name;
		void (*Fn)();
	};

	std::vector<FTestCase>& Registry()
	{
		static std::vector<FTestCase> Tests;
		return Tests;
	}

	struct FRegister
	{
		FRegister(const char* Name, void (*Fn)()) { Registry().push_back({Name, Fn}); }
	};
}

#define TEST(Name) \
	static void Name(); \
	static FRegister Register_##Name(#Name, &Name); \
	static void Name()

#define CHECK(Cond) \
	do { ++GChecks; if (!(Cond)) { ++GFailures; std::printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #Cond); } } while (0)

#define CHECK_MSG(Cond, ...) \
	do { ++GChecks; if (!(Cond)) { ++GFailures; std::printf("    FAIL %s:%d: %s -- ", __FILE__, __LINE__, #Cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

#define LOG(...) do { if (GVerbose) { std::printf("    "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
#define INFO(...) do { std::printf("    "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

// ------------------------------------------------------------------ box world

namespace
{
	double Axis(const FVector& V, int A) { return A == 0 ? V.X : (A == 1 ? V.Y : V.Z); }

	FVector AxisVector(int A, double Sign)
	{
		return FVector(A == 0 ? Sign : 0.0, A == 1 ? Sign : 0.0, A == 2 ? Sign : 0.0);
	}

	struct FBox3
	{
		FVector Min;
		FVector Max;
		bool bAnchor = false;
	};

	/** Axis-aligned boxes with a 2D grid broadphase. The sim's traces and the host's sweeps both run here. */
	class FBoxWorld : public ITraversalWorld
	{
	public:
		static constexpr double CellSize = 50.0;

		void AddBox(const FVector& Centre, const FVector& Size, bool bAnchor = false)
		{
			FBox3 Box;
			Box.Min = Centre - Size * 0.5;
			Box.Max = Centre + Size * 0.5;
			Box.bAnchor = bAnchor;
			const int32 Index = int32(Boxes.size());
			Boxes.push_back(Box);
			if (Size.X > 500.0 || Size.Y > 500.0)
			{
				BigBoxes.push_back(Index);  // ground slab: always tested
				return;
			}
			for (int32 CX = CellOf(Box.Min.X); CX <= CellOf(Box.Max.X); ++CX)
			{
				for (int32 CY = CellOf(Box.Min.Y); CY <= CellOf(Box.Max.Y); ++CY)
				{
					Cells[Key(CX, CY)].push_back(Index);
				}
			}
		}

		bool Trace(const FVector& Start, const FVector& End, double Radius, FTraversalHit& OutHit) const override
		{
			++TraceCount;
			return Sweep(Start, End, FVector(Radius, Radius, Radius), OutHit);
		}

		/** The host's capsule sweep, approximated by a box of the capsule's extents. */
		bool SweepBody(const FVector& Start, const FVector& End, const FTraversalBody& Body, FTraversalHit& OutHit) const
		{
			return Sweep(Start, End, FVector(Body.Radius, Body.Radius, Body.HalfHeight), OutHit);
		}

		mutable int64 TraceCount = 0;

	private:
		static int32 CellOf(double V) { return int32(std::floor(V / CellSize)); }
		static int64 Key(int32 X, int32 Y) { return (int64(X) << 32) ^ int64(uint32(Y)); }

		bool Sweep(const FVector& Start, const FVector& End, const FVector& Inflate, FTraversalHit& OutHit) const
		{
			const FVector D = End - Start;
			const double Len = D.Size();
			if (Len < 1e-9)
			{
				return false;
			}

			double BestT = 2.0;
			FVector BestNormal = FVector::UpVector;
			bool bBestAnchor = false;
			++Stamp;

			auto TestBox = [&](int32 Index)
			{
				if (Stamps.size() < Boxes.size()) Stamps.resize(Boxes.size(), 0);
				if (Stamps[Index] == Stamp) return;
				Stamps[Index] = Stamp;

				const FBox3& B = Boxes[Index];
				double TEnter = -std::numeric_limits<double>::infinity();
				double TExit = std::numeric_limits<double>::infinity();
				FVector Normal = FVector::UpVector;
				for (int A = 0; A < 3; ++A)
				{
					const double S = Axis(Start, A);
					const double Dir = Axis(D, A);
					const double Lo = Axis(B.Min, A) - Axis(Inflate, A);
					const double Hi = Axis(B.Max, A) + Axis(Inflate, A);
					if (std::fabs(Dir) < 1e-12)
					{
						// Touching a face counts as outside, so resting on the ground isn't a collision.
						if (S <= Lo || S >= Hi) return;
						continue;
					}
					double T1 = (Lo - S) / Dir;
					double T2 = (Hi - S) / Dir;
					double N1 = -1.0;
					if (T1 > T2) { std::swap(T1, T2); N1 = 1.0; }
					if (T1 > TEnter) { TEnter = T1; Normal = AxisVector(A, N1); }
					TExit = std::min(TExit, T2);
					if (TEnter > TExit) return;
				}
				if (TExit <= 1e-9 || TEnter > 1.0) return;
				if (TEnter < 0.0)
				{
					// Started inside: push out through the nearest face, unless already moving out of it.
					double Best = std::numeric_limits<double>::infinity();
					for (int A = 0; A < 3; ++A)
					{
						const double S = Axis(Start, A);
						const double Lo = Axis(B.Min, A) - Axis(Inflate, A);
						const double Hi = Axis(B.Max, A) + Axis(Inflate, A);
						if (S - Lo < Best) { Best = S - Lo; Normal = AxisVector(A, -1.0); }
						if (Hi - S < Best) { Best = Hi - S; Normal = AxisVector(A, 1.0); }
					}
					if (FVector::DotProduct(D, Normal) >= 0.0) return;
					TEnter = 0.0;
				}
				if (TEnter < BestT)
				{
					BestT = TEnter;
					BestNormal = Normal;
					bBestAnchor = B.bAnchor;
				}
			};

			for (int32 Index : BigBoxes) TestBox(Index);
			const double MinX = std::min(Start.X, End.X) - Inflate.X, MaxX = std::max(Start.X, End.X) + Inflate.X;
			const double MinY = std::min(Start.Y, End.Y) - Inflate.Y, MaxY = std::max(Start.Y, End.Y) + Inflate.Y;
			for (int32 CX = CellOf(MinX); CX <= CellOf(MaxX); ++CX)
			{
				for (int32 CY = CellOf(MinY); CY <= CellOf(MaxY); ++CY)
				{
					auto It = Cells.find(Key(CX, CY));
					if (It == Cells.end()) continue;
					for (int32 Index : It->second) TestBox(Index);
				}
			}

			if (BestT > 1.0)
			{
				return false;
			}
			OutHit.Distance = BestT * Len;
			OutHit.Normal = BestNormal;
			// Report the point on the real surface, not on the inflated one.
			const FVector Centre = Start + D * BestT;
			OutHit.Location = FVector(Centre.X - BestNormal.X * Inflate.X, Centre.Y - BestNormal.Y * Inflate.Y, Centre.Z - BestNormal.Z * Inflate.Z);
			OutHit.bAnchorProp = bBestAnchor;
			return true;
		}

		std::vector<FBox3> Boxes;
		std::vector<int32> BigBoxes;
		std::unordered_map<int64, std::vector<int32>> Cells;
		mutable std::vector<uint32> Stamps;
		mutable uint32 Stamp = 0;
	};
}

// ------------------------------------------------------------------ host (what UHeroMovementComponent does in the engine)

namespace
{
	struct FHost
	{
		FTraversalTuning Tuning;
		FTraversalSim Sim;
		FTraversalBody Body;
		const FBoxWorld* World = nullptr;
		int32 Landings = 0;
		int32 WallAttaches = 0;
		FTraversalEvents Seen;  // OR of all events so far

		explicit FHost(const FBoxWorld& InWorld, const FTraversalTuning& InTuning = FTraversalTuning())
			: Tuning(InTuning), World(&InWorld)
		{
			Sim.SetTuning(&Tuning);
		}

		void StartInAir(const FVector& Position, const FVector& Velocity)
		{
			Body.Position = Position;
			Body.Velocity = Velocity;
			Sim.ResetToAir();
		}

		void StartOnGround(const FVector& FeetPosition)
		{
			Body.Position = FeetPosition + FVector::UpVector * Body.HalfHeight;
			Body.Velocity = FVector::ZeroVector;
			Sim.NotifyLanded();
		}

		void Step(double Dt, const FTraversalInput& Input)
		{
			if (Sim.GetState() == ETraversalState::Ground)
			{
				Sim.TickGround(Dt, Input, Body, *World);
				if (Sim.GetState() == ETraversalState::Ground)
				{
					// Minimal walking: run along the stick, jump on press.
					const FVector Dir = FVector(Input.Move.X, Input.Move.Y, 0.0);
					const double Speed = Input.bTraversalHeld ? Tuning.GroundSprintSpeed : Tuning.GroundRunSpeed;
					Body.Velocity = Dir * Speed;
					if (Input.bJumpPressed)
					{
						Body.Velocity.Z = Tuning.GroundJumpSpeed;
						Sim.NotifyLeftGround(true);
					}
					MoveWithCollision(Body.Velocity * Dt, true);
					Record();
					return;
				}
			}
			Sim.Tick(Dt, Input, Body, *World);
			MoveWithCollision(Body.Move, false);
			Record();
		}

		void Record()
		{
			const FTraversalEvents E = Sim.ConsumeEvents();
			Seen.bSwingStarted |= E.bSwingStarted;
			Seen.bReleased |= E.bReleased;
			Seen.bPerfectRelease |= E.bPerfectRelease;
			Seen.bTierUp |= E.bTierUp;
			Seen.bSkyAnchor |= E.bSkyAnchor;
			Seen.bAutoCatch |= E.bAutoCatch;
			Seen.bZip |= E.bZip;
			Seen.bLaunch |= E.bLaunch;
			Seen.bWallAttach |= E.bWallAttach;
			Seen.bVault |= E.bVault;
			Seen.bLanded |= E.bLanded;
			Seen.bStreakEnded |= E.bStreakEnded;
			if (E.bWallAttach) ++WallAttaches;
		}

		void MoveWithCollision(FVector Delta, bool bWalking)
		{
			for (int Iteration = 0; Iteration < 4 && Delta.SizeSquared() > 1e-12; ++Iteration)
			{
				FTraversalHit Hit;
				if (!World->SweepBody(Body.Position, Body.Position + Delta, Body, Hit))
				{
					Body.Position += Delta;
					return;
				}
				const double Len = Delta.Size();
				const FVector Travel = Delta * (std::max(0.0, Hit.Distance - 0.01) / Len);
				Body.Position += Travel;
				const bool bWalkable = Hit.Normal.Z > 0.7;
				if (bWalking)
				{
					if (bWalkable) Body.Velocity.Z = 0.0;
					Delta = FVector::VectorPlaneProject(Delta - Travel, Hit.Normal);
					continue;
				}
				switch (Sim.OnBlocked(Hit, bWalkable, Body))
				{
				case ETraversalBlockResponse::Land:
					Body.Velocity = FVector::ZeroVector;
					Sim.NotifyLanded();
					++Landings;
					return;
				case ETraversalBlockResponse::Attach:
					return;
				case ETraversalBlockResponse::Slide:
					Delta = FVector::VectorPlaneProject(Delta - Travel, Hit.Normal);
					break;
				}
			}
		}
	};

	constexpr double Dt = 1.0 / 60.0;

	FTraversalInput Held(const FVector& Look, const FVector& Move = FVector::ZeroVector)
	{
		FTraversalInput In;
		In.CameraForward = Look.GetSafeNormal();
		In.Move = Move;
		In.bTraversalHeld = true;
		return In;
	}

	FTraversalInput Free(const FVector& Look, const FVector& Move = FVector::ZeroVector)
	{
		FTraversalInput In = Held(Look, Move);
		In.bTraversalHeld = false;
		return In;
	}

	/** Ground slab plus one wide, tall wall facing -X at X = WallX. */
	FBoxWorld WallWorld(double WallX, double WallTop = 200.0)
	{
		FBoxWorld W;
		W.AddBox(FVector(0.0, 0.0, -0.5), FVector(4000.0, 4000.0, 1.0));
		W.AddBox(FVector(WallX + 10.0, 0.0, WallTop * 0.5), FVector(20.0, 200.0, WallTop));
		return W;
	}

	/** A long street canyon along +X: walls either side at Y = +/-HalfWidth, facing inward. */
	FBoxWorld CanyonWorld(double HalfWidth = 15.0, double Height = 80.0)
	{
		FBoxWorld W;
		W.AddBox(FVector(0.0, 0.0, -0.5), FVector(4000.0, 4000.0, 1.0));
		W.AddBox(FVector(500.0, HalfWidth + 10.0, Height * 0.5), FVector(1200.0, 20.0, Height));
		W.AddBox(FVector(500.0, -HalfWidth - 10.0, Height * 0.5), FVector(1200.0, 20.0, Height));
		return W;
	}

	FBoxWorld OpenWorld()
	{
		FBoxWorld W;
		W.AddBox(FVector(0.0, 0.0, -0.5), FVector(4000.0, 4000.0, 1.0));
		return W;
	}
}

// ------------------------------------------------------------------ unit tests

TEST(RopeNeverStretches)
{
	const FBoxWorld W = CanyonWorld();
	FHost H(W);
	H.StartInAir(FVector(0, 0, 20), FVector(10, 0, 0));
	int32 SwingFrames = 0;
	double WorstStretch = 0.0;
	for (int i = 0; i < 300; ++i)
	{
		H.Step(Dt, Held(FVector(1, 0, 0)));
		if (H.Sim.GetState() == ETraversalState::Swing)
		{
			++SwingFrames;
			const double Dist = FVector::Dist(H.Body.Position, H.Sim.GetSwingAnchor().Pivot);
			WorstStretch = std::max(WorstStretch, Dist - H.Sim.GetRopeLength());
		}
	}
	CHECK(SwingFrames > 20);
	CHECK_MSG(WorstStretch < 1e-6, "rope stretched by %.6f m", WorstStretch);
}

TEST(SwingNeverStallsBelowMinSpeed)
{
	const FBoxWorld W = CanyonWorld();
	FHost H(W);
	H.StartInAir(FVector(0, 0, 20), FVector(0, 0, 0));
	double Slowest = 1e9;
	for (int i = 0; i < 240; ++i)
	{
		H.Step(Dt, Held(FVector(1, 0, 0)));
		if (H.Sim.GetState() == ETraversalState::Swing && H.Sim.GetStateTime() > 0.0)
		{
			const FVector R = (H.Body.Position - H.Sim.GetSwingAnchor().Pivot).GetSafeNormal();
			const FVector Tangent = H.Body.Velocity - R * FVector::DotProduct(H.Body.Velocity, R);
			Slowest = std::min(Slowest, Tangent.Size());
		}
	}
	CHECK(Slowest < 1e8);
	// The floor is applied before the rope projection, which can shave a hair off.
	CHECK_MSG(Slowest >= H.Tuning.SwingMinSpeed - 0.05, "slowest tangential speed %.3f", Slowest);
}

TEST(AnchorPrefersIdealElevation)
{
	const FBoxWorld W = WallWorld(40.0);
	FHost H(W);
	H.StartInAir(FVector(0, 0, 10), FVector(5, 0, 0));
	H.Step(Dt, Free(FVector(1, 0, 0)));
	const FTraversalAnchor& A = H.Sim.GetPreviewAnchor();
	CHECK(A.bValid);
	const FVector Origin = H.Body.Position + FVector::UpVector * (H.Body.HalfHeight * 0.6);
	const FVector To = A.SurfacePoint - Origin;
	const double Elevation = FMath::RadiansToDegrees(FMath::Asin(To.Z / To.Size()));
	LOG("preview anchor elevation %.1f deg, distance %.1f m", Elevation, To.Size());
	CHECK_MSG(Elevation >= 30.0 && Elevation <= 60.0, "elevation %.1f", Elevation);
	// The pivot sits out from the wall so the arc doesn't scrape it.
	CHECK(A.Pivot.X < A.SurfacePoint.X - 1.0);
}

TEST(AnchorPropBonusBreaksTies)
{
	// Two identical walls either side; only the right one is an anchor prop.
	FBoxWorld W;
	W.AddBox(FVector(0.0, 0.0, -0.5), FVector(4000.0, 4000.0, 1.0));
	W.AddBox(FVector(30.0, 25.0, 50.0), FVector(20.0, 10.0, 100.0), true);
	W.AddBox(FVector(30.0, -25.0, 50.0), FVector(20.0, 10.0, 100.0), false);
	FHost H(W);
	H.StartInAir(FVector(0, 0, 15), FVector(5, 0, 0));
	H.Step(Dt, Free(FVector(1, 0, 0)));
	CHECK(H.Sim.GetPreviewAnchor().bValid);
	CHECK(H.Sim.GetPreviewAnchor().SurfacePoint.Y > 0.0);
}

TEST(PerfectReleaseBoostsAndCountsAsChain)
{
	for (int Case = 0; Case < 2; ++Case)
	{
		const bool bInWindow = Case == 0;
		const FBoxWorld W = CanyonWorld();
		FHost H(W);
		H.StartInAir(FVector(0, 0, 30), FVector(15, 0, 0));
		bool bReleased = false;
		double SpeedBefore = 0.0;
		for (int i = 0; i < 400 && !bReleased; ++i)
		{
			bool bHold = true;
			if (H.Sim.GetState() == ETraversalState::Swing)
			{
				const double Angle = H.Sim.GetSwingAngle(H.Body.Position);
				const bool bLetGo = bInWindow ? (Angle > 5.0 && Angle < 25.0) : (Angle < -20.0 && H.Sim.GetStateTime() > 0.1);
				if (bLetGo)
				{
					bHold = false;
					SpeedBefore = H.Body.Velocity.Size();
				}
			}
			H.Step(Dt, bHold ? Held(FVector(1, 0, 0)) : Free(FVector(1, 0, 0)));
			bReleased = !bHold;
		}
		CHECK(bReleased);
		CHECK(H.Seen.bPerfectRelease == bInWindow);
		if (bInWindow)
		{
			CHECK_MSG(H.Body.Velocity.Size() > SpeedBefore + H.Tuning.ReleaseBoostSpeed * 0.5,
				"speed %.2f -> %.2f", SpeedBefore, H.Body.Velocity.Size());
			CHECK(H.Sim.GetChainCount() == 1 + H.Tuning.PerfectReleaseChainBonus);
		}
	}
}

TEST(SkyAnchorIsLimitedFallbackInOpenSpace)
{
	const FBoxWorld W = OpenWorld();
	FHost H(W);
	H.StartInAir(FVector(0, 0, 25), FVector(15, 0, 0));
	int32 Starts = 0;
	for (int i = 0; i < 1800 && H.Landings == 0; ++i)
	{
		H.Step(Dt, Held(FVector(1, 0, 0)));
		if (H.Sim.GetState() == ETraversalState::Swing && H.Sim.GetStateTime() == 0.0)
		{
			++Starts;
			CHECK(H.Sim.GetSwingAnchor().bSky);
		}
	}
	CHECK(H.Seen.bSkyAnchor);
	CHECK_MSG(Starts == H.Tuning.SkyAnchorMaxInARow, "%d sky swings", Starts);
	CHECK(H.Landings == 1);
	CHECK(H.Sim.GetSkyAnchorsLeft() == H.Tuning.SkyAnchorMaxInARow);  // refilled on landing
}

TEST(AutoWebCatchBeforeImpact)
{
	for (int Case = 0; Case < 2; ++Case)
	{
		const FBoxWorld W = WallWorld(30.0);
		FTraversalTuning T;
		T.bAutoWebCatch = Case == 0;
		FHost H(W, T);
		H.StartInAir(FVector(0, 0, 60), FVector(0, 0, -30));
		for (int i = 0; i < 180 && H.Landings == 0 && !H.Seen.bAutoCatch; ++i)
		{
			H.Step(Dt, Free(FVector(1, 0, 0)));
		}
		if (T.bAutoWebCatch)
		{
			CHECK(H.Seen.bAutoCatch);
			CHECK(H.Landings == 0);
		}
		else
		{
			CHECK(!H.Seen.bAutoCatch);
			CHECK(H.Landings == 1);
		}
	}
}

TEST(ZipToWallClampsDurationAndClings)
{
	const FBoxWorld W = WallWorld(20.0);
	FHost H(W);
	H.StartInAir(FVector(0, 0, 10), FVector(0, 0, 0));
	H.Step(Dt, Free(FVector(1, 0, 0)));  // settle one frame of air time
	FTraversalInput Jump = Free(FVector(1, 0, 0));
	Jump.bJumpPressed = true;
	H.Step(Dt, Jump);
	CHECK(H.Sim.GetState() == ETraversalState::Zip);
	double ZipTime = Dt;
	while (H.Sim.GetState() == ETraversalState::Zip && ZipTime < 2.0)
	{
		H.Step(Dt, Free(FVector(1, 0, 0)));
		ZipTime += Dt;
	}
	LOG("zip took %.3f s", ZipTime);
	CHECK(ZipTime >= H.Tuning.ZipMinDuration - Dt && ZipTime <= H.Tuning.ZipMaxDuration + 2 * Dt);
	CHECK(H.Sim.GetState() == ETraversalState::WallCling);
	CHECK(std::fabs(H.Body.Position.X - (20.0 - H.Body.Radius)) < 0.3);
}

TEST(ZipToLedgeThenPointLaunch)
{
	const FBoxWorld W = WallWorld(20.0, 12.0);
	FHost H(W);
	H.StartInAir(FVector(0, 0, 10), FVector(0, 0, 0));
	H.Step(Dt, Free(FVector(1, 0, 0)));
	// Aim just below the top of the wall.
	const FVector Look = FVector(20.0, 0.0, 11.3) - (H.Body.Position + FVector::UpVector * (H.Body.HalfHeight * 0.6));
	FTraversalInput Jump = Free(Look);
	Jump.bJumpPressed = true;
	H.Step(Dt, Jump);
	CHECK(H.Sim.GetState() == ETraversalState::Zip);
	CHECK(H.Sim.GetZipTarget().Z > 12.0);
	for (int i = 0; i < 60 && H.Sim.GetState() == ETraversalState::Zip && H.Body.Velocity.SizeSquared() > 0.0; ++i)
	{
		H.Step(Dt, Free(Look));
	}
	// Perched: press jump inside the window.
	CHECK(H.Sim.GetState() == ETraversalState::Zip);
	H.Step(Dt, Jump);
	CHECK(H.Seen.bLaunch);
	CHECK(H.Sim.GetState() == ETraversalState::Launch);
	CHECK(std::fabs(H.Body.Velocity.Z - H.Tuning.LaunchUpSpeed) < 1.0);
	CHECK(H.Body.Velocity.X > H.Tuning.LaunchForwardSpeed * 0.9);
}

TEST(HeadOnWallSticksGlancingWallSlides)
{
	{
		const FBoxWorld W = WallWorld(10.0);
		FHost H(W);
		H.StartInAir(FVector(8, 0, 20), FVector(20, 0, 0));
		for (int i = 0; i < 10; ++i) H.Step(Dt, Free(FVector(1, 0, 0)));
		CHECK(H.Sim.GetState() == ETraversalState::WallCling);
	}
	{
		const FBoxWorld W = WallWorld(10.0);
		FHost H(W);
		H.StartInAir(FVector(9.4, 0, 20), FVector(3, 20, 0));
		for (int i = 0; i < 8; ++i) H.Step(Dt, Free(FVector(0, 1, 0)));
		CHECK(H.Sim.GetState() == ETraversalState::Air);
		CHECK(H.Body.Velocity.Y > 15.0);
		CHECK(H.Body.Velocity.X <= 1e-6);
	}
}

TEST(SprintIntoWallRunsUpAndVaultsOntoRoof)
{
	const FBoxWorld W = WallWorld(10.0, 15.0);
	FHost H(W);
	H.StartOnGround(FVector(9.0, 0, 0));
	double T = 0.0;
	bool bSawWallRun = false;
	for (; T < 4.0; T += Dt)
	{
		H.Step(Dt, Held(FVector(1, 0, 0), FVector(1, 0, 0)));
		bSawWallRun |= H.Sim.GetState() == ETraversalState::WallRun;
		if (H.Sim.GetState() == ETraversalState::Ground && H.Body.Position.Z > 10.0) break;
	}
	LOG("reached the roof after %.2f s at z=%.2f", T, H.Body.Position.Z);
	CHECK(bSawWallRun);
	CHECK(H.Seen.bVault);
	CHECK(H.Sim.GetState() == ETraversalState::Ground);
	CHECK(std::fabs(H.Body.Position.Z - (15.0 + H.Body.HalfHeight)) < 0.1);
}

TEST(SprintIntoLowObstacleVaults)
{
	const FBoxWorld W = WallWorld(10.0, 1.5);
	FHost H(W);
	H.StartOnGround(FVector(9.0, 0, 0));
	for (int i = 0; i < 120 && !H.Seen.bVault; ++i)
	{
		H.Step(Dt, Held(FVector(1, 0, 0), FVector(1, 0, 0)));
	}
	CHECK(H.Seen.bVault);
	CHECK(!H.Seen.bWallAttach);
}

TEST(WallClingCrawlAndJumpOff)
{
	const FBoxWorld W = WallWorld(10.0);
	FHost H(W);
	H.StartInAir(FVector(8, 0, 20), FVector(20, 0, 0));
	for (int i = 0; i < 10; ++i) H.Step(Dt, Free(FVector(1, 0, 0)));
	CHECK(H.Sim.GetState() == ETraversalState::WallCling);
	const double Z0 = H.Body.Position.Z;
	for (int i = 0; i < 60; ++i) H.Step(Dt, Free(FVector(1, 0, 0)));  // no input: cling
	CHECK(std::fabs(H.Body.Position.Z - Z0) < 0.05);
	for (int i = 0; i < 60; ++i) H.Step(Dt, Free(FVector(1, 0, 0), FVector(1, 0, 0)));  // push into wall: crawl up
	CHECK(std::fabs(H.Body.Position.Z - Z0 - H.Tuning.WallCrawlSpeed) < 0.3);
	FTraversalInput Jump = Free(FVector(-1, 0, 0));
	Jump.bJumpPressed = true;
	H.Step(Dt, Jump);
	CHECK(H.Sim.GetState() == ETraversalState::Air);
	CHECK(H.Body.Velocity.X < -H.Tuning.WallJumpOutSpeed * 0.9);
}

TEST(GlideCapsSinkRateAndTurnRate)
{
	const FBoxWorld W = OpenWorld();
	FHost H(W);
	H.StartInAir(FVector(0, 0, 200), FVector(20, 0, -20));
	FTraversalInput Toggle = Free(FVector(1, 0, 0));
	Toggle.bGlideTogglePressed = true;
	H.Step(Dt, Toggle);
	CHECK(H.Sim.GetState() == ETraversalState::Glide);
	for (int i = 0; i < 180; ++i) H.Step(Dt, Free(FVector(1, 0, 0)));
	CHECK(H.Sim.GetState() == ETraversalState::Glide);
	CHECK(H.Body.Velocity.Z >= -H.Tuning.GlideSinkRate - 1e-6);
	CHECK(std::fabs(H.Body.Velocity.Size2D() - H.Tuning.GlideSpeed) < 1.0);
	// Hard right for half a second: heading changes by at most the turn rate.
	const double Before = std::atan2(H.Body.Velocity.Y, H.Body.Velocity.X);
	for (int i = 0; i < 30; ++i) H.Step(Dt, Free(FVector(1, 0, 0), FVector(0, 1, 0)));
	const double TurnDeg = FMath::RadiansToDegrees(std::atan2(H.Body.Velocity.Y, H.Body.Velocity.X) - Before);
	CHECK_MSG(TurnDeg > 0.0 && TurnDeg <= H.Tuning.GlideTurnRate * 0.5 + 1.0, "turned %.1f deg", TurnDeg);
	H.Step(Dt, Toggle);
	CHECK(H.Sim.GetState() == ETraversalState::Air);
}

TEST(CoyoteJumpThenZip)
{
	const FBoxWorld W = OpenWorld();
	{
		FHost H(W);
		H.StartOnGround(FVector(0, 0, 10));  // standing "on" nothing: walks off immediately
		H.Body.Position.Z = 10.0;
		H.Sim.NotifyLeftGround(false);
		for (int i = 0; i < 4; ++i) H.Step(Dt, Free(FVector(1, 0, 0)));
		FTraversalInput Jump = Free(FVector(1, 0, 0));
		Jump.bJumpPressed = true;
		H.Step(Dt, Jump);
		CHECK(H.Sim.GetState() == ETraversalState::Air);
		CHECK(H.Body.Velocity.Z > H.Tuning.GroundJumpSpeed - 1.0);
	}
	{
		FHost H(W);
		H.Body.Position = FVector(0, 0, 30);
		H.Sim.NotifyLanded();
		H.Sim.NotifyLeftGround(false);
		for (int i = 0; i < 30; ++i) H.Step(Dt, Free(FVector(1, 0, 0)));
		FTraversalInput Jump = Free(FVector(1, 0, 0));
		Jump.bJumpPressed = true;
		H.Step(Dt, Jump);
		CHECK(H.Sim.GetState() == ETraversalState::Zip);
	}
}

TEST(CameraFovScalesWithSpeed)
{
	const FTraversalTuning T;
	CHECK(std::fabs(FTraversalSim::ComputeCameraTarget(T, FVector(0, 0, 0)).Fov - T.CameraFovBase) < 1e-9);
	CHECK(std::fabs(FTraversalSim::ComputeCameraTarget(T, FVector(100, 0, 0)).Fov - T.CameraFovMax) < 1e-9);
	double Previous = 0.0;
	for (double Speed = 0.0; Speed <= 80.0; Speed += 1.0)
	{
		const double Fov = FTraversalSim::ComputeCameraTarget(T, FVector(Speed, 0, 0)).Fov;
		CHECK(Fov >= Previous);
		Previous = Fov;
	}
	CHECK(FTraversalSim::ComputeCameraTarget(T, FVector(70, 0, 0)).ArmLength > T.CameraArmBase);
}

// ------------------------------------------------------------------ greybox city + bots

namespace
{
	struct FLayout
	{
		FBoxWorld World;
		std::vector<FVector> Route;
		std::vector<FVector> Stations;
		FVector Start = FVector::ZeroVector;
		bool bLoaded = false;
	};

	const FLayout& Layout()
	{
		static FLayout L;
		static bool bTried = false;
		if (bTried) return L;
		bTried = true;
		std::ifstream In(GLayoutPath);
		std::string Line;
		while (std::getline(In, Line))
		{
			if (Line.empty() || Line[0] == '#' || Line.rfind("type,", 0) == 0) continue;
			std::stringstream SS(Line);
			std::string Type, Kind, Cell;
			std::getline(SS, Type, ',');
			std::getline(SS, Kind, ',');
			double V[7] = {};
			for (double& Value : V)
			{
				std::getline(SS, Cell, ',');
				Value = std::atof(Cell.c_str());
			}
			const FVector P(V[0], V[1], V[2]);
			if (Type == "box") L.World.AddBox(P, FVector(V[3], V[4], V[5]), V[6] != 0.0);
			else if (Type == "route") L.Route.push_back(P);
			else if (Type == "station") L.Stations.push_back(P);
			else if (Type == "start") L.Start = P;
		}
		L.bLoaded = !L.Route.empty();
		return L;
	}

	/** Polyline with arc-length lookups. */
	struct FRoute
	{
		std::vector<FVector> Points;
		std::vector<double> Cumulative;

		explicit FRoute(const std::vector<FVector>& InPoints)
		{
			for (const FVector& P : InPoints) Points.push_back(FVector(P.X, P.Y, 0.0));
			Cumulative.push_back(0.0);
			for (size_t i = 1; i < Points.size(); ++i) Cumulative.push_back(Cumulative.back() + FVector::Dist(Points[i - 1], Points[i]));
		}

		double Length() const { return Cumulative.back(); }

		/** Distance along the route of the closest point, searching near Hint. */
		double Project(const FVector& P, double Hint) const
		{
			const FVector Q(P.X, P.Y, 0.0);
			double Best = Hint, BestDist = 1e18;
			for (size_t i = 1; i < Points.size(); ++i)
			{
				if (Cumulative[i] < Hint - 150.0 || Cumulative[i - 1] > Hint + 150.0) continue;
				const FVector A = Points[i - 1], B = Points[i];
				const FVector AB = B - A;
				const double T = FMath::Clamp(FVector::DotProduct(Q - A, AB) / std::max(1e-9, AB.SizeSquared()), 0.0, 1.0);
				const double D = FVector::Dist(Q, A + AB * T);
				if (D < BestDist) { BestDist = D; Best = Cumulative[i - 1] + T * (Cumulative[i] - Cumulative[i - 1]); }
			}
			return Best;
		}

		FVector PointAt(double S) const
		{
			S = FMath::Clamp(S, 0.0, Length());
			for (size_t i = 1; i < Points.size(); ++i)
			{
				if (Cumulative[i] >= S)
				{
					const double T = (S - Cumulative[i - 1]) / std::max(1e-9, Cumulative[i] - Cumulative[i - 1]);
					return Points[i - 1] + (Points[i] - Points[i - 1]) * T;
				}
			}
			return Points.back();
		}
	};

	struct FBotResult
	{
		double Progress = 0.0;
		double Time = 0.0;
		int32 Landings = 0;
		FTraversalStreak Streak;
		double TracesPerFrame = 0.0;
	};

	/**
	 * Follows the route from the ground: running jump, then holds swing the whole way.
	 * The skilled bot also lets go inside the perfect-release window.
	 */
	FBotResult RunRouteBot(bool bSkilled, double MaxTime)
	{
		const FLayout& L = Layout();
		FRoute Route(L.Route);
		FHost H(L.World);
		H.StartOnGround(L.Start);
		const int64 TracesBefore = L.World.TraceCount;

		FBotResult R;
		double Hint = 0.0;
		int64 Frames = 0;
		for (double T = 0.0; T < MaxTime; T += Dt, ++Frames)
		{
			Hint = std::max(Hint, Route.Project(H.Body.Position, Hint));
			R.Progress = Hint;
			if (Hint >= Route.Length() - 5.0) break;

			const FVector Target = Route.PointAt(Hint + 60.0);
			const FVector Dir = FVector(Target.X - H.Body.Position.X, Target.Y - H.Body.Position.Y, 0.0).GetSafeNormal();
			FTraversalInput In = Held(Dir, Dir);
			In.bJumpPressed = T == 0.0;
			if (bSkilled && H.Sim.GetState() == ETraversalState::Swing && H.Body.Position.Z < 45.0)
			{
				const double Angle = H.Sim.GetSwingAngle(H.Body.Position);
				if (Angle > H.Tuning.ReleaseWindowMinAngle + 10.0 && Angle < H.Tuning.ReleaseWindowMaxAngle - 5.0)
				{
					In.bTraversalHeld = false;
				}
			}
			H.Step(Dt, In);
			R.Time = T;
			if (H.Landings > 0 && T > 0.5) break;
		}
		R.Landings = H.Landings;
		R.Streak = H.Landings > 0 ? H.Sim.GetLastStreak() : H.Sim.GetStreak();
		R.TracesPerFrame = double(L.World.TraceCount - TracesBefore) / double(std::max<int64>(1, Frames));
		return R;
	}
}

TEST(GreyboxLayoutLoads)
{
	const FLayout& L = Layout();
	CHECK_MSG(L.bLoaded, "could not read %s", GLayoutPath.c_str());
	CHECK(L.Route.size() > 50);
	CHECK(L.Stations.size() == 4);
}

TEST(NoviceBotSwings2kmWithoutTouchingDown)
{
	if (!Layout().bLoaded) { CHECK(false); return; }
	const FBotResult R = RunRouteBot(false, 240.0);
	INFO("novice: %.0f m of route in %.1f s, landings %d, swings %d, speed max %.1f min %.1f m/s, max tier %d, %.1f traces/frame",
		R.Progress, R.Time, R.Landings, R.Streak.Swings, R.Streak.MaxSpeed, R.Streak.MinSpeed, R.Streak.MaxTier + 1, R.TracesPerFrame);
	CHECK_MSG(R.Progress >= 2000.0, "only %.0f m before touching down", R.Progress);
	CHECK(R.TracesPerFrame < 60.0);
}

TEST(SkilledBotBuildsSpeedTiers)
{
	if (!Layout().bLoaded) { CHECK(false); return; }
	const FBotResult R = RunRouteBot(true, 240.0);
	INFO("skilled: %.0f m of route in %.1f s, landings %d, swings %d, perfect releases %d, speed max %.1f m/s, max tier %d",
		R.Progress, R.Time, R.Landings, R.Streak.Swings, R.Streak.PerfectReleases, R.Streak.MaxSpeed, R.Streak.MaxTier + 1);
	CHECK_MSG(R.Progress >= 2000.0, "only %.0f m before touching down", R.Progress);
	CHECK(R.Streak.PerfectReleases > 5);
	CHECK(R.Streak.MaxTier == 2);
}

TEST(NoDeadEndsFromRandomStreetStarts)
{
	if (!Layout().bLoaded) { CHECK(false); return; }
	const FLayout& L = Layout();
	// Starts on avenue and street centre lines (same numbers as make_greybox_city.py), heading into town.
	const double BlockX = 120.0, BlockY = 70.0, Avenue = 30.0, Street = 20.0;
	const double ExtentX = 12 * (BlockX + Avenue), ExtentY = 20 * (BlockY + Street);
	std::srand(1962);
	int32 Runs = 0, Clean = 0;
	for (int Sample = 0; Sample < 40; ++Sample)
	{
		const bool bOnAvenue = Sample % 2 == 0;
		FVector Start, Dir;
		if (bOnAvenue)
		{
			const int I = std::rand() % 11;
			const double X = I * (BlockX + Avenue) + BlockX + Avenue / 2;
			const double Y = 100.0 + (std::rand() % 1000) / 1000.0 * (ExtentY - 200.0);
			Start = FVector(X, Y, 20.0);
			Dir = FVector(0, Y < ExtentY / 2 ? 1.0 : -1.0, 0);
		}
		else
		{
			const int J = std::rand() % 19;
			const double Y = J * (BlockY + Street) + BlockY + Street / 2;
			const double X = 100.0 + (std::rand() % 1000) / 1000.0 * (ExtentX - 200.0);
			Start = FVector(X, Y, 20.0);
			Dir = FVector(X < ExtentX / 2 ? 1.0 : -1.0, 0, 0);
		}
		FHost H(L.World);
		H.StartInAir(Start, Dir * 15.0);
		double Travelled = 0.0;
		for (double T = 0.0; T < 20.0 && H.Landings == 0; T += Dt)
		{
			// Keep to the street: steer back onto the start line.
			FVector Aim = Dir * 60.0;
			if (bOnAvenue) Aim.X = Start.X - H.Body.Position.X; else Aim.Y = Start.Y - H.Body.Position.Y;
			const FVector Look = Aim.GetSafeNormal();
			const FVector Before = H.Body.Position;
			H.Step(Dt, Held(Look, Look));
			Travelled += FVector(H.Body.Position.X - Before.X, H.Body.Position.Y - Before.Y, 0.0).Size();
			// Ran out of city: that's a clean run, not a dead end.
			if (H.Body.Position.X < 20 || H.Body.Position.Y < 20 || H.Body.Position.X > ExtentX - 20 || H.Body.Position.Y > ExtentY - 20) break;
		}
		// Parks are open by design (few anchors, calm pacing): coming down in one isn't a dead end.
		const bool bInPark = H.Body.Position.X > 5 * (BlockX + Avenue) && H.Body.Position.X < 7 * (BlockX + Avenue) + BlockX
			&& H.Body.Position.Y > 6 * (BlockY + Street) && H.Body.Position.Y < 9 * (BlockY + Street) + BlockY;
		++Runs;
		if (H.Landings == 0 || bInPark) ++Clean;
		else LOG("landed after %.0f m starting at (%.0f, %.0f) heading (%.0f, %.0f)", Travelled, Start.X, Start.Y, Dir.X, Dir.Y);
	}
	INFO("dead-end scan: %d of %d random street starts swung 20 s (or out of town) without touching down", Clean, Runs);
	CHECK_MSG(Clean == Runs, "%d runs touched down", Runs - Clean);
}

TEST(ParkIsNeverADeadEnd)
{
	if (!Layout().bLoaded) { CHECK(false); return; }
	const FLayout& L = Layout();
	// Middle of the park (blocks 5-7 x 6-9), low, holding swing.
	FHost H(L.World);
	H.StartInAir(FVector(5 * 150.0 + 210.0, 6 * 90.0 + 170.0, 12.0), FVector(10, 0, 0));
	double T = 0.0;
	for (; T < 1.0 && H.Sim.GetState() != ETraversalState::Swing; T += Dt)
	{
		H.Step(Dt, Held(FVector(1, 0, 0), FVector(1, 0, 0)));
	}
	LOG("park: swing after %.2f s (%s anchor)", T, H.Sim.GetSwingAnchor().bSky ? "sky" : "real");
	CHECK(H.Sim.GetState() == ETraversalState::Swing);
	CHECK(T <= H.Tuning.SkyAnchorDelay + H.Tuning.SwingMinAirTime + 0.1);
}

// ------------------------------------------------------------------ main

int main(int argc, char** argv)
{
	for (int i = 1; i < argc; ++i)
	{
		if (std::strcmp(argv[i], "-v") == 0) GVerbose = true;
		else GLayoutPath = argv[i];
	}
	int Failed = 0;
	for (const FTestCase& Test : Registry())
	{
		const int Before = GFailures;
		std::printf("%s\n", Test.Name);
		Test.Fn();
		if (GFailures != Before) ++Failed;
	}
	std::printf("\n%zu tests, %d checks, %d failed checks, %d failed tests\n", Registry().size(), GChecks, GFailures, Failed);
	return GFailures == 0 ? 0 : 1;
}
