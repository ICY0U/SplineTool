// The sharp convex edges of a set of meshes as a graph, for drawing lines by hand: a click snaps to the
// nearest edge, a path between two clicks follows the edges, and a single pick can take an edge's whole run.
#include "AutoGrindCore.h"
#include "AutoGrindMath.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace AutoGrindCore
{
	using namespace Detail;

	namespace
	{
		// Pieces of separate meshes closer than this along an edge's line are joined: modular pieces placed
		// with a small gap, or pushed into one another.
		constexpr double MinBridge = 2;
		// A bridge must carry on an edge's direction this closely.
		constexpr double BridgeCos = 0.8;
		// Below this, a face's corner counts as lying in the other face's plane.
		constexpr double PlaneSlack = 1e-3;

		int64_t CellOf(double Value, double CellSize) { return int64_t(std::floor(Value / CellSize)); }
	}

	uint64_t EdgeGraph::CellKey(int64_t X, int64_t Y, int64_t Z) const
	{
		return (uint64_t(uint32_t(int32_t(X)) & 0x1fffff) << 42) | (uint64_t(uint32_t(int32_t(Y)) & 0x1fffff) << 21) | uint64_t(uint32_t(int32_t(Z)) & 0x1fffff);
	}

	EdgeGraph::EdgeGraph(const std::vector<Mesh>& Meshes, double FeatureAngleDegrees, double WeldTolerance)
	{
		// Every mesh's points in one list, welded together so meshes that touch share their points.
		std::vector<Vec3> All;
		std::vector<uint32_t> Corners;
		for (const Mesh& M : Meshes)
		{
			const uint32_t Base = uint32_t(All.size());
			All.insert(All.end(), M.Vertices.begin(), M.Vertices.end());
			for (size_t I = 0; I + 2 < M.Indices.size(); I += 3)
			{
				if (M.Indices[I] < M.Vertices.size() && M.Indices[I + 1] < M.Vertices.size() && M.Indices[I + 2] < M.Vertices.size())
				{
					Corners.insert(Corners.end(), {Base + M.Indices[I], Base + M.Indices[I + 1], Base + M.Indices[I + 2]});
				}
			}
		}
		std::vector<uint32_t> Remap;
		Points = Weld(All, std::max(WeldTolerance, 1e-4), Remap);
		for (uint32_t& C : Corners)
		{
			C = Remap[C];
		}
		std::vector<Face> Faces;
		EdgeMap FaceEdges;
		BuildFaces(Points, Corners, true, Faces, FaceEdges);

		// A sharp edge: open, or where two faces turn by at least the feature angle and stick out.
		const double FeatureCos = std::cos(Radians(FeatureAngleDegrees));
		std::vector<std::pair<uint32_t, uint32_t>> Sharp;
		for (const auto& [Key, Shared] : FaceEdges)
		{
			const uint32_t A = uint32_t(Key >> 32);
			const uint32_t B = uint32_t(Key & 0xffffffff);
			bool bSharp = Shared.size() == 1;
			for (size_t I = 0; I < Shared.size() && !bSharp; ++I)
			{
				for (size_t J = I + 1; J < Shared.size() && !bSharp; ++J)
				{
					const Face& F = Faces[Shared[I]];
					const Face& G = Faces[Shared[J]];
					if (Dot(F.Normal, G.Normal) > FeatureCos)
					{
						continue;
					}
					bSharp = Dot(Points[ThirdVertex(G, A, B)] - Points[A], F.Normal) < -PlaneSlack && Dot(Points[ThirdVertex(F, A, B)] - Points[A], G.Normal) < -PlaneSlack;
				}
			}
			if (bSharp)
			{
				Sharp.push_back({A, B});
			}
		}

		// Where one piece's edge ends on another piece's edge (modules pushed into one another), the other edge
		// is split there so a path can pass from one to the other.
		const double Bridge = std::max(MinBridge, 2 * WeldTolerance);
		CellSize = 50;
		std::unordered_map<uint64_t, std::vector<uint32_t>> Near;
		auto Register = [&](std::unordered_map<uint64_t, std::vector<uint32_t>>& Grid, uint32_t Index, const Vec3& P, const Vec3& Q, double Pad)
		{
			for (int64_t X = CellOf(std::min(P.X, Q.X) - Pad, CellSize); X <= CellOf(std::max(P.X, Q.X) + Pad, CellSize); ++X)
			{
				for (int64_t Y = CellOf(std::min(P.Y, Q.Y) - Pad, CellSize); Y <= CellOf(std::max(P.Y, Q.Y) + Pad, CellSize); ++Y)
				{
					for (int64_t Z = CellOf(std::min(P.Z, Q.Z) - Pad, CellSize); Z <= CellOf(std::max(P.Z, Q.Z) + Pad, CellSize); ++Z)
					{
						Grid[CellKey(X, Y, Z)].push_back(Index);
					}
				}
			}
		};
		for (uint32_t I = 0; I < Sharp.size(); ++I)
		{
			Register(Near, I, Points[Sharp[I].first], Points[Sharp[I].second], Bridge);
		}
		std::unordered_set<uint32_t> Ends;
		for (const auto& [A, B] : Sharp)
		{
			Ends.insert(A);
			Ends.insert(B);
		}
		std::vector<std::vector<std::pair<double, uint32_t>>> Splits(Sharp.size());
		for (uint32_t V : Ends)
		{
			const Vec3& P = Points[V];
			const auto Cell = Near.find(CellKey(CellOf(P.X, CellSize), CellOf(P.Y, CellSize), CellOf(P.Z, CellSize)));
			if (Cell == Near.end())
			{
				continue;
			}
			for (uint32_t I : Cell->second)
			{
				const auto [A, B] = Sharp[I];
				if (A == V || B == V)
				{
					continue;
				}
				double T = 0;
				if (PointSegmentDistance(P, Points[A], Points[B], &T) <= Bridge && T > 1e-3 && T < 1 - 1e-3)
				{
					Splits[I].push_back({T, V});
				}
			}
		}
		std::unordered_set<uint64_t> Seen;
		auto AddEdge = [&](uint32_t A, uint32_t B)
		{
			if (A != B && Seen.insert(EdgeKey(A, B)).second)
			{
				Edges.push_back({A, B});
			}
		};
		for (uint32_t I = 0; I < Sharp.size(); ++I)
		{
			std::vector<std::pair<double, uint32_t>>& Along = Splits[I];
			std::sort(Along.begin(), Along.end());
			uint32_t Previous = Sharp[I].first;
			for (const auto& [T, V] : Along)
			{
				AddEdge(Previous, V);
				Previous = V;
			}
			AddEdge(Previous, Sharp[I].second);
		}

		AtVertex.assign(Points.size(), {});
		for (uint32_t I = 0; I < Edges.size(); ++I)
		{
			AtVertex[Edges[I].A].push_back(I);
			AtVertex[Edges[I].B].push_back(I);
		}

		// Across a small gap between pieces, an edge that ends pointing at another edge's start is bridged.
		std::unordered_map<uint64_t, std::vector<uint32_t>> EndCells;
		for (uint32_t V : Ends)
		{
			const Vec3& P = Points[V];
			EndCells[CellKey(CellOf(P.X, CellSize), CellOf(P.Y, CellSize), CellOf(P.Z, CellSize))].push_back(V);
		}
		std::vector<std::pair<uint32_t, uint32_t>> Bridges;
		for (uint32_t V : Ends)
		{
			const Vec3& P = Points[V];
			for (int64_t OX = -1; OX <= 1; ++OX)
			{
				for (int64_t OY = -1; OY <= 1; ++OY)
				{
					for (int64_t OZ = -1; OZ <= 1; ++OZ)
					{
						const auto Cell = EndCells.find(CellKey(CellOf(P.X, CellSize) + OX, CellOf(P.Y, CellSize) + OY, CellOf(P.Z, CellSize) + OZ));
						if (Cell == EndCells.end())
						{
							continue;
						}
						for (uint32_t W : Cell->second)
						{
							const double Gap = Distance(P, Points[W]);
							if (W <= V || Gap > Bridge || Gap <= 0 || Seen.count(EdgeKey(V, W)))
							{
								continue;
							}
							const Vec3 Across = Unit(Points[W] - P);
							bool bIn = false;
							bool bOut = false;
							for (uint32_t E : AtVertex[V])
							{
								const uint32_t Other = Edges[E].A == V ? Edges[E].B : Edges[E].A;
								bIn |= Dot(Unit(P - Points[Other]), Across) >= BridgeCos;
							}
							for (uint32_t E : AtVertex[W])
							{
								const uint32_t Other = Edges[E].A == W ? Edges[E].B : Edges[E].A;
								bOut |= Dot(Across, Unit(Points[Other] - Points[W])) >= BridgeCos;
							}
							if (bIn && bOut)
							{
								Bridges.push_back({V, W});
							}
						}
					}
				}
			}
		}
		for (const auto& [V, W] : Bridges)
		{
			AddEdge(V, W);
			AtVertex[V].push_back(uint32_t(Edges.size() - 1));
			AtVertex[W].push_back(uint32_t(Edges.size() - 1));
		}

		for (uint32_t I = 0; I < Edges.size(); ++I)
		{
			Register(Cells, I, Points[Edges[I].A], Points[Edges[I].B], 0);
		}
	}

	std::pair<Vec3, Vec3> EdgeGraph::EdgeEnds(int64_t Index) const
	{
		if (Index < 0 || size_t(Index) >= Edges.size())
		{
			return {};
		}
		const Edge& Found = Edges[size_t(Index)];
		return {Points[Found.A], Points[Found.B]};
	}

	std::optional<EdgeGraph::Snap> EdgeGraph::Nearest(const Vec3& Point, double Radius) const
	{
		std::optional<Snap> Best;
		std::unordered_set<uint32_t> Tried;
		for (int64_t X = CellOf(Point.X - Radius, CellSize); X <= CellOf(Point.X + Radius, CellSize); ++X)
		{
			for (int64_t Y = CellOf(Point.Y - Radius, CellSize); Y <= CellOf(Point.Y + Radius, CellSize); ++Y)
			{
				for (int64_t Z = CellOf(Point.Z - Radius, CellSize); Z <= CellOf(Point.Z + Radius, CellSize); ++Z)
				{
					const auto Cell = Cells.find(CellKey(X, Y, Z));
					if (Cell == Cells.end())
					{
						continue;
					}
					for (uint32_t Index : Cell->second)
					{
						if (!Tried.insert(Index).second)
						{
							continue;
						}
						double T = 0;
						const double D = PointSegmentDistance(Point, Points[Edges[Index].A], Points[Edges[Index].B], &T);
						if (D <= Radius && (!Best || D < Best->Distance))
						{
							Best = Snap{Lerp(Points[Edges[Index].A], Points[Edges[Index].B], T), int64_t(Index), T, D};
						}
					}
				}
			}
		}
		return Best;
	}

	std::vector<Vec3> EdgeGraph::Path(const Snap& From, const Snap& To, double MaxDetour) const
	{
		if (From.Edge < 0 || To.Edge < 0 || size_t(From.Edge) >= Edges.size() || size_t(To.Edge) >= Edges.size())
		{
			return {};
		}
		if (From.Edge == To.Edge)
		{
			return {From.Point, To.Point};
		}
		const double Straight = Distance(From.Point, To.Point);
		const double Limit = std::max(Straight * MaxDetour, Straight + 1);
		constexpr double Unreached = std::numeric_limits<double>::max();
		constexpr int64_t StartMark = -2;
		std::vector<double> Best(Points.size(), Unreached);
		std::vector<int64_t> Previous(Points.size(), -1);
		using Entry = std::pair<double, uint32_t>;
		std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> Queue;
		const Edge& Start = Edges[size_t(From.Edge)];
		for (uint32_t V : {Start.A, Start.B})
		{
			const double D = Distance(From.Point, Points[V]);
			if (D < Best[V])
			{
				Best[V] = D;
				Previous[V] = StartMark;
				Queue.push({D, V});
			}
		}
		const Edge& Goal = Edges[size_t(To.Edge)];
		double BestTotal = Unreached;
		uint32_t BestEnd = 0;
		while (!Queue.empty())
		{
			const auto [D, V] = Queue.top();
			Queue.pop();
			if (D > Best[V] || D >= BestTotal || D > Limit)
			{
				continue;
			}
			if (V == Goal.A || V == Goal.B)
			{
				const double Total = D + Distance(Points[V], To.Point);
				if (Total < BestTotal)
				{
					BestTotal = Total;
					BestEnd = V;
				}
			}
			for (uint32_t Index : AtVertex[V])
			{
				const uint32_t W = Edges[Index].A == V ? Edges[Index].B : Edges[Index].A;
				const double Next = D + Distance(Points[V], Points[W]);
				if (Next < Best[W] && Next <= Limit)
				{
					Best[W] = Next;
					Previous[W] = V;
					Queue.push({Next, W});
				}
			}
		}
		if (BestTotal > Limit)
		{
			return {};
		}
		std::vector<Vec3> Out{To.Point};
		for (int64_t V = BestEnd; V >= 0; V = Previous[size_t(V)])
		{
			Out.push_back(Points[size_t(V)]);
		}
		Out.push_back(From.Point);
		std::reverse(Out.begin(), Out.end());
		// Drop repeats where a click landed on a corner.
		std::vector<Vec3> Clean;
		for (const Vec3& P : Out)
		{
			if (Clean.empty() || Distance(Clean.back(), P) > 1e-6)
			{
				Clean.push_back(P);
			}
		}
		return Clean;
	}

	std::vector<Vec3> EdgeGraph::Chain(const Snap& At, double MaxCornerDegrees) const
	{
		if (At.Edge < 0 || size_t(At.Edge) >= Edges.size())
		{
			return {};
		}
		const double CornerCos = std::cos(Radians(MaxCornerDegrees));
		std::vector<bool> Used(Edges.size(), false);
		Used[size_t(At.Edge)] = true;
		// Walks on from the last point of Run, taking the straightest unused edge that turns little enough.
		auto Extend = [&](std::vector<uint32_t>& Run)
		{
			while (Run.back() != Run.front() || Run.size() < 3)
			{
				const uint32_t Joint = Run.back();
				const Vec3 In = Unit(Points[Joint] - Points[Run[Run.size() - 2]]);
				int64_t Choice = -1;
				double ChoiceCos = CornerCos;
				for (uint32_t Index : AtVertex[Joint])
				{
					if (Used[Index])
					{
						continue;
					}
					const uint32_t Next = Edges[Index].A == Joint ? Edges[Index].B : Edges[Index].A;
					const double Turn = Dot(In, Unit(Points[Next] - Points[Joint]));
					if (Turn >= ChoiceCos)
					{
						Choice = Index;
						ChoiceCos = Turn;
					}
				}
				if (Choice < 0)
				{
					return;
				}
				Used[size_t(Choice)] = true;
				Run.push_back(Edges[size_t(Choice)].A == Joint ? Edges[size_t(Choice)].B : Edges[size_t(Choice)].A);
			}
		};
		const Edge& E = Edges[size_t(At.Edge)];
		std::vector<uint32_t> Forward{E.A, E.B};
		Extend(Forward);
		if (Forward.size() > 3 && Forward.back() == Forward.front())
		{
			std::vector<Vec3> Ring;
			for (uint32_t V : Forward)
			{
				Ring.push_back(Points[V]);
			}
			return Ring;
		}
		std::vector<uint32_t> Backward{E.B, E.A};
		Extend(Backward);
		// Backward runs B, A, then on past A; Forward starts at A.
		std::vector<Vec3> Out;
		for (size_t I = Backward.size(); I-- > 2;)
		{
			Out.push_back(Points[Backward[I]]);
		}
		for (uint32_t V : Forward)
		{
			Out.push_back(Points[V]);
		}
		return Out;
	}
}
