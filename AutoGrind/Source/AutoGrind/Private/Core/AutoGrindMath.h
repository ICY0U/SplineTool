// Small vector helpers shared by the detector's source files. Internal: not part of the API.
#pragma once

#include "AutoGrindCore.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

namespace AutoGrindCore::Detail
{
	constexpr double Pi = 3.14159265358979323846;

	inline Vec3 operator+(const Vec3& A, const Vec3& B) { return {A.X + B.X, A.Y + B.Y, A.Z + B.Z}; }
	inline Vec3 operator-(const Vec3& A, const Vec3& B) { return {A.X - B.X, A.Y - B.Y, A.Z - B.Z}; }
	inline Vec3 operator*(const Vec3& A, double S) { return {A.X * S, A.Y * S, A.Z * S}; }
	inline double Dot(const Vec3& A, const Vec3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	inline Vec3 Cross(const Vec3& A, const Vec3& B) { return {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X}; }
	inline double Size(const Vec3& A) { return std::sqrt(Dot(A, A)); }
	inline Vec3 Unit(const Vec3& A)
	{
		const double S = Size(A);
		return S > 0 ? A * (1 / S) : Vec3{};
	}
	inline Vec3 Lerp(const Vec3& A, const Vec3& B, double T) { return A + (B - A) * T; }
	inline double Radians(double Degrees) { return Degrees * Pi / 180; }
	inline double Degrees(double Radians) { return Radians * 180 / Pi; }
	inline Vec3 Flat(const Vec3& A) { return {A.X, A.Y, 0}; }
	inline double FlatSize(const Vec3& A) { return std::sqrt(A.X * A.X + A.Y * A.Y); }
	inline double Distance(const Vec3& A, const Vec3& B) { return Size(B - A); }
	// The angle between two unit vectors, in degrees.
	inline double AngleBetween(const Vec3& A, const Vec3& B) { return Degrees(std::acos(std::clamp(Dot(A, B), -1.0, 1.0))); }

	inline double PointSegmentDistance(const Vec3& P, const Vec3& A, const Vec3& B, double* OutT = nullptr)
	{
		const Vec3 AB = B - A;
		const double LengthSquared = Dot(AB, AB);
		const double T = LengthSquared > 0 ? std::clamp(Dot(P - A, AB) / LengthSquared, 0.0, 1.0) : 0.0;
		if (OutT)
		{
			*OutT = T;
		}
		return Size(P - (A + AB * T));
	}

	inline double Median(std::vector<double> Values)
	{
		if (Values.empty())
		{
			return 0;
		}
		std::sort(Values.begin(), Values.end());
		return Values[Values.size() / 2];
	}

	inline uint64_t EdgeKey(uint32_t A, uint32_t B)
	{
		if (A > B)
		{
			std::swap(A, B);
		}
		return (uint64_t(A) << 32) | B;
	}

	struct DisjointSet
	{
		std::vector<uint32_t> Parent;
		explicit DisjointSet(size_t Count) : Parent(Count)
		{
			for (size_t I = 0; I < Count; ++I)
			{
				Parent[I] = uint32_t(I);
			}
		}
		uint32_t Find(uint32_t I)
		{
			while (Parent[I] != I)
			{
				Parent[I] = Parent[Parent[I]];
				I = Parent[I];
			}
			return I;
		}
		void Join(uint32_t A, uint32_t B) { Parent[Find(A)] = Find(B); }
	};

	// Merges vertices closer than Tolerance, so split normals and UV seams do not break the topology.
	// Remap gets the welded index of each input vertex.
	std::vector<Vec3> Weld(const std::vector<Vec3>& Vertices, double Tolerance, std::vector<uint32_t>& Remap);

	// Moller-Trumbore, hitting a triangle from either side: how far along From -> To (0 to 1) it is met.
	std::optional<double> SegmentHitsTriangle(const Vec3& From, const Vec3& To, const Vec3& A, const Vec3& B, const Vec3& C);

	struct Face
	{
		uint32_t V[3] = {0, 0, 0};
		Vec3 Normal;
		double Area = 0;
		int32_t Patch = -1; // the detector's connected tops; -1 when the face is not a top
	};

	// Each undirected edge (EdgeKey) and the faces that share it.
	using EdgeMap = std::unordered_map<uint64_t, std::vector<uint32_t>>;

	// Faces over welded points, from three point indices per triangle. Degenerate triangles are dropped.
	// With bRepair, a triangle repeated over the same three points (a double-sided copy, a duplicate) is
	// kept once, facing up when the copies face opposite ways, and each connected surface is wound
	// consistently, keeping the winding most of its area already has.
	void BuildFaces(const std::vector<Vec3>& Points, const std::vector<uint32_t>& Corners, bool bRepair, std::vector<Face>& OutFaces, EdgeMap& OutEdges);

	// Whether a face, as wound, runs from point A to point B along one of its edges.
	inline bool Runs(const Face& F, uint32_t A, uint32_t B)
	{
		for (size_t K = 0; K < 3; ++K)
		{
			if (F.V[K] == A && F.V[(K + 1) % 3] == B)
			{
				return true;
			}
		}
		return false;
	}

	// A face's corner that is not on the edge A-B.
	inline uint32_t ThirdVertex(const Face& F, uint32_t A, uint32_t B)
	{
		for (uint32_t V : F.V)
		{
			if (V != A && V != B)
			{
				return V;
			}
		}
		return F.V[0];
	}
}
