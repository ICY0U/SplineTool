// Procedural test scenes for the AutoGrind detector, each with the grind lines a designer would place.
//
// Every scene is built from simple solids (boxes, extrusions, sweeps, revolutions) standing on a ground
// plane at Z = 0, in centimetres with Z up. Truth lines say where a line belongs, what kind it is, and
// whether it is required, should be suggested for review (found but unticked), or is optional (either
// way is fine). Forbidden boxes mark places where no kept line may run.
#pragma once

#include "../Source/AutoGrind/Private/Core/AutoGrindCore.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Scenes
{
	using AutoGrindCore::LineKind;
	using AutoGrindCore::Mesh;
	using AutoGrindCore::Settings;
	using AutoGrindCore::Vec3;

	constexpr double TwoPi = 6.283185307179586;
	constexpr double Pi = 3.141592653589793;

	inline Vec3 V(double X, double Y, double Z) { return Vec3{X, Y, Z}; }

	// ---------------------------------------------------------------------------------------------
	// Geometry builders. Faces are wound anticlockwise seen from outside, as the detector requires.
	// ---------------------------------------------------------------------------------------------

	inline uint32_t AddPoint(Mesh& M, const Vec3& P)
	{
		M.Vertices.push_back(P);
		return uint32_t(M.Vertices.size() - 1);
	}

	inline void Tri(Mesh& M, uint32_t A, uint32_t B, uint32_t C) { M.Indices.insert(M.Indices.end(), {A, B, C}); }

	// A, B, C, D anticlockwise seen from the side the quad faces.
	inline void Quad(Mesh& M, uint32_t A, uint32_t B, uint32_t C, uint32_t D)
	{
		Tri(M, A, B, C);
		Tri(M, A, C, D);
	}

	// Sweeps a closed profile, anticlockwise in (Y, Z) seen from +X, along X from X0 to X1, with caps.
	inline void Extrude(Mesh& M, const std::vector<std::pair<double, double>>& Profile, double X0, double X1, bool bCaps = true)
	{
		const uint32_t Base = uint32_t(M.Vertices.size());
		const uint32_t N = uint32_t(Profile.size());
		for (const auto& [Y, Z] : Profile) M.Vertices.push_back({X0, Y, Z});
		for (const auto& [Y, Z] : Profile) M.Vertices.push_back({X1, Y, Z});
		for (uint32_t I = 0; I < N; ++I)
		{
			const uint32_t J = (I + 1) % N;
			M.Indices.insert(M.Indices.end(), {Base + I, Base + J, Base + N + J, Base + I, Base + N + J, Base + N + I});
		}
		if (bCaps)
		{
			// Ear clipping, so concave profiles (a quarter pipe's) get caps that stay inside the outline.
			std::vector<uint32_t> Left;
			for (uint32_t I = 0; I < N; ++I) Left.push_back(I);
			auto Cross2 = [&](uint32_t A, uint32_t B, uint32_t C)
			{
				return (Profile[B].first - Profile[A].first) * (Profile[C].second - Profile[A].second) - (Profile[B].second - Profile[A].second) * (Profile[C].first - Profile[A].first);
			};
			auto Inside = [&](uint32_t P, uint32_t A, uint32_t B, uint32_t C)
			{
				return Cross2(A, B, P) > 1e-9 && Cross2(B, C, P) > 1e-9 && Cross2(C, A, P) > 1e-9;
			};
			while (Left.size() > 3)
			{
				bool bClipped = false;
				for (size_t K = 0; K < Left.size() && !bClipped; ++K)
				{
					const uint32_t A = Left[(K + Left.size() - 1) % Left.size()], B = Left[K], C = Left[(K + 1) % Left.size()];
					if (Cross2(A, B, C) <= 1e-9) continue;
					bool bEmpty = true;
					for (uint32_t P : Left) if (P != A && P != B && P != C && Inside(P, A, B, C)) { bEmpty = false; break; }
					if (!bEmpty) continue;
					M.Indices.insert(M.Indices.end(), {Base + N + A, Base + N + B, Base + N + C, Base + A, Base + C, Base + B});
					Left.erase(Left.begin() + std::ptrdiff_t(K));
					bClipped = true;
				}
				if (!bClipped) break;
			}
			if (Left.size() == 3)
			{
				M.Indices.insert(M.Indices.end(), {Base + N + Left[0], Base + N + Left[1], Base + N + Left[2], Base + Left[0], Base + Left[2], Base + Left[1]});
			}
		}
	}

	inline void Box(Mesh& M, double X0, double Y0, double Z0, double X1, double Y1, double Z1)
	{
		Extrude(M, {{Y0, Z0}, {Y1, Z0}, {Y1, Z1}, {Y0, Z1}}, X0, X1);
	}

	// A tube's cross-section around (0, CentreZ): Sides sides, with a flat facet on top when the count is even.
	inline std::vector<std::pair<double, double>> Tube(double Radius, double CentreZ, int Sides = 12, double OffsetY = 0)
	{
		std::vector<std::pair<double, double>> Out;
		for (int K = 0; K < Sides; ++K)
		{
			const double T = (180.0 / Sides + 360.0 / Sides * K) * Pi / 180;
			Out.push_back({OffsetY - Radius * std::sin(T), CentreZ + Radius * std::cos(T)});
		}
		return Out;
	}

	// Sweeps a closed profile (U to the left of the path, V up), anticlockwise seen looking along the path,
	// along a polyline with mitred joints. A closed path wraps round with no caps.
	inline void Sweep(Mesh& M, const std::vector<std::pair<double, double>>& Profile, const std::vector<Vec3>& Path, bool bClosedPath = false, bool bCaps = true)
	{
		auto Sub = [](const Vec3& A, const Vec3& B) { return Vec3{A.X - B.X, A.Y - B.Y, A.Z - B.Z}; };
		auto Add = [](const Vec3& A, const Vec3& B) { return Vec3{A.X + B.X, A.Y + B.Y, A.Z + B.Z}; };
		auto Mul = [](const Vec3& A, double S) { return Vec3{A.X * S, A.Y * S, A.Z * S}; };
		auto Dot = [](const Vec3& A, const Vec3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; };
		auto Cross = [](const Vec3& A, const Vec3& B) { return Vec3{A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X}; };
		auto Unit = [&](const Vec3& A) { const double S = std::sqrt(Dot(A, A)); return S > 0 ? Mul(A, 1 / S) : A; };
		const size_t Count = Path.size();
		const uint32_t N = uint32_t(Profile.size());
		const uint32_t Base = uint32_t(M.Vertices.size());
		for (size_t I = 0; I < Count; ++I)
		{
			const bool bHasPrev = I > 0 || bClosedPath;
			const bool bHasNext = I + 1 < Count || bClosedPath;
			const Vec3 Prev = bHasPrev ? Unit(Sub(Path[I], Path[(I + Count - 1) % Count])) : Unit(Sub(Path[I + 1], Path[I]));
			const Vec3 Next = bHasNext ? Unit(Sub(Path[(I + 1) % Count], Path[I])) : Prev;
			const Vec3 Tangent = Unit(Add(Prev, Next));
			const Vec3 Left = Unit(Cross(Vec3{0, 0, 1}, Prev));
			const Vec3 Up = Cross(Prev, Left);
			for (const auto& [U, W] : Profile)
			{
				const Vec3 Offset = Add(Mul(Left, U), Mul(Up, W));
				const double T = -Dot(Offset, Tangent) / Dot(Prev, Tangent);
				M.Vertices.push_back(Add(Add(Path[I], Offset), Mul(Prev, T)));
			}
		}
		const size_t Rings = bClosedPath ? Count : Count - 1;
		for (size_t R = 0; R < Rings; ++R)
		{
			const uint32_t A0 = Base + uint32_t(R) * N;
			const uint32_t B0 = Base + uint32_t((R + 1) % Count) * N;
			for (uint32_t I = 0; I < N; ++I)
			{
				const uint32_t J = (I + 1) % N;
				// The profile runs anticlockwise seen looking along the path, so its outside lies to the right
				// of each profile edge: A(I) -> A(J) -> B(J) faces out.
				M.Indices.insert(M.Indices.end(), {A0 + I, B0 + I, B0 + J, A0 + I, B0 + J, A0 + J});
			}
		}
		if (bCaps && !bClosedPath)
		{
			const uint32_t Last = Base + uint32_t(Count - 1) * N;
			for (uint32_t I = 1; I + 1 < N; ++I)
			{
				M.Indices.insert(M.Indices.end(), {Base, Base + I, Base + I + 1, Last, Last + I + 1, Last + I});
			}
		}
	}

	// Revolves a profile of (radius, height) points about Z. Faces point out when the profile runs
	// upward along its outside (from the bottom to the top of an outer wall).
	inline void Revolve(Mesh& M, const std::vector<std::pair<double, double>>& Profile, int Segments, double CentreX = 0, double CentreY = 0)
	{
		const uint32_t Base = uint32_t(M.Vertices.size());
		const uint32_t N = uint32_t(Profile.size());
		for (int S = 0; S < Segments; ++S)
		{
			const double A = TwoPi * S / Segments;
			for (const auto& [R, Z] : Profile)
			{
				M.Vertices.push_back({CentreX + R * std::cos(A), CentreY + R * std::sin(A), Z});
			}
		}
		for (int S = 0; S < Segments; ++S)
		{
			const uint32_t A0 = Base + uint32_t(S) * N;
			const uint32_t B0 = Base + uint32_t((S + 1) % Segments) * N;
			for (uint32_t I = 0; I + 1 < N; ++I)
			{
				M.Indices.insert(M.Indices.end(), {A0 + I, B0 + I, B0 + I + 1, A0 + I, B0 + I + 1, A0 + I + 1});
			}
		}
	}

	inline double SignedVolume(const Mesh& M)
	{
		double Volume = 0;
		for (size_t I = 0; I + 2 < M.Indices.size(); I += 3)
		{
			const Vec3& A = M.Vertices[M.Indices[I]];
			const Vec3& B = M.Vertices[M.Indices[I + 1]];
			const Vec3& C = M.Vertices[M.Indices[I + 2]];
			Volume += (A.X * (B.Y * C.Z - B.Z * C.Y) - A.Y * (B.X * C.Z - B.Z * C.X) + A.Z * (B.X * C.Y - B.Y * C.X)) / 6;
		}
		return Volume;
	}

	inline void FlipAll(Mesh& M)
	{
		for (size_t I = 0; I + 2 < M.Indices.size(); I += 3)
		{
			std::swap(M.Indices[I + 1], M.Indices[I + 2]);
		}
	}

	inline Mesh Transformed(Mesh M, double YawDegrees, const Vec3& Offset)
	{
		const double C = std::cos(YawDegrees * Pi / 180), S = std::sin(YawDegrees * Pi / 180);
		for (Vec3& P : M.Vertices)
		{
			P = {P.X * C - P.Y * S + Offset.X, P.X * S + P.Y * C + Offset.Y, P.Z + Offset.Z};
		}
		return M;
	}

	inline Mesh Named(const std::string& Name)
	{
		Mesh M;
		M.Name = Name;
		return M;
	}

	inline Mesh Ground(double Z = 0, double Half = 5000)
	{
		return {"ground", {{-Half, -Half, Z}, {Half, -Half, Z}, {Half, Half, Z}, {-Half, Half, Z}}, {0, 1, 2, 0, 2, 3}};
	}

	// ---------------------------------------------------------------------------------------------
	// Scenes and their truth.
	// ---------------------------------------------------------------------------------------------

	enum class Expect
	{
		Required, // must be found and kept, as one line of this kind
		Suggested, // must be found but left unticked for review
		Optional // fine either way, and a line here is not a false positive
	};

	struct Truth
	{
		std::string Label;
		std::vector<Vec3> Points;
		LineKind Kind = LineKind::Stone;
		Expect Want = Expect::Required;
		// How many separate lines may cover it. 1 for a line that must come out whole.
		int MaxPieces = 1;
	};

	struct Box3
	{
		Vec3 Min;
		Vec3 Max;
		std::string Label;
	};

	struct Scene
	{
		std::string Name;
		std::string Group;
		std::string What;
		std::vector<Mesh> Scan;
		std::vector<Mesh> Context;
		std::vector<Truth> Lines;
		std::vector<Box3> Forbidden;
		std::function<void(Settings&)> Configure;
		bool bGround = true;
	};

	inline std::vector<Vec3> Seg(double X0, double Y0, double Z0, double X1, double Y1, double Z1) { return {V(X0, Y0, Z0), V(X1, Y1, Z1)}; }

	inline std::vector<Vec3> Arc(double CX, double CY, double Radius, double Z, double FromDegrees, double ToDegrees, int Steps)
	{
		std::vector<Vec3> Out;
		for (int I = 0; I <= Steps; ++I)
		{
			const double A = (FromDegrees + (ToDegrees - FromDegrees) * I / Steps) * Pi / 180;
			Out.push_back({CX + Radius * std::cos(A), CY + Radius * std::sin(A), Z});
		}
		return Out;
	}

	inline Truth Req(const std::string& Label, std::vector<Vec3> Points, LineKind Kind = LineKind::Stone) { return {Label, std::move(Points), Kind, Expect::Required, 1}; }
	inline Truth Sug(const std::string& Label, std::vector<Vec3> Points, LineKind Kind = LineKind::Stone) { return {Label, std::move(Points), Kind, Expect::Suggested, 1}; }
	inline Truth Opt(const std::string& Label, std::vector<Vec3> Points, LineKind Kind = LineKind::Stone) { return {Label, std::move(Points), Kind, Expect::Optional, 99}; }

	// The four top edges of an axis-aligned box top.
	inline void BoxTop(Scene& S, double X0, double Y0, double X1, double Y1, double Z, Expect Want = Expect::Required, const std::string& Prefix = "")
	{
		auto Make = [&](const std::string& L, std::vector<Vec3> P) { S.Lines.push_back({Prefix + L, std::move(P), LineKind::Stone, Want, Want == Expect::Optional ? 99 : 1}); };
		Make("front", Seg(X0, Y0, Z, X1, Y0, Z));
		Make("back", Seg(X0, Y1, Z, X1, Y1, Z));
		if (Y1 - Y0 >= 50)
		{
			Make("left end", Seg(X0, Y0, Z, X0, Y1, Z));
			Make("right end", Seg(X1, Y0, Z, X1, Y1, Z));
		}
	}

	inline std::vector<Scene> All()
	{
		std::vector<Scene> Out;

		// ----- Modular pieces: one line across meshes placed next to each other -----
		{
			Scene S{"modular-ledge-3", "joining", "three 150 cm ledge modules placed end to end", {}, {}, {}, {}, {}, true};
			for (int I = 0; I < 3; ++I) { Mesh M = Named("ledge module " + std::to_string(I)); Box(M, I * 150.0, 0, 0, I * 150.0 + 150, 60, 50); S.Scan.push_back(M); }
			BoxTop(S, 0, 0, 450, 60, 50);
			Out.push_back(S);
		}
		{
			Scene S{"modular-ledge-gaps", "joining", "three ledge modules with 1 cm gaps", {}, {}, {}, {}, {}, true};
			for (int I = 0; I < 3; ++I) { Mesh M = Named("gapped module " + std::to_string(I)); Box(M, I * 151.0, 0, 0, I * 151.0 + 150, 60, 50); S.Scan.push_back(M); }
			BoxTop(S, 0, 0, 452, 60, 50);
			Out.push_back(S);
		}
		{
			Scene S{"modular-ledge-overlap", "joining", "three ledge modules overlapping by 4 cm", {}, {}, {}, {}, {}, true};
			for (int I = 0; I < 3; ++I) { Mesh M = Named("overlapping module " + std::to_string(I)); Box(M, I * 146.0, 0, 0, I * 146.0 + 150, 60, 50); S.Scan.push_back(M); }
			BoxTop(S, 0, 0, 442, 60, 50);
			Out.push_back(S);
		}
		{
			Scene S{"modular-ledge-short", "joining", "six 40 cm modules: each too short alone", {}, {}, {}, {}, {}, true};
			for (int I = 0; I < 6; ++I) { Mesh M = Named("short module " + std::to_string(I)); Box(M, I * 40.0, 0, 0, I * 40.0 + 40, 60, 50); S.Scan.push_back(M); }
			BoxTop(S, 0, 0, 240, 60, 50);
			Out.push_back(S);
		}
		{
			Scene S{"modular-ledge-misaligned", "joining", "modules 0.5 cm out of line and level", {}, {}, {}, {}, {}, true};
			for (int I = 0; I < 3; ++I) { Mesh M = Named("misaligned module " + std::to_string(I)); Box(M, I * 150.0, I * 0.5, 0, I * 150.0 + 150, 60 + I * 0.5, 50 + (I % 2) * 0.5); S.Scan.push_back(M); }
			BoxTop(S, 0, 0.5, 450, 60.5, 50.25);
			Out.push_back(S);
		}
		{
			Scene S{"modular-rail", "joining", "four 1 m tube segments in a row", {}, {}, {}, {}, {}, true};
			for (int I = 0; I < 4; ++I) { Mesh M = Named("rail segment " + std::to_string(I)); Extrude(M, Tube(2.5, 80), I * 100.0, I * 100.0 + 100); S.Scan.push_back(M); }
			S.Lines.push_back(Req("rail", Seg(0, 0, 82.5, 400, 0, 82.5), LineKind::Rail));
			Out.push_back(S);
		}
		{
			Scene S{"modular-rail-bracket", "joining", "two rail segments either side of a 25 cm bracket", {}, {}, {}, {}, {}, true};
			Mesh A = Named("rail a"); Extrude(A, Tube(2.5, 80), 0, 200); S.Scan.push_back(A);
			Mesh B = Named("rail b"); Extrude(B, Tube(2.5, 80), 225, 425); S.Scan.push_back(B);
			Mesh Post = Named("bracket"); Box(Post, 205, -2, 0, 220, 2, 78); S.Context.push_back(Post);
			S.Lines.push_back(Req("rail", Seg(0, 0, 82.5, 425, 0, 82.5), LineKind::Rail));
			Out.push_back(S);
		}
		{
			Scene S{"modular-curved-ledge", "joining", "a curved ledge built from four 22.5 degree pieces", {}, {}, {}, {}, {}, true};
			for (int I = 0; I < 4; ++I)
			{
				std::vector<Vec3> Path;
				for (int K = 0; K <= 4; ++K) { const double A = (I * 22.5 + K * 22.5 / 4) * Pi / 180; Path.push_back({400 * std::cos(A), 400 * std::sin(A), 0}); }
				Mesh M = Named("curve piece " + std::to_string(I));
				// Profile left of the path (towards the centre): inner radius 340, outer 400, 50 high.
				Sweep(M, {{0, 0}, {0, 50}, {60, 50}, {60, 0}}, Path);
				if (SignedVolume(M) < 0) FlipAll(M);
				S.Scan.push_back(M);
			}
			S.Lines.push_back(Req("outer arc", Arc(0, 0, 400, 50, 0, 90, 32)));
			S.Lines.push_back(Req("inner arc", Arc(0, 0, 340, 50, 0, 90, 32)));
			S.Lines.push_back(Req("start end", Seg(340, 0, 50, 400, 0, 50)));
			S.Lines.push_back(Req("finish end", Seg(0, 340, 50, 0, 400, 50)));
			Out.push_back(S);
		}
		{
			Scene S{"modular-corner", "joining", "an L of two modules: lines stop at the corner", {}, {}, {}, {}, {}, true};
			Mesh A = Named("leg a"); Box(A, 0, 0, 0, 300, 60, 50); S.Scan.push_back(A);
			Mesh B = Named("leg b"); Box(B, 0, 60, 0, 60, 300, 50); S.Scan.push_back(B);
			S.Lines.push_back(Req("outer x", Seg(0, 0, 50, 300, 0, 50)));
			S.Lines.push_back(Req("outer y", Seg(0, 0, 50, 0, 300, 50)));
			S.Lines.push_back(Req("inner x", Seg(60, 60, 50, 300, 60, 50)));
			S.Lines.push_back(Req("inner y", Seg(60, 60, 50, 60, 300, 50)));
			S.Lines.push_back(Req("end x", Seg(300, 0, 50, 300, 60, 50)));
			S.Lines.push_back(Req("end y", Seg(0, 300, 50, 60, 300, 50)));
			Out.push_back(S);
		}
		{
			Scene S{"parallel-ledges", "joining", "two ledges 30 cm apart stay four lines", {}, {}, {}, {}, {}, true};
			Mesh A = Named("ledge a"); Box(A, 0, 0, 0, 300, 50, 45); S.Scan.push_back(A);
			Mesh B = Named("ledge b"); Box(B, 0, 80, 0, 300, 130, 45); S.Scan.push_back(B);
			BoxTop(S, 0, 0, 300, 50, 45, Expect::Required, "a ");
			BoxTop(S, 0, 80, 300, 130, 45, Expect::Required, "b ");
			Out.push_back(S);
		}
		{
			Scene S{"stacked-duplicate", "joining", "the same ledge twice in one place gives one set of lines", {}, {}, {}, {}, {}, true};
			for (int I = 0; I < 2; ++I) { Mesh M = Named("duplicate " + std::to_string(I)); Box(M, 0, 0, 0, 300, 80, 45); S.Scan.push_back(M); }
			BoxTop(S, 0, 0, 300, 80, 45);
			Out.push_back(S);
		}

		// ----- Rails and stone: what kind a line is -----
		{
			Scene S{"round-rail-posts", "rails", "a 5 cm tube rail on three posts, one mesh", {}, {}, {}, {}, {}, true};
			Mesh M = Named("round rail"); Extrude(M, Tube(2.5, 90), 0, 400);
			for (double X : {20.0, 200.0, 380.0}) Box(M, X - 2.5, -2.5, 0, X + 2.5, 2.5, 88);
			S.Scan.push_back(M);
			S.Lines.push_back(Req("rail", Seg(0, 0, 92.5, 400, 0, 92.5), LineKind::Rail));
			Out.push_back(S);
		}
		{
			Scene S{"square-rail", "rails", "a 5 cm square bar on posts", {}, {}, {}, {}, {}, true};
			Mesh M = Named("square rail"); Box(M, 0, -2.5, 60, 400, 2.5, 65);
			for (double X : {30.0, 370.0}) Box(M, X - 2.5, -2.5, 0, X + 2.5, 2.5, 60);
			S.Scan.push_back(M);
			S.Lines.push_back(Req("rail", Seg(0, 0, 65, 400, 0, 65), LineKind::Rail));
			Out.push_back(S);
		}
		{
			Scene S{"flat-bar", "rails", "an 8 cm flat bar 1.5 cm thick on legs", {}, {}, {}, {}, {}, true};
			Mesh M = Named("flat bar"); Box(M, 0, -4, 30, 300, 4, 31.5);
			for (double X : {20.0, 280.0}) Box(M, X - 2, -2, 0, X + 2, 2, 30);
			S.Scan.push_back(M);
			S.Lines.push_back(Req("rail", Seg(0, 0, 31.5, 300, 0, 31.5), LineKind::Rail));
			Out.push_back(S);
		}
		{
			// Wider than Rail Max Width: one line along the crest, stone as retail maps make thick pipes.
			Scene S{"thick-pipe", "rails", "a 20 cm pipe on posts: one stone line on its crest", {}, {}, {}, {}, {}, true};
			Mesh M = Named("thick pipe"); Extrude(M, Tube(10, 70, 24), 0, 400);
			for (double X : {40.0, 360.0}) Box(M, X - 4, -4, 0, X + 4, 4, 61);
			S.Scan.push_back(M);
			S.Lines.push_back(Req("crest", Seg(0, 0, 80, 400, 0, 80), LineKind::Stone));
			S.Forbidden.push_back({V(5, -12, 60), V(395, -6, 79), "shoulder line"});
			S.Forbidden.push_back({V(5, 6, 60), V(395, 12, 79), "shoulder line"});
			Out.push_back(S);
		}
		{
			Scene S{"big-pipe-ground", "rails", "a 40 cm pipe lying on the ground: one stone line on its crest", {}, {}, {}, {}, {}, true};
			Mesh M = Named("big pipe"); Extrude(M, Tube(20, 20, 32), 0, 400);
			S.Scan.push_back(M);
			S.Lines.push_back(Req("crest", Seg(0, 0, 40, 400, 0, 40), LineKind::Stone));
			S.Forbidden.push_back({V(5, -25, 0), V(395, -8, 45), "shoulder line"});
			S.Forbidden.push_back({V(5, 8, 0), V(395, 25, 45), "shoulder line"});
			Out.push_back(S);
		}
		{
			Scene S{"planter", "rails", "a planter with 12 cm walls: wall tops are stone, not rail", {}, {}, {}, {}, {}, true};
			Mesh P = Named("planter");
			Box(P, 0, 0, 0, 200, 12, 50); Box(P, 0, 188, 0, 200, 200, 50);
			Box(P, 0, 12, 0, 12, 188, 50); Box(P, 188, 12, 0, 200, 188, 50);
			Box(P, 12, 12, 0, 188, 188, 40);
			S.Scan.push_back(P);
			// Where the side walls meet them, the front and back walls' tops carry on into a corner: the line
			// runs where the top has a fall on both sides.
			S.Lines.push_back(Req("front wall", Seg(12, 6, 50, 188, 6, 50), LineKind::Stone));
			S.Lines.push_back(Req("back wall", Seg(12, 194, 50, 188, 194, 50), LineKind::Stone));
			S.Lines.push_back(Opt("front corners", Seg(0, 6, 50, 200, 6, 50), LineKind::Stone));
			S.Lines.push_back(Opt("back corners", Seg(0, 194, 50, 200, 194, 50), LineKind::Stone));
			S.Lines.push_back(Req("left wall", Seg(6, 12, 50, 6, 188, 50), LineKind::Stone));
			S.Lines.push_back(Req("right wall", Seg(194, 12, 50, 194, 188, 50), LineKind::Stone));
			Out.push_back(S);
		}
		{
			Scene S{"narrow-wall", "rails", "a 14 cm concrete wall 1 m tall is stone", {}, {}, {}, {}, {}, true};
			Mesh W = Named("wall"); Box(W, 0, -7, 0, 300, 7, 100); S.Scan.push_back(W);
			S.Lines.push_back(Req("wall top", Seg(0, 0, 100, 300, 0, 100), LineKind::Stone));
			Out.push_back(S);
		}
		{
			Scene S{"kinked-rail", "rails", "flat, down, flat: one rail through both kinks", {}, {}, {}, {}, {}, true};
			Mesh M = Named("kinked rail");
			const std::vector<Vec3> Path{V(0, 0, 120), V(150, 0, 120), V(400, 0, 20), V(550, 0, 20)};
			Sweep(M, Tube(2.5, 0), Path);
			if (SignedVolume(M) < 0) FlipAll(M);
			S.Scan.push_back(M);
			S.Lines.push_back(Req("rail", {V(0, 0, 122.5), V(150, 0, 122.5), V(400, 0, 22.5), V(550, 0, 22.5)}, LineKind::Rail));
			Out.push_back(S);
		}
		{
			Scene S{"rail-on-wall", "rails", "a tube rail on short posts above a 20 cm wall", {}, {}, {}, {}, {}, true};
			Mesh W = Named("low wall"); Box(W, 0, -10, 0, 400, 10, 80); S.Scan.push_back(W);
			Mesh R = Named("wall rail"); Extrude(R, Tube(2.5, 107.5), 0, 400);
			for (double X : {20.0, 200.0, 380.0}) Box(R, X - 2, -2, 80, X + 2, 2, 105);
			S.Scan.push_back(R);
			S.Lines.push_back(Req("rail", Seg(0, 0, 110, 400, 0, 110), LineKind::Rail));
			S.Lines.push_back(Opt("wall edge a", Seg(0, -10, 80, 400, -10, 80)));
			S.Lines.push_back(Opt("wall edge b", Seg(0, 10, 80, 400, 10, 80)));
			S.Lines.push_back(Opt("wall crest", Seg(0, 0, 80, 400, 0, 80)));
			Out.push_back(S);
		}
		{
			Scene S{"handrail-stairs", "rails", "six steps with a sloped handrail: rail yes, steps no", {}, {}, {}, {}, {}, true};
			Mesh Stairs = Named("stairs");
			// Steps run down along +X: the top landing is 102 high, each step 17 down and 30 deep.
			Box(Stairs, -200, 0, 0, 0, 200, 102);
			for (int K = 0; K < 5; ++K) Box(Stairs, K * 30.0, 0, 0, K * 30.0 + 30, 200, 102 - 17.0 * (K + 1));
			S.Scan.push_back(Stairs);
			Mesh Rail = Named("handrail");
			const std::vector<Vec3> Path{V(-60, 100, 192), V(0, 100, 192), V(180, 100, 192 - 102), V(240, 100, 192 - 102)};
			Sweep(Rail, Tube(2.5, 0), Path);
			if (SignedVolume(Rail) < 0) FlipAll(Rail);
			for (const Vec3& P : {V(-40, 100, 102), V(200, 100, 0)}) Box(Rail, P.X - 2, 98, P.Z, P.X + 2, 102, (P.X < 0 ? 192 : 90) - 2);
			S.Scan.push_back(Rail);
			S.Lines.push_back(Req("handrail", {V(-60, 100, 194.5), V(0, 100, 194.5), V(180, 100, 92.5), V(240, 100, 92.5)}, LineKind::Rail));
			for (int K = 0; K <= 5; ++K) S.Forbidden.push_back({V(K * 30.0 - 3, 5, 102 - 17.0 * K - 3), V(K * 30.0 + 3, 195, 102 - 17.0 * K + 3), "stair nosing " + std::to_string(K)});
			S.Lines.push_back(Opt("landing back", Seg(-200, 0, 102, -200, 200, 102)));
			S.Lines.push_back(Opt("landing side a", Seg(-200, 0, 102, 0, 0, 102)));
			S.Lines.push_back(Opt("landing side b", Seg(-200, 200, 102, 0, 200, 102)));
			Out.push_back(S);
		}
		{
			Scene S{"quarter-coping", "rails", "a quarter pipe with coping: the coping is a rail", {}, {}, {}, {}, {}, true};
			Mesh Q = Named("quarter pipe");
			// Ramp faces -Y: transition radius 150 rising to 120, deck 100 deep behind it, 300 wide along X.
			std::vector<std::pair<double, double>> Profile;
			Profile.push_back({-150, 0});
			for (int K = 0; K <= 16; ++K)
			{
				const double A = (Pi / 2) * K / 16;
				Profile.push_back({-150 + 150 * std::sin(A), 150 - 150 * std::cos(A)});
			}
			// The transition tops out at 150 high on the circle; cut it at 120 by keeping points up to there.
			std::vector<std::pair<double, double>> Cut;
			for (const auto& P : Profile) if (P.second <= 120.0001) Cut.push_back(P);
			const double TopY = Cut.back().first;
			Cut.push_back({TopY, 120});
			Cut.push_back({TopY + 100, 120});
			Cut.push_back({TopY + 100, 0});
			// The profile runs from the ramp foot up and over: clockwise seen from +X, so reverse it.
			std::reverse(Cut.begin(), Cut.end());
			Extrude(Q, Cut, 0, 300);
			if (SignedVolume(Q) < 0) FlipAll(Q);
			S.Scan.push_back(Q);
			Mesh C = Named("coping"); Extrude(C, Tube(3, 120, 12, TopY), 0, 300); S.Scan.push_back(C);
			S.Lines.push_back(Req("coping", Seg(0, TopY, 123, 300, TopY, 123), LineKind::Rail));
			S.Lines.push_back(Opt("deck back", Seg(0, TopY + 100, 120, 300, TopY + 100, 120)));
			S.Lines.push_back(Opt("deck side a", Seg(0, TopY, 120, 0, TopY + 100, 120)));
			S.Lines.push_back(Opt("deck side b", Seg(300, TopY, 120, 300, TopY + 100, 120)));
			S.Lines.push_back(Opt("transition side a", Seg(0, TopY - 60, 30, 0, TopY, 120)));
			S.Lines.push_back(Opt("transition side b", Seg(300, TopY - 60, 30, 300, TopY, 120)));
			Out.push_back(S);
		}
		{
			Scene S{"bowl-coping", "rails", "a round bowl: its coping is one closed rail", {}, {}, {}, {}, {}, true};
			// Deck at 100 with a round hole of radius 200; the bowl's floor is the ground.
			Mesh Deck = Named("bowl deck");
			Revolve(Deck, {{200, 0}, {200, 100}, {500, 100}}, 48);
			// Revolve faces point out of a profile running up its outside; this one runs up its inside.
			FlipAll(Deck);
			S.Context.push_back(Deck);
			Mesh Coping = Named("bowl coping");
			std::vector<std::pair<double, double>> Ring;
			for (int K = 0; K < 12; ++K) { const double T = TwoPi * K / 12; Ring.push_back({200 + 3 * std::cos(T), 100 + 3 * std::sin(T)}); }
			Ring.push_back(Ring.front());
			Revolve(Coping, Ring, 48);
			if (SignedVolume(Coping) < 0) FlipAll(Coping);
			S.Scan.push_back(Coping);
			auto Circle = Arc(0, 0, 200, 103, 0, 360, 96);
			S.Lines.push_back(Req("coping ring", Circle, LineKind::Rail));
			Out.push_back(S);
		}

		// ----- What to line and what not -----
		{
			Scene S{"stairs-alone", "filtering", "a flight of stairs has no lines", {}, {}, {}, {}, {}, true};
			Mesh Stairs = Named("stair flight");
			Box(Stairs, -150, 0, 0, 0, 250, 102);
			for (int K = 0; K < 5; ++K) Box(Stairs, K * 30.0, 0, 0, K * 30.0 + 30, 250, 102 - 17.0 * (K + 1));
			S.Scan.push_back(Stairs);
			for (int K = 0; K <= 5; ++K) S.Forbidden.push_back({V(K * 30.0 - 3, 5, 102 - 17.0 * K - 3), V(K * 30.0 + 3, 245, 102 - 17.0 * K + 3), "stair nosing " + std::to_string(K)});
			S.Lines.push_back(Opt("landing back", Seg(-150, 0, 102, -150, 250, 102)));
			S.Lines.push_back(Opt("landing side a", Seg(-150, 0, 102, 0, 0, 102)));
			S.Lines.push_back(Opt("landing side b", Seg(-150, 250, 102, 0, 250, 102)));
			S.Configure = [](Settings& C) { C.MinDrop = 15; };
			Out.push_back(S);
		}
		{
			Scene S{"curb", "filtering", "a 15 cm curb is suggested, not kept", {}, {}, {}, {}, {}, true};
			Mesh Walk = Named("sidewalk"); Box(Walk, 0, 0, 0, 600, 250, 15); S.Scan.push_back(Walk);
			Mesh Building = Named("building"); Box(Building, -100, 250, 0, 700, 400, 600); S.Context.push_back(Building);
			S.Lines.push_back(Sug("curb", Seg(0, 0, 15, 600, 0, 15)));
			S.Lines.push_back(Opt("end a", Seg(0, 0, 15, 0, 250, 15)));
			S.Lines.push_back(Opt("end b", Seg(600, 0, 15, 600, 250, 15)));
			S.Forbidden.push_back({V(5, 240, 10), V(595, 260, 20), "edge against the building"});
			Out.push_back(S);
		}
		{
			Scene S{"manual-pad", "filtering", "a 20 cm manual pad: suggested for review", {}, {}, {}, {}, {}, true};
			Mesh Pad = Named("manual pad"); Box(Pad, 0, 0, 0, 300, 120, 20); S.Scan.push_back(Pad);
			BoxTop(S, 0, 0, 300, 120, 20, Expect::Suggested);
			Out.push_back(S);
		}
		{
			Scene S{"ledge-against-wall", "filtering", "only the free edge of a ledge pushed into a wall", {}, {}, {}, {}, {}, true};
			Mesh Ledge = Named("ledge"); Box(Ledge, 0, 0, 0, 300, 70, 60); S.Scan.push_back(Ledge);
			Mesh Wall = Named("wall"); Box(Wall, -50, 40, 0, 350, 300, 400); S.Context.push_back(Wall);
			S.Lines.push_back(Req("front", Seg(0, 0, 60, 300, 0, 60)));
			S.Forbidden.push_back({V(-5, 35, 50), V(305, 75, 70), "inside the wall"});
			S.Lines.push_back(Opt("end a", Seg(0, 0, 60, 0, 40, 60)));
			S.Lines.push_back(Opt("end b", Seg(300, 0, 60, 300, 40, 60)));
			Out.push_back(S);
		}
		{
			Scene S{"bench-slats", "filtering", "a slatted bench seat: only the outer long edges", {}, {}, {}, {}, {}, true};
			Mesh Seat = Named("slatted seat");
			for (int K = 0; K < 5; ++K) Box(Seat, 0, K * 11.5, 40, 150, K * 11.5 + 10, 45);
			for (double X : {10.0, 140.0}) Box(Seat, X - 3, 2, 0, X + 3, 54, 40);
			S.Scan.push_back(Seat);
			S.Lines.push_back(Req("front", Seg(0, 0, 45, 150, 0, 45)));
			S.Lines.push_back(Req("back", Seg(0, 56, 45, 150, 56, 45)));
			S.Lines.push_back(Opt("end a", Seg(0, 0, 45, 0, 56, 45)));
			S.Lines.push_back(Opt("end b", Seg(150, 0, 45, 150, 56, 45)));
			for (int K = 0; K < 4; ++K) S.Forbidden.push_back({V(5, K * 11.5 + 9, 40), V(145, K * 11.5 + 12.5, 50), "slat gap " + std::to_string(K)});
			Out.push_back(S);
		}
		{
			Scene S{"dome", "filtering", "a 1 m dome has no edge", {}, {}, {}, {}, {}, true};
			Mesh D = Named("dome");
			const int Rings = 12, Segments = 32;
			for (int I = 0; I <= Rings; ++I)
				for (int J = 0; J < Segments; ++J)
				{
					const double Th = (Pi / 2) * I / Rings, Ph = TwoPi * J / Segments;
					D.Vertices.push_back({100 * std::sin(Th) * std::cos(Ph), 100 * std::sin(Th) * std::sin(Ph), 100 * std::cos(Th)});
				}
			for (int I = 0; I < Rings; ++I)
				for (int J = 0; J < Segments; ++J)
				{
					const uint32_t A = uint32_t(I * Segments + J), B = uint32_t(I * Segments + (J + 1) % Segments);
					const uint32_t C = uint32_t((I + 1) * Segments + J), E = uint32_t((I + 1) * Segments + (J + 1) % Segments);
					D.Indices.insert(D.Indices.end(), {A, C, E, A, E, B});
				}
			S.Scan.push_back(D);
			S.Forbidden.push_back({V(-110, -110, 1), V(110, 110, 110), "anywhere on the dome"});
			Out.push_back(S);
		}
		{
			Scene S{"mound", "filtering", "a 3 m radius mound: no lines along its smooth sides", {}, {}, {}, {}, {}, true};
			Mesh M = Named("mound");
			std::vector<std::pair<double, double>> Profile;
			for (int K = 0; K <= 24; ++K) { const double A = Pi * K / 24; Profile.push_back({300 * std::cos(A), 300 * std::sin(A)}); }
			Extrude(M, Profile, 0, 500);
			if (SignedVolume(M) < 0) FlipAll(M);
			S.Scan.push_back(M);
			S.Forbidden.push_back({V(20, -300, 1), V(480, -100, 300), "smooth side"});
			S.Forbidden.push_back({V(20, 100, 1), V(480, 300, 300), "smooth side"});
			S.Lines.push_back(Opt("end arc a", Arc(0, 0, 300, 0, 0, 180, 24)));
			for (Vec3& P : S.Lines.back().Points) { const double Y = P.X; P = {0, Y, P.Y}; }
			S.Lines.push_back(Opt("end arc b", Arc(0, 0, 300, 0, 0, 180, 24)));
			for (Vec3& P : S.Lines.back().Points) { const double Y = P.X; P = {500, Y, P.Y}; }
			Out.push_back(S);
		}
		{
			Scene S{"rounded-block", "filtering", "a block with 40 cm rounded edges, like a car: no lines", {}, {}, {}, {}, {}, true};
			Mesh M = Named("rounded block");
			std::vector<std::pair<double, double>> Profile{{-90, 0}, {90, 0}};
			for (int K = 0; K <= 8; ++K) { const double A = (Pi / 2) * K / 8; Profile.push_back({50 + 40 * std::cos(A), 100 + 40 * std::sin(A)}); }
			for (int K = 0; K <= 8; ++K) { const double A = Pi / 2 + (Pi / 2) * K / 8; Profile.push_back({-50 + 40 * std::cos(A), 100 + 40 * std::sin(A)}); }
			Extrude(M, Profile, 0, 400);
			if (SignedVolume(M) < 0) FlipAll(M);
			S.Scan.push_back(M);
			S.Forbidden.push_back({V(5, -95, 50), V(395, 95, 145), "rounded top"});
			// The block's flat ends are real edges, following the top of its profile.
			std::vector<Vec3> EndA, EndB;
			for (const auto& [Y, Z] : Profile) if (Z > 120) { EndA.push_back(V(0, Y, Z)); EndB.push_back(V(400, Y, Z)); }
			std::sort(EndA.begin(), EndA.end(), [](const Vec3& L, const Vec3& R) { return L.Y < R.Y; });
			std::sort(EndB.begin(), EndB.end(), [](const Vec3& L, const Vec3& R) { return L.Y < R.Y; });
			S.Lines.push_back(Opt("end a", EndA));
			S.Lines.push_back(Opt("end b", EndB));
			Out.push_back(S);
		}
		{
			Scene S{"kicker", "filtering", "a 30 degree kicker: no lines up its sloping sides", {}, {}, {}, {}, {}, true};
			Mesh M = Named("kicker");
			Extrude(M, {{0, 0}, {150, 0}, {150, 150 * std::tan(30 * Pi / 180)}}, 0, 100);
			if (SignedVolume(M) < 0) FlipAll(M);
			S.Scan.push_back(M);
			const double LipZ = 150 * std::tan(30 * Pi / 180);
			S.Lines.push_back(Opt("lip", Seg(0, 150, LipZ, 100, 150, LipZ)));
			S.Forbidden.push_back({V(-3, 20, 8), V(3, 145, LipZ - 3), "sloping side"});
			S.Forbidden.push_back({V(97, 20, 8), V(103, 145, LipZ - 3), "sloping side"});
			Out.push_back(S);
		}
		{
			Scene S{"bullnose-ledge", "filtering", "a ledge with 5 cm rounded edges still has its lines", {}, {}, {}, {}, {}, true};
			Mesh M = Named("bullnose ledge");
			std::vector<std::pair<double, double>> Profile{{0, 0}, {60, 0}};
			for (int K = 0; K <= 6; ++K) { const double A = (Pi / 2) * K / 6; Profile.push_back({55 + 5 * std::cos(A), 40 + 5 * std::sin(A)}); }
			for (int K = 0; K <= 6; ++K) { const double A = Pi / 2 + (Pi / 2) * K / 6; Profile.push_back({5 + 5 * std::cos(A), 40 + 5 * std::sin(A)}); }
			Extrude(M, Profile, 0, 300);
			if (SignedVolume(M) < 0) FlipAll(M);
			S.Scan.push_back(M);
			S.Lines.push_back(Req("front", Seg(0, 1.5, 43.5, 300, 1.5, 43.5)));
			S.Lines.push_back(Req("back", Seg(0, 58.5, 43.5, 300, 58.5, 43.5)));
			S.Lines.push_back(Opt("end a", Seg(0, 0, 45, 0, 60, 45)));
			S.Lines.push_back(Opt("end b", Seg(300, 0, 45, 300, 60, 45)));
			Out.push_back(S);
		}
		{
			Scene S{"chamfered-ledge", "filtering", "a 3 cm 45 degree chamfer gives one line at its foot", {}, {}, {}, {}, {}, true};
			Mesh M = Named("chamfered ledge"); Extrude(M, {{0, 0}, {40, 0}, {40, 97}, {37, 100}, {0, 100}}, 0, 200); S.Scan.push_back(M);
			S.Lines.push_back(Req("chamfer foot", Seg(0, 40, 97, 200, 40, 97)));
			S.Lines.push_back(Req("square edge", Seg(0, 0, 100, 200, 0, 100)));
			S.Forbidden.push_back({V(5, 36, 99), V(195, 38, 101), "top of the chamfer"});
			Out.push_back(S);
		}
		{
			Scene S{"hut-roof", "filtering", "a gable roof: eaves optional, nothing on the ridge", {}, {}, {}, {}, {}, true};
			Mesh Hut = Named("hut");
			Box(Hut, 0, 0, 0, 400, 300, 250);
			S.Context.push_back(Hut);
			Mesh Roof = Named("roof");
			Extrude(Roof, {{-20, 250}, {320, 250}, {150, 250 + 170 * std::tan(30 * Pi / 180)}}, -20, 420);
			if (SignedVolume(Roof) < 0) FlipAll(Roof);
			S.Scan.push_back(Roof);
			const double RidgeZ = 250 + 170 * std::tan(30 * Pi / 180);
			S.Lines.push_back(Opt("eave a", Seg(-20, -20, 250, 420, -20, 250)));
			S.Lines.push_back(Opt("eave b", Seg(-20, 320, 250, 420, 320, 250)));
			for (double X : {-20.0, 420.0})
			{
				S.Lines.push_back(Opt("rake a", Seg(X, -20, 250, X, 150, RidgeZ)));
				S.Lines.push_back(Opt("rake b", Seg(X, 150, RidgeZ, X, 320, 250)));
			}
			S.Forbidden.push_back({V(0, 140, RidgeZ - 5), V(400, 160, RidgeZ + 5), "ridge"});
			Out.push_back(S);
		}
		{
			Scene S{"aframe-ridge", "filtering", "a steep A-frame: a ridge line when ridges are on", {}, {}, {}, {}, {}, true};
			Mesh M = Named("a-frame");
			Extrude(M, {{-15, 30}, {15, 30}, {0, 30 + 15 * std::tan(60 * Pi / 180)}}, 0, 300);
			if (SignedVolume(M) < 0) FlipAll(M);
			for (double X : {30.0, 270.0}) Box(M, X - 3, -3, 0, X + 3, 3, 30);
			S.Scan.push_back(M);
			S.Lines.push_back(Sug("ridge", Seg(0, 0, 30 + 15 * std::tan(60 * Pi / 180), 300, 0, 30 + 15 * std::tan(60 * Pi / 180)), LineKind::Rail));
#ifndef AUTOGRIND_LEGACY
			S.Configure = [](Settings& C) { C.bDetectRidges = true; };
#endif
			Out.push_back(S);
		}

		// ----- Robustness: messy geometry -----
		{
			Scene S{"flipped-top-triangle", "robustness", "one top triangle wound the wrong way", {}, {}, {}, {}, {}, true};
			Mesh M = Named("flipped top"); Box(M, 0, 0, 0, 300, 100, 45);
			// Find a triangle facing up and flip it.
			for (size_t I = 0; I + 2 < M.Indices.size(); I += 3)
			{
				const Vec3 &A = M.Vertices[M.Indices[I]], &B = M.Vertices[M.Indices[I + 1]], &C = M.Vertices[M.Indices[I + 2]];
				const double NZ = (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
				if (NZ > 0 && A.Z > 44) { std::swap(M.Indices[I + 1], M.Indices[I + 2]); break; }
			}
			S.Scan.push_back(M);
			BoxTop(S, 0, 0, 300, 100, 45);
			Out.push_back(S);
		}
		{
			Scene S{"double-sided-plank", "robustness", "a plank modelled as a double-sided plane", {}, {}, {}, {}, {}, true};
			Mesh M = Named("double sided plank");
			const uint32_t A = AddPoint(M, V(0, 0, 45)), B = AddPoint(M, V(250, 0, 45)), C = AddPoint(M, V(250, 30, 45)), D = AddPoint(M, V(0, 30, 45));
			Quad(M, A, B, C, D);
			Quad(M, A, D, C, B);
			for (double X : {20.0, 230.0}) Box(M, X - 3, 5, 0, X + 3, 25, 44);
			S.Scan.push_back(M);
			S.Lines.push_back(Req("front", Seg(0, 0, 45, 250, 0, 45), LineKind::Stone));
			S.Lines.push_back(Req("back", Seg(0, 30, 45, 250, 30, 45), LineKind::Stone));
			S.Lines.push_back(Opt("crest", Seg(0, 15, 45, 250, 15, 45), LineKind::Stone));
			Out.push_back(S);
		}
		{
			Scene S{"rotated-modules", "joining", "two modules turned 30 degrees, meeting end to end", {}, {}, {}, {}, {}, true};
			Mesh A = Named("turned a"); Box(A, 0, 0, 0, 200, 60, 50);
			Mesh B = Named("turned b"); Box(B, 200, 0, 0, 400, 60, 50);
			S.Scan.push_back(Transformed(A, 30, V(100, 50, 0)));
			S.Scan.push_back(Transformed(B, 30, V(100, 50, 0)));
			auto Turn = [](double X, double Y, double Z) { const double C = std::cos(Pi / 6), Sn = std::sin(Pi / 6); return V(X * C - Y * Sn + 100, X * Sn + Y * C + 50, Z); };
			S.Lines.push_back(Req("front", {Turn(0, 0, 50), Turn(400, 0, 50)}));
			S.Lines.push_back(Req("back", {Turn(0, 60, 50), Turn(400, 60, 50)}));
			S.Lines.push_back(Req("end a", {Turn(0, 0, 50), Turn(0, 60, 50)}));
			S.Lines.push_back(Req("end b", {Turn(400, 0, 50), Turn(400, 60, 50)}));
			Out.push_back(S);
		}
		{
			Scene S{"round-planter", "filtering", "a round planter: one closed line round its rim", {}, {}, {}, {}, {}, true};
			Mesh M = Named("round planter");
			Revolve(M, {{0, 0}, {100, 0}, {100, 50}, {0, 50}}, 48);
			if (SignedVolume(M) < 0) FlipAll(M);
			S.Scan.push_back(M);
			S.Lines.push_back(Req("rim", Arc(0, 0, 100, 50, 0, 360, 96)));
			Out.push_back(S);
		}
		return Out;
	}
}
