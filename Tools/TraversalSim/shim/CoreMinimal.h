#pragma once

// Minimal stand-ins for the Unreal types the traversal core uses, so Source/.../Traversal/TraversalSim
// compiles and runs outside the engine. Semantics follow UE 5.x (FVector is TVector<double>).
// Kept deliberately strict: FMath templates take one type, like UE's, so float/double mixing that
// would break the engine build breaks this build too.

#include <cmath>
#include <cstdint>
#include <type_traits>

using int32 = std::int32_t;
using int64 = std::int64_t;
using uint8 = std::uint8_t;
using uint32 = std::uint32_t;
using TCHAR = char;

#define TEXT(x) x
#define UENUM(...)
#define USTRUCT(...)
#define UCLASS(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UMETA(...)
#define GENERATED_BODY()
#define WEBOFTHECITY_API

#define UE_SMALL_NUMBER (1.e-8)
#define UE_KINDA_SMALL_NUMBER (1.e-4)
#define UE_DOUBLE_PI (3.141592653589793238462643383279502884197169399)

struct FMath
{
	template <typename T> static constexpr T Clamp(const T X, const T MinValue, const T MaxValue) { return X < MinValue ? MinValue : (X < MaxValue ? X : MaxValue); }
	template <typename T> static constexpr T Max(const T A, const T B) { return A >= B ? A : B; }
	template <typename T> static constexpr T Min(const T A, const T B) { return A <= B ? A : B; }
	template <typename T> static constexpr T Abs(const T A) { return A >= T(0) ? A : -A; }
	template <typename T> static constexpr T Square(const T A) { return A * A; }
	template <typename T, typename U> static constexpr T Lerp(const T& A, const T& B, const U& Alpha) { return (T)(A + Alpha * (B - A)); }
	template <typename T> static constexpr auto DegreesToRadians(T const& DegVal) { return DegVal * (UE_DOUBLE_PI / 180.0); }
	template <typename T> static constexpr auto RadiansToDegrees(T const& RadVal) { return RadVal * (180.0 / UE_DOUBLE_PI); }
	static double Sqrt(double V) { return std::sqrt(V); }
	static double Sin(double V) { return std::sin(V); }
	static double Cos(double V) { return std::cos(V); }
	static double Asin(double V) { return std::asin(V); }
	static double Acos(double V) { return std::acos(V); }
	static double Atan2(double Y, double X) { return std::atan2(Y, X); }
	static bool IsNearlyZero(double Value, double ErrorTolerance = UE_SMALL_NUMBER) { return Abs(Value) <= ErrorTolerance; }
};

struct FVector
{
	double X, Y, Z;

	// Like UE, the default constructor leaves the components uninitialized.
	FVector() {}
	constexpr FVector(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
	explicit constexpr FVector(double InF) : X(InF), Y(InF), Z(InF) {}

	static const FVector ZeroVector;
	static const FVector OneVector;
	static const FVector UpVector;
	static const FVector ForwardVector;
	static const FVector RightVector;

	FVector operator+(const FVector& V) const { return FVector(X + V.X, Y + V.Y, Z + V.Z); }
	FVector operator-(const FVector& V) const { return FVector(X - V.X, Y - V.Y, Z - V.Z); }
	FVector operator-() const { return FVector(-X, -Y, -Z); }
	template <typename FArg, typename = std::enable_if_t<std::is_arithmetic_v<FArg>>>
	FVector operator*(FArg Scale) const { return FVector(X * Scale, Y * Scale, Z * Scale); }
	template <typename FArg, typename = std::enable_if_t<std::is_arithmetic_v<FArg>>>
	FVector operator/(FArg Scale) const { const double R = 1.0 / Scale; return FVector(X * R, Y * R, Z * R); }
	FVector& operator+=(const FVector& V) { X += V.X; Y += V.Y; Z += V.Z; return *this; }
	FVector& operator-=(const FVector& V) { X -= V.X; Y -= V.Y; Z -= V.Z; return *this; }
	template <typename FArg, typename = std::enable_if_t<std::is_arithmetic_v<FArg>>>
	FVector& operator*=(FArg Scale) { X *= Scale; Y *= Scale; Z *= Scale; return *this; }

	double SizeSquared() const { return X * X + Y * Y + Z * Z; }
	double Size() const { return std::sqrt(SizeSquared()); }
	double Size2D() const { return std::sqrt(X * X + Y * Y); }
	bool IsNearlyZero(double Tolerance = UE_KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(X) <= Tolerance && FMath::Abs(Y) <= Tolerance && FMath::Abs(Z) <= Tolerance;
	}

	FVector GetSafeNormal(double Tolerance = UE_SMALL_NUMBER, const FVector& ResultIfZero = ZeroVector) const
	{
		const double SquareSum = SizeSquared();
		if (SquareSum == 1.0)
		{
			return *this;
		}
		if (SquareSum < Tolerance)
		{
			return ResultIfZero;
		}
		const double Scale = 1.0 / std::sqrt(SquareSum);
		return FVector(X * Scale, Y * Scale, Z * Scale);
	}

	FVector ProjectOnToNormal(const FVector& Normal) const { return Normal * DotProduct(*this, Normal); }

	static double DotProduct(const FVector& A, const FVector& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	static FVector CrossProduct(const FVector& A, const FVector& B)
	{
		return FVector(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X);
	}
	static double Dist(const FVector& A, const FVector& B) { return (A - B).Size(); }
	static FVector VectorPlaneProject(const FVector& V, const FVector& PlaneNormal) { return V - V.ProjectOnToNormal(PlaneNormal); }
};

inline const FVector FVector::ZeroVector(0.0, 0.0, 0.0);
inline const FVector FVector::OneVector(1.0, 1.0, 1.0);
inline const FVector FVector::UpVector(0.0, 0.0, 1.0);
inline const FVector FVector::ForwardVector(1.0, 0.0, 0.0);
inline const FVector FVector::RightVector(0.0, 1.0, 0.0);

template <typename FArg, typename = std::enable_if_t<std::is_arithmetic_v<FArg>>>
inline FVector operator*(FArg Scale, const FVector& V) { return V * Scale; }
