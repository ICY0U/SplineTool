#include "AutoGrindCore.h"
#include "AutoGrindMath.h"

#include <array>
#include <unordered_map>

namespace AutoGrindCore
{
	using namespace Detail;

	namespace
	{
		struct CellKeyHash
		{
			size_t operator()(const std::array<int64_t, 3>& K) const
			{
				return size_t(K[0] * 73856093) ^ size_t(K[1] * 19349663) ^ size_t(K[2] * 83492791);
			}
		};

		// Douglas-Peucker between First and Last, marking the points to keep.
		void SimplifyRange(const std::vector<Vec3>& In, size_t First, size_t Last, double Tolerance, std::vector<bool>& Keep)
		{
			double Worst = 0;
			size_t WorstIndex = First;
			for (size_t I = First + 1; I < Last; ++I)
			{
				const double D = PointSegmentDistance(In[I], In[First], In[Last]);
				if (D > Worst)
				{
					Worst = D;
					WorstIndex = I;
				}
			}
			if (Worst > Tolerance)
			{
				Keep[WorstIndex] = true;
				SimplifyRange(In, First, WorstIndex, Tolerance, Keep);
				SimplifyRange(In, WorstIndex, Last, Tolerance, Keep);
			}
		}

		// The point between First and Last farthest from the straight line joining them.
		size_t Farthest(const std::vector<Vec3>& In, size_t First, size_t Last)
		{
			double Worst = -1;
			size_t WorstIndex = First;
			for (size_t I = First + 1; I < Last; ++I)
			{
				const double D = PointSegmentDistance(In[I], In[First], In[Last]);
				if (D > Worst)
				{
					Worst = D;
					WorstIndex = I;
				}
			}
			return WorstIndex;
		}
	}

	namespace Detail
	{
		std::vector<Vec3> Weld(const std::vector<Vec3>& Vertices, double Tolerance, std::vector<uint32_t>& Remap)
		{
			std::vector<Vec3> Welded;
			std::unordered_map<std::array<int64_t, 3>, std::vector<uint32_t>, CellKeyHash> Grid;
			Remap.resize(Vertices.size());
			const double Cell = std::max(Tolerance, 1e-6);
			for (size_t I = 0; I < Vertices.size(); ++I)
			{
				const Vec3& P = Vertices[I];
				const int64_t X = int64_t(std::floor(P.X / Cell));
				const int64_t Y = int64_t(std::floor(P.Y / Cell));
				const int64_t Z = int64_t(std::floor(P.Z / Cell));
				int64_t Found = -1;
				for (int64_t OX = -1; OX <= 1 && Found < 0; ++OX)
				{
					for (int64_t OY = -1; OY <= 1 && Found < 0; ++OY)
					{
						for (int64_t OZ = -1; OZ <= 1 && Found < 0; ++OZ)
						{
							const auto Cell3 = Grid.find({X + OX, Y + OY, Z + OZ});
							if (Cell3 == Grid.end())
							{
								continue;
							}
							for (uint32_t Candidate : Cell3->second)
							{
								if (Size(Welded[Candidate] - P) <= Tolerance)
								{
									Found = Candidate;
									break;
								}
							}
						}
					}
				}
				if (Found < 0)
				{
					Found = int64_t(Welded.size());
					Welded.push_back(P);
					Grid[{X, Y, Z}].push_back(uint32_t(Found));
				}
				Remap[I] = uint32_t(Found);
			}
			return Welded;
		}

		std::optional<double> SegmentHitsTriangle(const Vec3& From, const Vec3& To, const Vec3& A, const Vec3& B, const Vec3& C)
		{
			const Vec3 Along = To - From;
			const Vec3 E1 = B - A;
			const Vec3 E2 = C - A;
			const Vec3 P = Cross(Along, E2);
			const double Det = Dot(E1, P);
			// Parallel to the plane, to within rounding, relative to the sizes involved: a segment lying in the
			// plane, as one run along an edge lies in the side face below it, passes along the face, not through.
			if (std::abs(Det) <= 1e-7 * Size(Along) * Size(E1) * Size(E2))
			{
				return std::nullopt;
			}
			const double Inverse = 1 / Det;
			const Vec3 S = From - A;
			const double U = Dot(S, P) * Inverse;
			if (U < 0 || U > 1)
			{
				return std::nullopt;
			}
			const Vec3 Q = Cross(S, E1);
			const double V = Dot(Along, Q) * Inverse;
			if (V < 0 || U + V > 1)
			{
				return std::nullopt;
			}
			const double T = Dot(E2, Q) * Inverse;
			return T >= 0 && T <= 1 ? std::optional<double>(T) : std::nullopt;
		}

		namespace
		{
			struct TripleKey
			{
				uint32_t V[3];
				bool operator==(const TripleKey& Other) const { return V[0] == Other.V[0] && V[1] == Other.V[1] && V[2] == Other.V[2]; }
			};
			struct TripleHash
			{
				size_t operator()(const TripleKey& K) const { return size_t(K.V[0]) * 73856093u ^ size_t(K.V[1]) * 19349663u ^ size_t(K.V[2]) * 83492791u; }
			};

			// Across each edge two faces share and no other does, a consistently wound surface runs the edge
			// one way in one face and the other way in the other. Each connected surface is made consistent,
			// keeping the winding most of its area already has, so a stray flipped triangle is turned round
			// without turning inside out a surface meant to face inwards, such as a tunnel's.
			void RepairWinding(std::vector<Face>& Faces, EdgeMap& Edges)
			{
				std::vector<int8_t> Flip(Faces.size(), -1);
				std::vector<uint32_t> Component;
				std::vector<uint32_t> Queue;
				for (uint32_t Seed = 0; Seed < Faces.size(); ++Seed)
				{
					if (Flip[Seed] >= 0)
					{
						continue;
					}
					Component.clear();
					Queue.assign(1, Seed);
					Flip[Seed] = 0;
					while (!Queue.empty())
					{
						const uint32_t F = Queue.back();
						Queue.pop_back();
						Component.push_back(F);
						for (size_t K = 0; K < 3; ++K)
						{
							const uint32_t A = Faces[F].V[K];
							const uint32_t B = Faces[F].V[(K + 1) % 3];
							const std::vector<uint32_t>& Shared = Edges[EdgeKey(A, B)];
							if (Shared.size() != 2)
							{
								continue;
							}
							const uint32_t G = Shared[0] == F ? Shared[1] : Shared[0];
							if (Flip[G] >= 0)
							{
								continue;
							}
							// F runs A to B, or B to A once flipped; G must run the other way.
							const bool bForward = Flip[F] == 0;
							Flip[G] = Runs(Faces[G], bForward ? A : B, bForward ? B : A) ? 1 : 0;
							Queue.push_back(G);
						}
					}
					double Total = 0;
					double Flipped = 0;
					for (uint32_t F : Component)
					{
						Total += Faces[F].Area;
						Flipped += Flip[F] == 1 ? Faces[F].Area : 0;
					}
					if (Flipped > Total / 2)
					{
						for (uint32_t F : Component)
						{
							Flip[F] = Flip[F] == 1 ? 0 : 1;
						}
					}
				}
				for (uint32_t F = 0; F < Faces.size(); ++F)
				{
					if (Flip[F] == 1)
					{
						std::swap(Faces[F].V[1], Faces[F].V[2]);
						Faces[F].Normal = Faces[F].Normal * -1;
					}
				}
			}
		}

		void BuildFaces(const std::vector<Vec3>& Points, const std::vector<uint32_t>& Corners, bool bRepair, std::vector<Face>& OutFaces, EdgeMap& OutEdges)
		{
			OutFaces.clear();
			OutEdges.clear();
			std::unordered_map<TripleKey, uint32_t, TripleHash> Seen;
			for (size_t I = 0; I + 2 < Corners.size(); I += 3)
			{
				Face F;
				for (size_t K = 0; K < 3; ++K)
				{
					F.V[K] = Corners[I + K];
				}
				if (F.V[0] == F.V[1] || F.V[1] == F.V[2] || F.V[0] == F.V[2] || F.V[0] >= Points.size() || F.V[1] >= Points.size() || F.V[2] >= Points.size())
				{
					continue;
				}
				const Vec3 N = Cross(Points[F.V[1]] - Points[F.V[0]], Points[F.V[2]] - Points[F.V[0]]);
				F.Area = Size(N) / 2;
				if (F.Area < 1e-6)
				{
					continue;
				}
				F.Normal = Unit(N);
				if (bRepair)
				{
					TripleKey Key{{F.V[0], F.V[1], F.V[2]}};
					std::sort(std::begin(Key.V), std::end(Key.V));
					const auto Existing = Seen.find(Key);
					if (Existing != Seen.end())
					{
						Face& Kept = OutFaces[Existing->second];
						if (Dot(Kept.Normal, F.Normal) < 0 && F.Normal.Z > Kept.Normal.Z)
						{
							Kept = F;
						}
						continue;
					}
					Seen.emplace(Key, uint32_t(OutFaces.size()));
				}
				OutFaces.push_back(F);
			}
			for (uint32_t I = 0; I < OutFaces.size(); ++I)
			{
				for (size_t K = 0; K < 3; ++K)
				{
					OutEdges[EdgeKey(OutFaces[I].V[K], OutFaces[I].V[(K + 1) % 3])].push_back(I);
				}
			}
			if (bRepair)
			{
				RepairWinding(OutFaces, OutEdges);
			}
		}
	}

	namespace
	{
		// Twice the signed area of a triangle seen from above; zero for a wall.
		template <typename TriangleType>
		double PlanDeterminant(const TriangleType& T)
		{
			return (T.B.Y - T.C.Y) * (T.A.X - T.C.X) + (T.C.X - T.B.X) * (T.A.Y - T.C.Y);
		}

		double Axis(const Vec3& V, int Index) { return Index == 0 ? V.X : Index == 1 ? V.Y : V.Z; }

		// Where a segment, From + (To - From) * T for T in [Near, Far], is inside a box; false when it misses.
		bool ClipToBox(const Vec3& From, const Vec3& Along, const Vec3& Min, const Vec3& Max, double& Near, double& Far)
		{
			for (int I = 0; I < 3; ++I)
			{
				const double Start = Axis(From, I);
				const double Step = Axis(Along, I);
				const double Low = Axis(Min, I);
				const double High = Axis(Max, I);
				if (std::abs(Step) < 1e-12)
				{
					if (Start < Low || Start > High)
					{
						return false;
					}
					continue;
				}
				double T0 = (Low - Start) / Step;
				double T1 = (High - Start) / Step;
				if (T0 > T1)
				{
					std::swap(T0, T1);
				}
				Near = std::max(Near, T0);
				Far = std::min(Far, T1);
				if (Near > Far)
				{
					return false;
				}
			}
			return true;
		}
	}

	TriangleField::TriangleField(const std::vector<Mesh>& Meshes, double InCellSize)
		: CellSize(InCellSize)
	{
		size_t Count = 0;
		for (const Mesh& M : Meshes)
		{
			Count += M.Indices.size() / 3;
		}
		Triangles.reserve(Count);
		std::vector<double> Footprints;
		for (size_t MeshIndex = 0; MeshIndex < Meshes.size(); ++MeshIndex)
		{
			const Mesh& M = Meshes[MeshIndex];
			for (size_t I = 0; I + 2 < M.Indices.size(); I += 3)
			{
				if (M.Indices[I] >= M.Vertices.size() || M.Indices[I + 1] >= M.Vertices.size() || M.Indices[I + 2] >= M.Vertices.size())
				{
					continue;
				}
				const Triangle T{M.Vertices[M.Indices[I]], M.Vertices[M.Indices[I + 1]], M.Vertices[M.Indices[I + 2]], MeshIndex, uint32_t(I / 3)};
				if (std::abs(PlanDeterminant(T)) >= 1e-9)
				{
					const double Width = std::max({T.A.X, T.B.X, T.C.X}) - std::min({T.A.X, T.B.X, T.C.X});
					const double Depth = std::max({T.A.Y, T.B.Y, T.C.Y}) - std::min({T.A.Y, T.B.Y, T.C.Y});
					Footprints.push_back(std::max(Width, 0.5) * std::max(Depth, 0.5));
				}
				Triangles.push_back(T);
			}
		}
		// The probe straight down searches a grid seen from above, leaving out walls, which it never meets.
		if (CellSize <= 0)
		{
			// About this many typical triangles' plan footprints per cell, within sensible sizes. The median
			// footprint, so a road or floor of a few huge triangles does not make every cell huge.
			constexpr double PerCell = 12;
			double Typical = 100 * 100 / PerCell;
			if (!Footprints.empty())
			{
				std::nth_element(Footprints.begin(), Footprints.begin() + std::ptrdiff_t(Footprints.size() / 2), Footprints.end());
				Typical = Footprints[Footprints.size() / 2];
			}
			CellSize = std::clamp(std::sqrt(Typical * PerCell), 4.0, 100.0);
		}
		// A triangle spanning more cells than this (a road, a whole floor) is kept aside and always tested.
		constexpr int64_t MaxCellsPerTriangle = 1024;
		for (uint32_t Index = 0; Index < Triangles.size(); ++Index)
		{
			const Triangle& T = Triangles[Index];
			if (std::abs(PlanDeterminant(T)) < 1e-9)
			{
				continue;
			}
			const int64_t X0 = int64_t(std::floor(std::min({T.A.X, T.B.X, T.C.X}) / CellSize));
			const int64_t X1 = int64_t(std::floor(std::max({T.A.X, T.B.X, T.C.X}) / CellSize));
			const int64_t Y0 = int64_t(std::floor(std::min({T.A.Y, T.B.Y, T.C.Y}) / CellSize));
			const int64_t Y1 = int64_t(std::floor(std::max({T.A.Y, T.B.Y, T.C.Y}) / CellSize));
			if ((X1 - X0 + 1) * (Y1 - Y0 + 1) > MaxCellsPerTriangle)
			{
				PlanBig.push_back(Index);
				continue;
			}
			for (int64_t X = X0; X <= X1; ++X)
			{
				for (int64_t Y = Y0; Y <= Y1; ++Y)
				{
					PlanCells[CellKey((double(X) + 0.5) * CellSize, (double(Y) + 0.5) * CellSize)].push_back(Index);
				}
			}
		}
		BuildTree();
	}

	// Segments and rays search a bounding volume hierarchy: boxes split along their longest side at the
	// middle triangle, down to a few triangles each, so a wall of dense triangles costs a few box tests.
	void TriangleField::BuildTree()
	{
		Order.resize(Triangles.size());
		std::vector<Vec3> Centres(Triangles.size());
		for (uint32_t I = 0; I < Triangles.size(); ++I)
		{
			Order[I] = I;
			const Triangle& T = Triangles[I];
			Centres[I] = (T.A + T.B + T.C) * (1.0 / 3);
		}
		Nodes.clear();
		if (Triangles.empty())
		{
			return;
		}
		Nodes.reserve(2 * Triangles.size() / LeafSize + 1);
		Nodes.push_back({});
		Nodes[0].Start = 0;
		Nodes[0].Count = uint32_t(Triangles.size());
		std::vector<uint32_t> Pending{0};
		while (!Pending.empty())
		{
			const uint32_t Index = Pending.back();
			Pending.pop_back();
			const uint32_t Start = Nodes[Index].Start;
			const uint32_t Count = Nodes[Index].Count;
			Vec3 Min{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
			Vec3 Max = Min * -1;
			Vec3 CentreMin = Min;
			Vec3 CentreMax = Max;
			for (uint32_t K = Start; K < Start + Count; ++K)
			{
				const Triangle& T = Triangles[Order[K]];
				for (const Vec3* P : {&T.A, &T.B, &T.C})
				{
					Min = {std::min(Min.X, P->X), std::min(Min.Y, P->Y), std::min(Min.Z, P->Z)};
					Max = {std::max(Max.X, P->X), std::max(Max.Y, P->Y), std::max(Max.Z, P->Z)};
				}
				const Vec3& C = Centres[Order[K]];
				CentreMin = {std::min(CentreMin.X, C.X), std::min(CentreMin.Y, C.Y), std::min(CentreMin.Z, C.Z)};
				CentreMax = {std::max(CentreMax.X, C.X), std::max(CentreMax.Y, C.Y), std::max(CentreMax.Z, C.Z)};
			}
			// A little slack, so a segment grazing a box face is not lost to rounding.
			const double Slack = 1e-6 * std::max(1.0, Size(Max - Min));
			Nodes[Index].Min = Min - Vec3{Slack, Slack, Slack};
			Nodes[Index].Max = Max + Vec3{Slack, Slack, Slack};
			const Vec3 Spread = CentreMax - CentreMin;
			if (Count <= LeafSize || std::max({Spread.X, Spread.Y, Spread.Z}) <= 0)
			{
				continue;
			}
			const int Split = Spread.X >= Spread.Y && Spread.X >= Spread.Z ? 0 : (Spread.Y >= Spread.Z ? 1 : 2);
			const uint32_t Half = Count / 2;
			std::nth_element(Order.begin() + Start, Order.begin() + Start + Half, Order.begin() + Start + Count,
				[&](uint32_t L, uint32_t R) { return Axis(Centres[L], Split) < Axis(Centres[R], Split); });
			const uint32_t Child = uint32_t(Nodes.size());
			Nodes.push_back({});
			Nodes.push_back({});
			Nodes[Child].Start = Start;
			Nodes[Child].Count = Half;
			Nodes[Child + 1].Start = Start + Half;
			Nodes[Child + 1].Count = Count - Half;
			Nodes[Index].Child = Child;
			Nodes[Index].Count = 0;
			Pending.push_back(Child);
			Pending.push_back(Child + 1);
		}
	}

	std::optional<std::pair<double, uint32_t>> TriangleField::Nearest(const Vec3& From, const Vec3& To) const
	{
		if (Nodes.empty())
		{
			return std::nullopt;
		}
		const Vec3 Along = To - From;
		double BestT = std::numeric_limits<double>::max();
		uint32_t BestTriangle = 0;
		// The tree is searched nearest box first, skipping boxes that begin beyond the best hit so far.
		std::vector<std::pair<uint32_t, double>> Pending;
		Pending.reserve(64);
		double Near = 0, Far = 1;
		if (!ClipToBox(From, Along, Nodes[0].Min, Nodes[0].Max, Near, Far))
		{
			return std::nullopt;
		}
		Pending.push_back({0, Near});
		while (!Pending.empty())
		{
			const auto [Index, Enter] = Pending.back();
			Pending.pop_back();
			if (Enter > BestT)
			{
				continue;
			}
			const Node& N = Nodes[Index];
			if (N.Count > 0)
			{
				for (uint32_t K = N.Start; K < N.Start + N.Count; ++K)
				{
					const Triangle& T = Triangles[Order[K]];
					const std::optional<double> Hit = SegmentHitsTriangle(From, To, T.A, T.B, T.C);
					if (Hit && *Hit < BestT)
					{
						BestT = *Hit;
						BestTriangle = Order[K];
					}
				}
				continue;
			}
			double NearA = 0, FarA = std::min(1.0, BestT), NearB = 0, FarB = std::min(1.0, BestT);
			const bool bA = ClipToBox(From, Along, Nodes[N.Child].Min, Nodes[N.Child].Max, NearA, FarA);
			const bool bB = ClipToBox(From, Along, Nodes[N.Child + 1].Min, Nodes[N.Child + 1].Max, NearB, FarB);
			// Push the farther first so the nearer is searched first.
			if (bA && bB)
			{
				if (NearA <= NearB)
				{
					Pending.push_back({N.Child + 1, NearB});
					Pending.push_back({N.Child, NearA});
				}
				else
				{
					Pending.push_back({N.Child, NearA});
					Pending.push_back({N.Child + 1, NearB});
				}
			}
			else if (bA)
			{
				Pending.push_back({N.Child, NearA});
			}
			else if (bB)
			{
				Pending.push_back({N.Child + 1, NearB});
			}
		}
		if (BestT > 1)
		{
			return std::nullopt;
		}
		return std::pair<double, uint32_t>{BestT, BestTriangle};
	}

	uint64_t TriangleField::CellKey(double X, double Y) const
	{
		const int64_t CX = int64_t(std::floor(X / CellSize));
		const int64_t CY = int64_t(std::floor(Y / CellSize));
		return (uint64_t(uint32_t(int32_t(CX))) << 32) | uint32_t(int32_t(CY));
	}

	std::optional<double> TriangleField::Below(const Vec3& From, double MaxDistance) const
	{
		std::optional<double> Best;
		const auto Cell = PlanCells.find(CellKey(From.X, From.Y));
		const std::vector<uint32_t>* Lists[2] = {&PlanBig, Cell != PlanCells.end() ? &Cell->second : nullptr};
		for (const std::vector<uint32_t>* List : Lists)
		{
			if (!List)
			{
				continue;
			}
			for (uint32_t Index : *List)
			{
				const Triangle& T = Triangles[Index];
				// Barycentric coordinates of From in the triangle's plan view.
				const double D = PlanDeterminant(T);
				const double U = ((T.B.Y - T.C.Y) * (From.X - T.C.X) + (T.C.X - T.B.X) * (From.Y - T.C.Y)) / D;
				const double V = ((T.C.Y - T.A.Y) * (From.X - T.C.X) + (T.A.X - T.C.X) * (From.Y - T.C.Y)) / D;
				const double W = 1 - U - V;
				constexpr double Slack = -1e-7;
				if (U < Slack || V < Slack || W < Slack)
				{
					continue;
				}
				const double Z = U * T.A.Z + V * T.B.Z + W * T.C.Z;
				if (Z <= From.Z && Z >= From.Z - MaxDistance && (!Best || Z > *Best))
				{
					Best = Z;
				}
			}
		}
		return Best;
	}

	std::optional<SegmentHit> TriangleField::FirstHit(const Vec3& From, const Vec3& To) const
	{
		const std::optional<std::pair<double, uint32_t>> Hit = Nearest(From, To);
		if (!Hit)
		{
			return std::nullopt;
		}
		const Triangle& T = Triangles[Hit->second];
		return SegmentHit{Hit->first, Cross(T.B - T.A, T.C - T.A)};
	}

	std::optional<RayHit> TriangleField::Raycast(const Vec3& Origin, const Vec3& Direction, double MaxDistance) const
	{
		const Vec3 Ray = Unit(Direction);
		if (Size(Ray) == 0 || MaxDistance <= 0)
		{
			return std::nullopt;
		}
		const std::optional<std::pair<double, uint32_t>> Hit = Nearest(Origin, Origin + Ray * MaxDistance);
		if (!Hit)
		{
			return std::nullopt;
		}
		const Triangle& T = Triangles[Hit->second];
		const double Distance = Hit->first * MaxDistance;
		return RayHit{Distance, Origin + Ray * Distance, Unit(Cross(T.B - T.A, T.C - T.A)), T.Mesh, T.Index};
	}

	double PolylineLength(const std::vector<Vec3>& Points)
	{
		double Length = 0;
		for (size_t I = 0; I + 1 < Points.size(); ++I)
		{
			Length += Size(Points[I + 1] - Points[I]);
		}
		return Length;
	}

	std::vector<Vec3> SimplifyPolyline(const std::vector<Vec3>& Points, double Tolerance, bool bClosed)
	{
		if (Points.size() < 3)
		{
			return Points;
		}
		std::vector<bool> Keep(Points.size(), false);
		Keep.front() = Keep.back() = true;
		if (bClosed && Points.size() >= 4)
		{
			// Split the loop at the point farthest from its seam and keep a corner either side, so even an
			// extreme tolerance leaves three distinct points round it.
			size_t Far = 1;
			double FarDistance = -1;
			for (size_t I = 1; I + 1 < Points.size(); ++I)
			{
				const double D = Size(Points[I] - Points.front());
				if (D > FarDistance)
				{
					FarDistance = D;
					Far = I;
				}
			}
			Keep[Far] = true;
			const size_t Before = Farthest(Points, 0, Far);
			const size_t After = Farthest(Points, Far, Points.size() - 1);
			Keep[Before] = Keep[After] = true;
			for (const auto& [First, Last] : {std::pair<size_t, size_t>{0, Before}, {Before, Far}, {Far, After}, {After, Points.size() - 1}})
			{
				if (Last > First)
				{
					SimplifyRange(Points, First, Last, Tolerance, Keep);
				}
			}
		}
		else
		{
			SimplifyRange(Points, 0, Points.size() - 1, Tolerance, Keep);
		}
		std::vector<Vec3> Out;
		for (size_t I = 0; I < Points.size(); ++I)
		{
			if (Keep[I] && (Out.empty() || Size(Points[I] - Out.back()) > 1e-6))
			{
				Out.push_back(Points[I]);
			}
		}
		if (Out.size() < 2)
		{
			return Points;
		}
		return Out;
	}

	std::optional<PolylinePoint> NearestOnPolyline(const std::vector<Vec3>& Points, const Vec3& P)
	{
		if (Points.empty())
		{
			return std::nullopt;
		}
		PolylinePoint Best{Points.front(), Size(P - Points.front()), 0};
		double Walked = 0;
		for (size_t I = 0; I + 1 < Points.size(); ++I)
		{
			double T = 0;
			const double D = PointSegmentDistance(P, Points[I], Points[I + 1], &T);
			const double Length = Size(Points[I + 1] - Points[I]);
			if (D < Best.Distance)
			{
				Best = {Lerp(Points[I], Points[I + 1], T), D, Walked + Length * T};
			}
			Walked += Length;
		}
		return Best;
	}

	namespace
	{
		Vec3 PointAlong(const std::vector<Vec3>& Points, double Along, size_t* OutSegment)
		{
			double Walked = 0;
			for (size_t I = 0; I + 1 < Points.size(); ++I)
			{
				const double Length = Size(Points[I + 1] - Points[I]);
				if (Walked + Length >= Along || I + 2 == Points.size())
				{
					if (OutSegment)
					{
						*OutSegment = I;
					}
					return Lerp(Points[I], Points[I + 1], Length > 0 ? std::clamp((Along - Walked) / Length, 0.0, 1.0) : 0.0);
				}
				Walked += Length;
			}
			if (OutSegment)
			{
				*OutSegment = 0;
			}
			return Points.empty() ? Vec3{} : Points.front();
		}

		// From <= To on an open polyline.
		std::vector<Vec3> Forward(const std::vector<Vec3>& Points, double From, double To)
		{
			size_t FirstSegment = 0, LastSegment = 0;
			std::vector<Vec3> Out{PointAlong(Points, From, &FirstSegment)};
			const Vec3 End = PointAlong(Points, To, &LastSegment);
			for (size_t I = FirstSegment + 1; I <= LastSegment && I < Points.size(); ++I)
			{
				if (Size(Points[I] - Out.back()) > 1e-6)
				{
					Out.push_back(Points[I]);
				}
			}
			if (Size(End - Out.back()) > 1e-6 || Out.size() == 1)
			{
				Out.push_back(End);
			}
			return Out;
		}
	}

	std::vector<Vec3> SubPolyline(const std::vector<Vec3>& Points, double From, double To, bool bClosed)
	{
		if (Points.size() < 2)
		{
			return Points;
		}
		const double Length = PolylineLength(Points);
		From = std::clamp(From, 0.0, Length);
		To = std::clamp(To, 0.0, Length);
		if (bClosed && Length > 0)
		{
			// Go the shorter way round: forwards past the seam when that is shorter.
			const double Ahead = To >= From ? To - From : Length - From + To;
			if (Ahead <= Length - Ahead)
			{
				if (To >= From)
				{
					return Forward(Points, From, To);
				}
				std::vector<Vec3> Out = Forward(Points, From, Length);
				const std::vector<Vec3> Rest = Forward(Points, 0, To);
				Out.insert(Out.end(), Rest.begin() + 1, Rest.end());
				return Out;
			}
			std::vector<Vec3> Out = SubPolyline(Points, To, From, true);
			std::reverse(Out.begin(), Out.end());
			return Out;
		}
		if (From <= To)
		{
			return Forward(Points, From, To);
		}
		std::vector<Vec3> Out = Forward(Points, To, From);
		std::reverse(Out.begin(), Out.end());
		return Out;
	}

	const char* Describe(Reject Reason)
	{
		switch (Reason)
		{
		case Reject::TooSteep: return "steeper than the maximum line slope";
		case Reject::InsideCorner: return "the neighbouring face rises: an inside corner";
		case Reject::Covered: return "another part covers the top";
		case Reject::Buried: return "the edge is inside another part";
		case Reject::Walled: return "a wall or part stands over, beside or across the edge";
		case Reject::Seam: return "a flat surface carries on just past the edge: a seam, step or slat gap";
		case Reject::NoSurface: return "no surface below the edge";
		case Reject::Obstructed: return "something stands just past the edge";
		case Reject::SmallFall: return "the fall past the edge is too small";
		case Reject::TooHigh: return "the fall past the edge is more than Max Drop: a roof, wall top or cliff";
		case Reject::Stair: return "a stair step: another step lies just past it or rises just behind it";
		case Reject::RoundedEdge: return "the surface rounds over smoothly: not a sharp convex edge";
		case Reject::Unpaired: return "one side of a narrow top that falls too little for a ledge and pairs with no other side";
		case Reject::RampSide: return "the edge climbs out of level ground: the side of a ramp, bank or kicker";
		case Reject::Short: return "shorter than the minimum length";
		case Reject::Count: break;
		}
		return "turned down";
	}

	const char* SettingFor(Reject Reason)
	{
		switch (Reason)
		{
		case Reject::TooSteep: return "Max Line Slope";
		case Reject::InsideCorner: return "Max Top Slope";
		case Reject::Covered:
		case Reject::Buried:
		case Reject::Walled: return "Check Walls";
		case Reject::Seam: return "Gap Bridge";
		case Reject::NoSurface: return "Max Drop Search";
		case Reject::Obstructed: return "Probe Distance";
		case Reject::SmallFall: return "Min Drop, Low Ledges";
		case Reject::TooHigh: return "Max Drop";
		case Reject::Stair: return "Reject Stairs";
		case Reject::RoundedEdge: return "Min Edge Angle";
		case Reject::Unpaired: return "Rail Min Drop";
		case Reject::RampSide: return "Max Line Slope";
		case Reject::Short: return "Min Length";
		case Reject::Count: break;
		}
		return "";
	}

	const char* DescribeNote(uint32_t Note)
	{
		switch (Note)
		{
		case Notes::LowLedge: return "low ledge or curb";
		case Notes::HighDrop: return "high drop: the back of a deck or a wall top?";
		case Notes::SoftEdge: return "rounded edge, only just sharp enough";
		case Notes::PartialRail: return "only part of the top pairs into a rail";
		case Notes::Ridge: return "a ridge with no top to stand on";
		case Notes::RoundTop: return "along the crest of a wide round top";
		case Notes::Joined: return "joined across meshes";
		case Notes::SteepLip: return "a bank or quarter-pipe lip";
		case Notes::Short: return "short";
		case Notes::Coping: return "coping: a deck or ledge sits just below one side";
		default: break;
		}
		return "";
	}
}
