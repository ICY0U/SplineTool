// Scores the AutoGrind detector on the procedural scenes in scenes.h.
//
//   benchmark [-v] [scene-name-part...]
//
// For each scene the detector scans the scene's meshes with the drop and wall tests seeing every mesh
// and the ground, and its lines are compared with the scene's truth:
//   - covered: how much of each required line a kept line of the right kind runs along (within 6 cm);
//   - pieces: how many separate lines cover it (one is right: a ledge is one spline, not several);
//   - kind: rail or stone, as the truth says;
//   - suggested lines must be found but left unticked for review;
//   - false positives: kept line length near no truth line, and any kept line in a forbidden box.
// Built with -DAUTOGRIND_LEGACY against an older detector, every line counts as kept.
// Exits non-zero when a scene fails, so it can gate a build.
#include "scenes.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace AutoGrindCore;

namespace
{
	constexpr double Tolerance = 6;
	constexpr double Step = 2;
	constexpr double PieceMinimum = 10;

	struct Found
	{
		LineKind Kind = LineKind::Stone;
		std::vector<Vec3> Points;
		bool bKept = true;
		bool bClosed = false;
	};

	double Distance(const Vec3& A, const Vec3& B) { return std::sqrt((A.X - B.X) * (A.X - B.X) + (A.Y - B.Y) * (A.Y - B.Y) + (A.Z - B.Z) * (A.Z - B.Z)); }

	double PointSegment(const Vec3& P, const Vec3& A, const Vec3& B)
	{
		const Vec3 AB{B.X - A.X, B.Y - A.Y, B.Z - A.Z};
		const double L = AB.X * AB.X + AB.Y * AB.Y + AB.Z * AB.Z;
		double T = L > 0 ? ((P.X - A.X) * AB.X + (P.Y - A.Y) * AB.Y + (P.Z - A.Z) * AB.Z) / L : 0;
		T = std::clamp(T, 0.0, 1.0);
		return Distance(P, {A.X + AB.X * T, A.Y + AB.Y * T, A.Z + AB.Z * T});
	}

	double PointPolyline(const Vec3& P, const std::vector<Vec3>& Line)
	{
		double Best = std::numeric_limits<double>::max();
		for (size_t I = 0; I + 1 < Line.size(); ++I)
		{
			Best = std::min(Best, PointSegment(P, Line[I], Line[I + 1]));
		}
		if (Line.size() == 1)
		{
			Best = Distance(P, Line[0]);
		}
		return Best;
	}

	std::vector<Vec3> Samples(const std::vector<Vec3>& Line)
	{
		std::vector<Vec3> Out;
		for (size_t I = 0; I + 1 < Line.size(); ++I)
		{
			const double L = Distance(Line[I], Line[I + 1]);
			const int N = std::max(1, int(std::ceil(L / Step)));
			for (int K = 0; K < N; ++K)
			{
				const double T = double(K) / N;
				Out.push_back({Line[I].X + (Line[I + 1].X - Line[I].X) * T, Line[I].Y + (Line[I + 1].Y - Line[I].Y) * T, Line[I].Z + (Line[I + 1].Z - Line[I].Z) * T});
			}
		}
		if (!Line.empty())
		{
			Out.push_back(Line.back());
		}
		return Out;
	}

	bool InBox(const Vec3& P, const Scenes::Box3& B)
	{
		return P.X >= B.Min.X && P.X <= B.Max.X && P.Y >= B.Min.Y && P.Y <= B.Max.Y && P.Z >= B.Min.Z && P.Z <= B.Max.Z;
	}

	struct TruthScore
	{
		double Length = 0;
		double Covered = 0; // by kept lines of the right kind
		double CoveredAny = 0; // by any line, kept or not, any kind
		int Pieces = 0;
		bool bKindRight = true;
		bool bAllUnkept = true;
		std::vector<std::string> Problems;
	};

	struct SceneScore
	{
		bool bPass = true;
		double RequiredLength = 0;
		double RequiredCovered = 0;
		int Fragments = 0;
		int KindErrors = 0;
		int Missing = 0;
		int SuggestErrors = 0;
		double FalseLength = 0;
		int ForbiddenHits = 0;
		int Overmerged = 0;
		size_t Lines = 0;
		size_t Kept = 0;
		double Seconds = 0;
		std::vector<std::string> Problems;
	};

	// A turn about Z and a shift, applied to a whole scene: the detector must not care which way a park faces.
	struct Placement
	{
		double Yaw = 0;
		Vec3 Offset;

		Vec3 Apply(const Vec3& P) const
		{
			const double C = std::cos(Yaw * Scenes::Pi / 180), S = std::sin(Yaw * Scenes::Pi / 180);
			return {P.X * C - P.Y * S + Offset.X, P.X * S + P.Y * C + Offset.Y, P.Z + Offset.Z};
		}
		Vec3 Undo(const Vec3& P) const
		{
			const double C = std::cos(Yaw * Scenes::Pi / 180), S = std::sin(Yaw * Scenes::Pi / 180);
			const Vec3 Q{P.X - Offset.X, P.Y - Offset.Y, P.Z - Offset.Z};
			return {Q.X * C + Q.Y * S, -Q.X * S + Q.Y * C, Q.Z};
		}
	};

	Scenes::Scene Placed(Scenes::Scene S, const Placement& Where)
	{
		for (std::vector<Mesh>* List : {&S.Scan, &S.Context})
		{
			for (Mesh& M : *List)
			{
				for (Vec3& P : M.Vertices) P = Where.Apply(P);
			}
		}
		for (Scenes::Truth& T : S.Lines)
		{
			for (Vec3& P : T.Points) P = Where.Apply(P);
		}
		return S;
	}

	std::vector<Found> Detect(const Scenes::Scene& S, double& Seconds, const Placement& Where)
	{
		std::vector<Mesh> Context = S.Context;
		Context.insert(Context.end(), S.Scan.begin(), S.Scan.end());
		if (S.bGround)
		{
			Mesh Floor = Scenes::Ground();
			for (Vec3& P : Floor.Vertices) P = Where.Apply(P);
			Context.push_back(Floor);
		}
		Settings Config;
		if (S.Configure)
		{
			S.Configure(Config);
		}
		const auto Started = std::chrono::steady_clock::now();
		const TriangleField Field(Context);
		const std::vector<Line> Lines = FindGrindLines(S.Scan, [&](const Vec3& From, double Max) { return Field.Below(From, Max); },
			[&](const Vec3& From, const Vec3& To) { return Field.FirstHit(From, To); }, Config);
		Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Started).count();
		std::vector<Found> Out;
		for (const Line& L : Lines)
		{
			Found F;
			F.Kind = L.Kind;
			F.Points = L.Points;
			F.bClosed = L.bClosed;
#ifndef AUTOGRIND_LEGACY
			F.bKept = L.bSuggested;
#endif
			Out.push_back(F);
		}
		return Out;
	}

	SceneScore Score(const Scenes::Scene& S, const std::vector<Found>& Lines, bool bVerbose, const Placement& Where)
	{
		SceneScore Result;
		Result.Lines = Lines.size();
		for (const Found& F : Lines)
		{
			Result.Kept += F.bKept ? 1 : 0;
		}
		// Which truth lines each found line is a piece of, to catch one line run round a corner.
		std::vector<std::set<size_t>> PieceOf(Lines.size());
		for (size_t T = 0; T < S.Lines.size(); ++T)
		{
			const Scenes::Truth& Want = S.Lines[T];
			const std::vector<Vec3> Points = Samples(Want.Points);
			TruthScore Here;
			for (size_t I = 0; I + 1 < Want.Points.size(); ++I)
			{
				Here.Length += Distance(Want.Points[I], Want.Points[I + 1]);
			}
			const double PerSample = Here.Length / double(std::max<size_t>(1, Points.size() - 1));
			std::vector<double> CoverBy(Lines.size(), 0);
			int CoveredRight = 0, CoveredAny = 0;
			for (const Vec3& P : Points)
			{
				bool bRight = false, bAny = false;
				for (size_t F = 0; F < Lines.size(); ++F)
				{
					if (PointPolyline(P, Lines[F].Points) <= Tolerance)
					{
						bAny = true;
						CoverBy[F] += PerSample;
						bRight |= Lines[F].bKept && Lines[F].Kind == Want.Kind;
					}
				}
				CoveredRight += bRight ? 1 : 0;
				CoveredAny += bAny ? 1 : 0;
			}
			const double Share = Points.empty() ? 0 : double(CoveredRight) / double(Points.size());
			const double ShareAny = Points.empty() ? 0 : double(CoveredAny) / double(Points.size());
			Here.Covered = Here.Length * Share;
			Here.CoveredAny = Here.Length * ShareAny;
			for (size_t F = 0; F < Lines.size(); ++F)
			{
				if (CoverBy[F] >= std::min(PieceMinimum, Here.Length * 0.5))
				{
					++Here.Pieces;
					Here.bKindRight &= Lines[F].Kind == Want.Kind;
					Here.bAllUnkept &= !Lines[F].bKept;
					if (Want.Want != Scenes::Expect::Optional)
					{
						PieceOf[F].insert(T);
					}
				}
			}
			char Buffer[256];
			if (Want.Want == Scenes::Expect::Required)
			{
				Result.RequiredLength += Here.Length;
				Result.RequiredCovered += Here.Covered;
				if (Share < 0.9)
				{
					++Result.Missing;
					std::snprintf(Buffer, sizeof(Buffer), "'%s' covered %.0f%% by kept lines of the right kind (%.0f%% by any line)", Want.Label.c_str(), Share * 100, ShareAny * 100);
					Result.Problems.push_back(Buffer);
				}
				if (Here.Pieces > Want.MaxPieces)
				{
					Result.Fragments += Here.Pieces - Want.MaxPieces;
					std::snprintf(Buffer, sizeof(Buffer), "'%s' comes out in %d pieces", Want.Label.c_str(), Here.Pieces);
					Result.Problems.push_back(Buffer);
				}
				if (Here.Pieces > 0 && !Here.bKindRight)
				{
					++Result.KindErrors;
					std::snprintf(Buffer, sizeof(Buffer), "'%s' should be %s", Want.Label.c_str(), Want.Kind == LineKind::Rail ? "rail" : "stone");
					Result.Problems.push_back(Buffer);
				}
			}
			else if (Want.Want == Scenes::Expect::Suggested)
			{
				if (ShareAny < 0.9)
				{
					++Result.SuggestErrors;
					std::snprintf(Buffer, sizeof(Buffer), "'%s' should be suggested for review, found %.0f%%", Want.Label.c_str(), ShareAny * 100);
					Result.Problems.push_back(Buffer);
				}
				else if (!Here.bAllUnkept)
				{
					++Result.SuggestErrors;
					std::snprintf(Buffer, sizeof(Buffer), "'%s' should be left unticked for review, not kept", Want.Label.c_str());
					Result.Problems.push_back(Buffer);
				}
				else if (!Here.bKindRight)
				{
					++Result.KindErrors;
					std::snprintf(Buffer, sizeof(Buffer), "'%s' should be %s", Want.Label.c_str(), Want.Kind == LineKind::Rail ? "rail" : "stone");
					Result.Problems.push_back(Buffer);
				}
			}
			if (bVerbose)
			{
				std::printf("      truth %-22s %6.0f cm  covered %3.0f%%  any %3.0f%%  pieces %d%s\n", Want.Label.c_str(), Here.Length, Share * 100, ShareAny * 100, Here.Pieces,
					Here.Pieces > 0 && !Here.bKindRight ? "  WRONG KIND" : "");
			}
		}
		for (size_t F = 0; F < Lines.size(); ++F)
		{
			if (PieceOf[F].size() > 1)
			{
				++Result.Overmerged;
				Result.Problems.push_back("one line runs along " + std::to_string(PieceOf[F].size()) + " separate truth lines (joined round a corner?)");
			}
			if (!Lines[F].bKept)
			{
				continue;
			}
			const std::vector<Vec3> Points = Samples(Lines[F].Points);
			double Length = 0;
			for (size_t I = 0; I + 1 < Lines[F].Points.size(); ++I)
			{
				Length += Distance(Lines[F].Points[I], Lines[F].Points[I + 1]);
			}
			const double PerSample = Length / double(std::max<size_t>(1, Points.size() - 1));
			double Unexplained = 0;
			std::set<std::string> HitBoxes;
			for (const Vec3& P : Points)
			{
				bool bExplained = false;
				for (const Scenes::Truth& T : S.Lines)
				{
					if (PointPolyline(P, T.Points) <= Tolerance)
					{
						bExplained = true;
						break;
					}
				}
				Unexplained += bExplained ? 0 : PerSample;
				for (const Scenes::Box3& B : S.Forbidden)
				{
					if (InBox(Where.Undo(P), B))
					{
						HitBoxes.insert(B.Label);
					}
				}
			}
			Result.FalseLength += Unexplained;
			if (Unexplained > 10)
			{
				char Buffer[256];
				std::snprintf(Buffer, sizeof(Buffer), "a kept %s line %.0f cm long is %.0f cm away from every truth line (from %.0f,%.0f,%.0f)", Lines[F].Kind == LineKind::Rail ? "rail" : "stone", Length, Unexplained,
					Lines[F].Points.front().X, Lines[F].Points.front().Y, Lines[F].Points.front().Z);
				Result.Problems.push_back(Buffer);
			}
			for (const std::string& Label : HitBoxes)
			{
				++Result.ForbiddenHits;
				Result.Problems.push_back("a kept line runs on the " + Label);
			}
		}
		Result.bPass = Result.Missing == 0 && Result.Fragments == 0 && Result.KindErrors == 0 && Result.SuggestErrors == 0 && Result.ForbiddenHits == 0 && Result.Overmerged == 0 && Result.FalseLength <= 10;
		return Result;
	}
}

int main(int ArgCount, char** Args)
{
	bool bVerbose = false;
	bool bTurn = false;
	std::vector<std::string> Only;
	for (int I = 1; I < ArgCount; ++I)
	{
		if (std::strcmp(Args[I], "-v") == 0)
		{
			bVerbose = true;
		}
		else if (std::strcmp(Args[I], "-turn") == 0)
		{
			bTurn = true;
		}
		else
		{
			Only.push_back(Args[I]);
		}
	}
	// With -turn every scene also runs turned and moved to awkward places: off the grid, far from the origin.
	std::vector<Placement> Placements{Placement{}};
	if (bTurn)
	{
		Placements.push_back({17, {1234.5, -987.25, 3.5}});
		Placements.push_back({133, {-40213.75, 5521.5, -250}});
		Placements.push_back({251.5, {77.7, 99999.9, 1200}});
		Placements.push_back({90, {-0.001, 0.001, 0}});
	}
#ifdef AUTOGRIND_LEGACY
	std::printf("AutoGrind benchmark: legacy detector (every line counts as kept)\n\n");
#else
	std::printf("AutoGrind benchmark\n\n");
#endif
	SceneScore Total;
	int Passed = 0, Count = 0;
	std::map<std::string, std::pair<int, int>> ByGroup;
	std::vector<std::pair<Scenes::Scene, Placement>> Runs;
	for (const Scenes::Scene& Original : Scenes::All())
	{
		for (const Placement& Where : Placements)
		{
			Scenes::Scene Turned = Placed(Original, Where);
			if (Where.Yaw != 0 || Where.Offset.X != 0)
			{
				char Suffix[64];
				std::snprintf(Suffix, sizeof(Suffix), "@%g", Where.Yaw);
				Turned.Name += Suffix;
			}
			Runs.push_back({Turned, Where});
		}
	}
	for (const auto& [S, Where] : Runs)
	{
		if (!Only.empty())
		{
			bool bWanted = false;
			for (const std::string& Part : Only)
			{
				bWanted |= S.Name.find(Part) != std::string::npos;
			}
			if (!bWanted)
			{
				continue;
			}
		}
		double Seconds = 0;
		const std::vector<Found> Lines = Detect(S, Seconds, Where);
		const SceneScore R = Score(S, Lines, bVerbose, Where);
		++Count;
		Passed += R.bPass ? 1 : 0;
		ByGroup[S.Group].first += R.bPass ? 1 : 0;
		ByGroup[S.Group].second += 1;
		Total.RequiredLength += R.RequiredLength;
		Total.RequiredCovered += R.RequiredCovered;
		Total.Fragments += R.Fragments;
		Total.KindErrors += R.KindErrors;
		Total.Missing += R.Missing;
		Total.SuggestErrors += R.SuggestErrors;
		Total.FalseLength += R.FalseLength;
		Total.ForbiddenHits += R.ForbiddenHits;
		Total.Overmerged += R.Overmerged;
		std::printf("%s %-26s %2zu lines (%2zu kept)  %5.1f ms  %s\n", R.bPass ? "PASS" : "FAIL", S.Name.c_str(), R.Lines, R.Kept, Seconds * 1000, S.What.c_str());
		if (!R.bPass || bVerbose)
		{
			for (const std::string& P : R.Problems)
			{
				std::printf("      - %s\n", P.c_str());
			}
		}
		if (bVerbose)
		{
			for (const Found& F : Lines)
			{
				double Length = 0;
				for (size_t I = 0; I + 1 < F.Points.size(); ++I) Length += Distance(F.Points[I], F.Points[I + 1]);
				std::printf("      found %-5s %s%s %6.1f cm, %zu points, from (%.1f, %.1f, %.1f) to (%.1f, %.1f, %.1f)\n", F.Kind == LineKind::Rail ? "rail" : "stone", F.bKept ? "kept" : "review", F.bClosed ? " closed" : "",
					Length, F.Points.size(), F.Points.front().X, F.Points.front().Y, F.Points.front().Z, F.Points.back().X, F.Points.back().Y, F.Points.back().Z);
			}
		}
	}
	std::printf("\n%d of %d scenes pass", Passed, Count);
	for (const auto& [Group, Counts] : ByGroup)
	{
		std::printf("  |  %s %d/%d", Group.c_str(), Counts.first, Counts.second);
	}
	std::printf("\nrequired length covered by kept lines of the right kind: %.1f%% of %.0f m\n", Total.RequiredLength > 0 ? 100 * Total.RequiredCovered / Total.RequiredLength : 0, Total.RequiredLength / 100);
	std::printf("required lines missing: %d   extra pieces: %d   wrong kind: %d   suggestion errors: %d\n", Total.Missing, Total.Fragments, Total.KindErrors, Total.SuggestErrors);
	std::printf("false positive length: %.0f cm   forbidden hits: %d   merged round corners: %d\n", Total.FalseLength, Total.ForbiddenHits, Total.Overmerged);
	if (Passed == Count)
	{
		std::printf("AUTOGRIND_BENCHMARK_PASS\n");
	}
	return Passed == Count ? 0 : 1;
}
