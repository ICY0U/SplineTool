#include "AutoGrindCore.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace AutoGrindCore
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		// How far inside an edge its top is checked for something stacked on it, and the height that counts.
		constexpr double CoverInset = 2;
		// How much further past the probe the landing surface's slope is measured.
		constexpr double LandingStep = 5;
		// Where past an edge a seam or slat gap is looked for, and the step over which the surface found
		// there must stay flat. The first distance clears a rounded or bevelled edge's own fall.
		constexpr double GapFirst = 2;
		constexpr double GapStep = 1.5;
		constexpr double FlatStep = 1;
		// A surface at most this far below the edge carries the top on; a deeper one is a fall, such as the
		// wall a rail runs above. It must also be about level: a tube's own sides slope at 45 degrees and more.
		constexpr double MaxSeamStep = 10;
		constexpr double CarryOnSlopeDegrees = 20;
		// Past the side of a narrow top only a surface this nearly level carries it on, as the next slat of a
		// bench does. Lower ground beside a rail is a fall, however small.
		constexpr double LevelStep = 2;
		// Height above an edge at which the space across and along it must be clear of walls, and how far
		// back over the top that space reaches: about where a skate's frame rests on the top.
		constexpr double BesideLift = 5;
		constexpr double SkateReach = 10;
		// How far up, and how far out past the edge, the first surface is looked for. Both met from behind,
		// they are the top and the far face of a part the edge is buried in. Up alone is not enough: a bridge
		// deck or a canopy overhead is often modelled with no underside.
		constexpr double BuriedReach = 2000;
		constexpr double BuriedSideReach = 300;
		// Rail sides are compared at points this far apart.
		constexpr double RailSampleSpacing = 5;
		constexpr double TopSlopeSlackDegrees = 0.5;

		Vec3 operator+(const Vec3& A, const Vec3& B) { return {A.X + B.X, A.Y + B.Y, A.Z + B.Z}; }
		Vec3 operator-(const Vec3& A, const Vec3& B) { return {A.X - B.X, A.Y - B.Y, A.Z - B.Z}; }
		Vec3 operator*(const Vec3& A, double S) { return {A.X * S, A.Y * S, A.Z * S}; }
		double Dot(const Vec3& A, const Vec3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		Vec3 Cross(const Vec3& A, const Vec3& B) { return {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X}; }
		double Size(const Vec3& A) { return std::sqrt(Dot(A, A)); }
		Vec3 Normal(const Vec3& A)
		{
			const double S = Size(A);
			return S > 0 ? A * (1 / S) : Vec3{};
		}
		Vec3 Lerp(const Vec3& A, const Vec3& B, double T) { return A + (B - A) * T; }
		double Radians(double Degrees) { return Degrees * Pi / 180; }

		double PointSegmentDistance(const Vec3& P, const Vec3& A, const Vec3& B)
		{
			const Vec3 AB = B - A;
			const double LengthSquared = Dot(AB, AB);
			const double T = LengthSquared > 0 ? std::clamp(Dot(P - A, AB) / LengthSquared, 0.0, 1.0) : 0.0;
			return Size(P - (A + AB * T));
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

		Vec3 Flat(const Vec3& A) { return {A.X, A.Y, 0}; }
		double FlatSize(const Vec3& A) { return std::sqrt(A.X * A.X + A.Y * A.Y); }

		// The point of a polyline nearest P seen from above, and the direction of the segment it lies on.
		struct Nearest
		{
			double Distance = std::numeric_limits<double>::max();
			Vec3 Point;
			Vec3 Direction;
		};

		Nearest NearestFromAbove(const Vec3& P, const std::vector<Vec3>& Points)
		{
			Nearest Best;
			for (size_t I = 0; I + 1 < Points.size(); ++I)
			{
				const Vec3 AB = Flat(Points[I + 1] - Points[I]);
				const double LengthSquared = Dot(AB, AB);
				const double T = LengthSquared > 0 ? std::clamp(Dot(Flat(P - Points[I]), AB) / LengthSquared, 0.0, 1.0) : 0.0;
				const Vec3 On = Lerp(Points[I], Points[I + 1], T);
				const double D = FlatSize(P - On);
				if (D < Best.Distance)
				{
					Best = {D, On, Normal(Points[I + 1] - Points[I])};
				}
			}
			return Best;
		}

		// A polyline cut into pieces no longer than Step, keeping its own points.
		struct Sample
		{
			Vec3 Point;
			Vec3 Outward;
			Vec3 Direction;
		};

		std::vector<Sample> Resample(const std::vector<Vec3>& Points, const std::vector<Vec3>& Outward, double Step)
		{
			std::vector<Sample> Out;
			for (size_t I = 0; I + 1 < Points.size(); ++I)
			{
				const Vec3 Direction = Normal(Points[I + 1] - Points[I]);
				const int Count = std::max(1, int(std::ceil(Size(Points[I + 1] - Points[I]) / Step)));
				for (int K = 0; K < Count; ++K)
				{
					const double T = double(K) / Count;
					Out.push_back({Lerp(Points[I], Points[I + 1], T), Normal(Lerp(Outward[I], Outward[I + 1], T)), Direction});
				}
			}
			if (!Points.empty())
			{
				Out.push_back({Points.back(), Outward.back(), Out.empty() ? Vec3{} : Out.back().Direction});
			}
			return Out;
		}

		// Moller-Trumbore, hitting a triangle from either side: how far along From -> To it is met.
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

		double Median(std::vector<double> Values)
		{
			if (Values.empty())
			{
				return 0;
			}
			std::sort(Values.begin(), Values.end());
			return Values[Values.size() / 2];
		}

		uint64_t EdgeKey(uint32_t A, uint32_t B)
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

		struct Face
		{
			uint32_t V[3] = {0, 0, 0};
			Vec3 Normal;
			double Area = 0;
			int32_t Patch = -1; // connected top faces; -1 when the face is not a top
		};

		// What a lip can become. A ledge line needs a firm lip. The side of a narrow top that falls less than a
		// ledge must, onto a wall top or the soil of a bed, can still be a side of a rail, though never a line
		// of its own; a side that falls less than RailMinDrop, or onto nothing, only confirms the other side.
		enum class Grade : uint8_t
		{
			Partner,
			Rail,
			Firm
		};

		// A top's outer edge that passed every test, oriented so the top lies on its left seen from above.
		struct Lip
		{
			uint32_t A = 0;
			uint32_t B = 0;
			int32_t Patch = -1;
			Vec3 Outward; // horizontal, away from the top
			double Drop = 0;
			Grade Strength = Grade::Firm;
		};

		struct Chain
		{
			std::vector<uint32_t> Vertices;
			std::vector<Vec3> Outward; // per vertex
			std::vector<double> Drops;
			int32_t Patch = -1;
			Grade Strength = Grade::Firm;
			bool bClosed = false;
		};

		struct CellKeyHash
		{
			size_t operator()(const std::array<int64_t, 3>& K) const
			{
				return size_t(K[0] * 73856093) ^ size_t(K[1] * 19349663) ^ size_t(K[2] * 83492791);
			}
		};

		// Merges vertices closer than Tolerance, so split normals and UV seams do not break the topology.
		std::vector<Vec3> Weld(const Mesh& In, double Tolerance, std::vector<uint32_t>& Remap)
		{
			std::vector<Vec3> Welded;
			std::unordered_map<std::array<int64_t, 3>, std::vector<uint32_t>, CellKeyHash> Grid;
			Remap.resize(In.Vertices.size());
			for (size_t I = 0; I < In.Vertices.size(); ++I)
			{
				const Vec3& P = In.Vertices[I];
				const int64_t X = int64_t(std::floor(P.X / Tolerance));
				const int64_t Y = int64_t(std::floor(P.Y / Tolerance));
				const int64_t Z = int64_t(std::floor(P.Z / Tolerance));
				int64_t Found = -1;
				for (int64_t DX = -1; DX <= 1 && Found < 0; ++DX)
				{
					for (int64_t DY = -1; DY <= 1 && Found < 0; ++DY)
					{
						for (int64_t DZ = -1; DZ <= 1 && Found < 0; ++DZ)
						{
							const auto Cell = Grid.find({X + DX, Y + DY, Z + DZ});
							if (Cell == Grid.end())
							{
								continue;
							}
							for (uint32_t Candidate : Cell->second)
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

		// Douglas-Peucker, keeping both ends.
		void Simplify(const std::vector<Vec3>& In, size_t First, size_t Last, double Tolerance, std::vector<bool>& Keep)
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
				Simplify(In, First, WorstIndex, Tolerance, Keep);
				Simplify(In, WorstIndex, Last, Tolerance, Keep);
			}
		}

		std::vector<Vec3> Simplified(const std::vector<Vec3>& In, double Tolerance)
		{
			if (In.size() < 3)
			{
				return In;
			}
			std::vector<bool> Keep(In.size(), false);
			Keep.front() = Keep.back() = true;
			Simplify(In, 0, In.size() - 1, Tolerance, Keep);
			std::vector<Vec3> Out;
			for (size_t I = 0; I < In.size(); ++I)
			{
				if (Keep[I])
				{
					Out.push_back(In[I]);
				}
			}
			return Out;
		}

		void FindInMesh(const Mesh& In, size_t MeshIndex, const SurfaceBelow& Below, const SegmentTest& FirstHit, const Settings& Config, std::vector<Line>& Out, std::vector<Rejection>* Rejections)
		{
			std::vector<uint32_t> Remap;
			std::vector<Vec3> Points = Weld(In, Config.WeldTolerance, Remap);

			std::vector<Face> Faces;
			for (size_t I = 0; I + 2 < In.Indices.size(); I += 3)
			{
				Face F;
				for (int32_t K = 0; K < 3; ++K)
				{
					F.V[K] = Remap[In.Indices[I + K]];
				}
				if (F.V[0] == F.V[1] || F.V[1] == F.V[2] || F.V[0] == F.V[2])
				{
					continue;
				}
				const Vec3 N = Cross(Points[F.V[1]] - Points[F.V[0]], Points[F.V[2]] - Points[F.V[0]]);
				F.Area = Size(N) / 2;
				if (F.Area < 1e-6)
				{
					continue;
				}
				F.Normal = Normal(N);
				Faces.push_back(F);
			}
			std::unordered_map<uint64_t, std::vector<uint32_t>> EdgeFaces;
			for (uint32_t I = 0; I < Faces.size(); ++I)
			{
				for (int32_t K = 0; K < 3; ++K)
				{
					EdgeFaces[EdgeKey(Faces[I].V[K], Faces[I].V[(K + 1) % 3])].push_back(I);
				}
			}

			// Tops, joined into patches wherever two tops share an edge. Half a degree of slack keeps a face
			// modelled at exactly the limit on one side of it: a 45 degree chamfer, or the matching facets
			// either side of a tube, whose rounding would otherwise split them between top and not.
			const double TopCos = std::cos(Radians(Config.MaxTopSlopeDegrees + TopSlopeSlackDegrees));
			auto IsTop = [&](uint32_t F) { return Faces[F].Normal.Z >= TopCos; };
			DisjointSet PatchSet(Faces.size());
			for (const auto& [Key, Shared] : EdgeFaces)
			{
				for (size_t I = 1; I < Shared.size(); ++I)
				{
					if (IsTop(Shared[0]) && IsTop(Shared[I]))
					{
						PatchSet.Join(Shared[0], Shared[I]);
					}
				}
			}
			std::unordered_map<uint32_t, int32_t> PatchIds;
			for (uint32_t F = 0; F < Faces.size(); ++F)
			{
				if (IsTop(F))
				{
					const uint32_t Root = PatchSet.Find(F);
					const auto Existing = PatchIds.find(Root);
					Faces[F].Patch = Existing != PatchIds.end() ? Existing->second : (PatchIds[Root] = int32_t(PatchIds.size()));
				}
			}

			// A patch's width, 2 x area / perimeter, tells a rail's narrow top from a ledge's broad one.
			std::vector<double> PatchArea(PatchIds.size(), 0);
			std::vector<double> PatchPerimeter(PatchIds.size(), 0);
			for (const Face& F : Faces)
			{
				if (F.Patch >= 0)
				{
					PatchArea[F.Patch] += F.Area;
				}
			}

			auto ThirdVertex = [&](uint32_t F, uint32_t A, uint32_t B)
			{
				for (uint32_t V : Faces[F].V)
				{
					if (V != A && V != B)
					{
						return V;
					}
				}
				return Faces[F].V[0];
			};

			const double MaxLineSlope = std::tan(Radians(Config.MaxLineSlopeDegrees));
			std::vector<double> GapDistances;
			for (double D = GapFirst; D < Config.GapBridge; D += GapStep)
			{
				GapDistances.push_back(D);
			}
			if (Config.GapBridge >= GapFirst)
			{
				GapDistances.push_back(Config.GapBridge);
			}
			const double FlatRise = FlatStep * std::tan(Radians(std::min(Config.MaxTopSlopeDegrees, CarryOnSlopeDegrees)));
			const double SeamStep = std::min(Config.MinDrop, MaxSeamStep);
			std::vector<Lip> Lips;
			for (const auto& [Key, Shared] : EdgeFaces)
			{
				const uint32_t A = uint32_t(Key >> 32);
				const uint32_t B = uint32_t(Key & 0xffffffff);
				for (uint32_t Top : Shared)
				{
					if (!IsTop(Top))
					{
						continue;
					}
					int32_t InPatch = 0;
					for (uint32_t F : Shared)
					{
						InPatch += Faces[F].Patch == Faces[Top].Patch ? 1 : 0;
					}
					if (InPatch == 1)
					{
						PatchPerimeter[Faces[Top].Patch] += Size(Points[B] - Points[A]);
					}
				}
			}
			auto PatchWidth = [&](int32_t Patch) { return PatchPerimeter[Patch] > 0 ? 2 * PatchArea[Patch] / PatchPerimeter[Patch] : 0.0; };
			auto Reject = [&](uint32_t A, uint32_t B, const char* Reason, double Drop)
			{
				if (Rejections)
				{
					Rejections->push_back({Points[A], Points[B], Reason, Drop, MeshIndex});
				}
			};
			for (const auto& [Key, Shared] : EdgeFaces)
			{
				const uint32_t A = uint32_t(Key >> 32);
				const uint32_t B = uint32_t(Key & 0xffffffff);
				// Only a top's outer edge is a candidate: edges between two tops are inside a patch.
				bool bHasTop = false;
				bool bInsidePatch = false;
				for (uint32_t F : Shared)
				{
					bInsidePatch |= bHasTop && IsTop(F);
					bHasTop |= IsTop(F);
				}
				if (!bHasTop || bInsidePatch)
				{
					continue;
				}
				const Vec3 Along = Points[B] - Points[A];
				const double Horizontal = std::sqrt(Along.X * Along.X + Along.Y * Along.Y);
				if (Horizontal <= 0 || std::abs(Along.Z) > MaxLineSlope * Horizontal)
				{
					Reject(A, B, "steeper than the maximum line slope", 0);
					continue;
				}
				for (uint32_t Top : Shared)
				{
					if (!IsTop(Top))
					{
						continue;
					}
					// Every other face on the edge must fall away below the top's plane.
					bool bConvex = true;
					for (uint32_t Other : Shared)
					{
						if (Other != Top && Dot(Points[ThirdVertex(Other, A, B)] - Points[A], Faces[Top].Normal) > -0.01)
						{
							bConvex = false;
							break;
						}
					}
					if (!bConvex)
					{
						Reject(A, B, "the neighbouring face rises: an inside corner", 0);
						continue;
					}
					// Outward is level and square to the edge seen from above, on the side away from the top. The
					// side comes from the top's winding: seen from above a top runs anticlockwise, so it lies left
					// of each of its edges in order. The third vertex cannot tell on a long, thin facet: along a
					// sloped rail or a curved tube its far end sits past the edge's line.
					bool bTopOnLeft = false;
					for (int32_t K = 0; K < 3; ++K)
					{
						bTopOnLeft |= Faces[Top].V[K] == A && Faces[Top].V[(K + 1) % 3] == B;
					}
					const Vec3 LeftOfEdge = Normal(Vec3{-Along.Y, Along.X, 0});
					const Vec3 Outward = bTopOnLeft ? LeftOfEdge * -1 : LeftOfEdge;
					// Just inside the edge the top must be the first surface from above: a top with another
					// part stacked on it is hidden. Just past the edge the surface must fall at least MinDrop,
					// with nothing standing above it, or fall steeply onto a steep slope (a bank's top edge).
					// A stair nosing falls as far as a steep lip but lands on a flat tread.
					const Vec3 EdgeStart = Points[A], EdgeEnd = Points[B];
					const uint32_t EdgeA = A, EdgeB = B;
					const int SpanCount = std::max(1, int(std::ceil(Size(Along) / std::max(1.0, Config.SampleSpacing))));
					uint32_t Previous = EdgeA;
					for (int Span = 0; Span < SpanCount; ++Span)
					{
						const uint32_t SpanEnd = Span + 1 == SpanCount ? EdgeB : uint32_t(Points.size());
						if (Span + 1 != SpanCount) Points.push_back(Lerp(EdgeStart, EdgeEnd, double(Span + 1) / SpanCount));
						const uint32_t SpanA = Previous, SpanB = SpanEnd;
						Previous = SpanEnd;
						const Vec3& Up = Faces[Top].Normal;
						const bool bNarrow = PatchWidth(Faces[Top].Patch) <= Config.RailMaxWidth;
						const double CarryStep = bNarrow ? std::min(SeamStep, LevelStep) : SeamStep;
						std::vector<double> Covers;
						std::vector<double> Drops;
						std::vector<double> LandingFalls;
						bool bWalled = false;
						bool bBuried = false;
						bool bCarriesOn = false;
						for (double T : {0.25, 0.5, 0.75})
						{
							const Vec3 OnEdge = Lerp(Points[SpanA], Points[SpanB], T);
							const Vec3 Inside = OnEdge - Outward * CoverInset;
							const double TopHeight = OnEdge.Z - (Up.X * (Inside.X - OnEdge.X) + Up.Y * (Inside.Y - OnEdge.Y)) / Up.Z;
							const std::optional<double> Surface = Below({Inside.X, Inside.Y, OnEdge.Z + Config.Clearance}, Config.Clearance + 2 * CoverInset);
							Covers.push_back(Surface ? *Surface - TopHeight : 0);
							const Vec3 Probe = OnEdge + Outward * Config.ProbeDistance;
							const std::optional<double> Hit = Below({Probe.X, Probe.Y, OnEdge.Z + Config.Clearance}, Config.Clearance + Config.MaxDropSearch);
							Drops.push_back(Hit ? OnEdge.Z - *Hit : -std::numeric_limits<double>::max());
							const Vec3 Further = Probe + Outward * LandingStep;
							const std::optional<double> FurtherHit = Below({Further.X, Further.Y, OnEdge.Z + Config.Clearance}, Config.Clearance + Config.MaxDropSearch);
							LandingFalls.push_back(Hit && FurtherHit ? *Hit - *FurtherHit : 0);
							// The space a skater grinds through: straight up from the edge, and across and along it just
							// above the top. The drop test cannot see a wall here when its traces start inside that wall.
							// Straight up, the first surface met from behind is the top of a part the edge is buried in.
							if (FirstHit && !bWalled && !bBuried)
							{
								const Vec3 Lifted = OnEdge + Vec3{0, 0, BesideLift};
								const Vec3 AlongEdge = Normal(Along);
								const std::optional<SegmentHit> Over = FirstHit(OnEdge + Vec3{0, 0, 1}, OnEdge + Vec3{0, 0, BuriedReach});
								if (Over && Over->Facing.Z > 0)
								{
									const std::optional<SegmentHit> Beyond = FirstHit(Lifted, Lifted + Outward * BuriedSideReach);
									bBuried = Beyond && Dot(Beyond->Facing, Outward) > 0;
								}
								bWalled = (Over && Over->T * (BuriedReach - 1) < Config.Clearance - 1)
									|| FirstHit(Lifted - Outward * SkateReach, Lifted + Outward * Config.ProbeDistance)
									|| FirstHit(Lifted - AlongEdge * SkateReach, Lifted + AlongEdge * SkateReach);
							}
							// A flat surface just past the edge, level with it or a small step below, carries the top on
							// across a seam, a step or the gap between slats. The first distance clears a rounded or
							// bevelled edge's own fall, and flatness tells that fall from a surface that carries on.
							for (double D : GapDistances)
							{
								const Vec3 Near = OnEdge + Outward * (D - FlatStep / 2);
								const std::optional<double> NearHit = Below({Near.X, Near.Y, OnEdge.Z + Config.Clearance}, Config.Clearance + CarryStep);
								if (!NearHit || OnEdge.Z - *NearHit >= CarryStep)
								{
									continue;
								}
								const Vec3 Next = OnEdge + Outward * (D + FlatStep / 2);
								const std::optional<double> NextHit = Below({Next.X, Next.Y, OnEdge.Z + Config.Clearance}, Config.Clearance + CarryStep + FlatRise);
								if (NextHit && std::abs(*NextHit - *NearHit) <= FlatRise)
								{
									bCarriesOn = true;
									break;
								}
							}
						}
						if (*std::max_element(Covers.begin(), Covers.end()) > CoverInset)
						{
							Reject(SpanA, SpanB, "another part covers the top", 0);
							continue;
						}
						if (bBuried)
						{
							Reject(SpanA, SpanB, "the edge is inside another part", 0);
							continue;
						}
						if (bWalled)
						{
							Reject(SpanA, SpanB, "a wall or part stands over, beside or across the edge", 0);
							continue;
						}
						if (bCarriesOn)
						{
							Reject(SpanA, SpanB, "a flat surface carries on just past the edge: a seam, step or slat gap", 0);
							continue;
						}
						const double Drop = *std::min_element(Drops.begin(), Drops.end());
						const double SteepFall = std::tan(Radians(Config.SteepLipDegrees));
						const bool bSteepLip = Config.SteepLipDegrees > 0 && Drop >= Config.ProbeDistance * SteepFall && *std::min_element(LandingFalls.begin(), LandingFalls.end()) >= LandingStep * SteepFall;
						const bool bNoSurface = Drop == -std::numeric_limits<double>::max();
						Grade Strength = Grade::Firm;
						if (Drop < Config.MinDrop && !bSteepLip)
						{
							if (!bNarrow || (Drop < 0 && !bNoSurface))
							{
								Reject(SpanA, SpanB, bNoSurface ? "no surface below the edge" : Drop < 0 ? "something stands just past the edge" : "the fall past the edge is too small", Drop);
								continue;
							}
							Strength = bNoSurface || Drop >= Config.RailMinDrop ? Grade::Rail : Grade::Partner;
						}
						// Orient SpanA -> SpanB with the top on the left seen from above, so chains run one way round.
						const bool bFlip = Cross(Along, Outward).Z < 0;
						Lips.push_back({bFlip ? SpanB : SpanA, bFlip ? SpanA : SpanB, Faces[Top].Patch, Outward, Drop, Strength});
					}
					break;
				}
			}

			// Chain lips end to end. A chain stops at a fork, a sharp corner or a change of top.
			std::unordered_map<uint32_t, std::vector<uint32_t>> AtVertex;
			for (uint32_t I = 0; I < Lips.size(); ++I)
			{
				AtVertex[Lips[I].A].push_back(I);
				AtVertex[Lips[I].B].push_back(I);
			}
			const double CornerCos = std::cos(Radians(Config.MaxCornerDegrees));
			std::vector<bool> Used(Lips.size(), false);
			auto Direction = [&](uint32_t L) { return Normal(Points[Lips[L].B] - Points[Lips[L].A]); };
			// The lip that continues on from Current through its end (bForward) or its start.
			auto Next = [&](uint32_t Current, bool bForward) -> int64_t
			{
				const uint32_t Joint = bForward ? Lips[Current].B : Lips[Current].A;
				const std::vector<uint32_t>& Here = AtVertex[Joint];
				if (Here.size() != 2)
				{
					return -1;
				}
				const uint32_t Onward = Here[0] == Current ? Here[1] : Here[0];
				const bool bRunsOn = bForward ? Lips[Onward].A == Joint : Lips[Onward].B == Joint;
				if (!bRunsOn || Lips[Onward].Patch != Lips[Current].Patch || Lips[Onward].Strength != Lips[Current].Strength || Dot(Direction(Current), Direction(Onward)) < CornerCos)
				{
					return -1;
				}
				return Onward;
			};

			std::vector<Chain> Chains;
			for (uint32_t Start = 0; Start < Lips.size(); ++Start)
			{
				if (Used[Start])
				{
					continue;
				}
				Used[Start] = true;
				std::vector<uint32_t> Run{Start};
				bool bClosed = false;
				for (int64_t L = Next(Start, true); L >= 0; L = Next(uint32_t(L), true))
				{
					if (uint32_t(L) == Start)
					{
						bClosed = true;
						break;
					}
					if (Used[L])
					{
						break;
					}
					Used[L] = true;
					Run.push_back(uint32_t(L));
				}
				if (!bClosed)
				{
					for (int64_t L = Next(Start, false); L >= 0 && !Used[L]; L = Next(uint32_t(L), false))
					{
						Used[L] = true;
						Run.insert(Run.begin(), uint32_t(L));
					}
				}
				Chain C;
				C.Patch = Lips[Start].Patch;
				C.Strength = Lips[Start].Strength;
				C.bClosed = bClosed;
				C.Vertices.push_back(Lips[Run.front()].A);
				C.Outward.push_back(Lips[Run.front()].Outward);
				for (size_t I = 0; I < Run.size(); ++I)
				{
					const Lip& L = Lips[Run[I]];
					C.Vertices.push_back(L.B);
					C.Outward.push_back(I + 1 < Run.size() ? Normal(L.Outward + Lips[Run[I + 1]].Outward) : L.Outward);
					C.Drops.push_back(L.Drop);
				}
				if (C.bClosed)
				{
					const Vec3 SeamOutward = Normal(Lips[Run.back()].Outward + Lips[Run.front()].Outward);
					C.Outward.front() = C.Outward.back() = SeamOutward;
				}
				Chains.push_back(std::move(C));
			}

			struct Candidate
			{
				std::vector<Vec3> Points;
				std::vector<Vec3> Outward; // per point; empty for a rail, which falls away on both sides
				LineKind Kind = LineKind::Stone;
				bool bClosed = false;
				double Drop = 0;
				int32_t Patch = -1;
				Grade Strength = Grade::Firm;
				bool bKeep = true;
			};
			std::vector<Candidate> Candidates;
			for (const Chain& C : Chains)
			{
				Candidate Made;
				Made.bClosed = C.bClosed;
				Made.Drop = Median(C.Drops);
				Made.Patch = C.Patch;
				Made.Strength = C.Strength;
				for (uint32_t V : C.Vertices)
				{
					Made.Points.push_back(Points[V]);
				}
				Made.Outward = C.Outward;
				Candidates.push_back(std::move(Made));
			}

			// On a narrow top, lips found on both sides are one rail: a single line along the crest, midway
			// between them. The sides are matched point by point, so a side broken by a post or a rejected span
			// still pairs wherever the other side runs beside it, and only those stretches become rail. A top
			// that pairs anywhere loses its lips, so one side cannot carry a rail past an obstruction on the
			// other. A narrow top with a lip on one side only, like coping beside a deck, stays a stone line.
			std::unordered_map<int32_t, std::vector<size_t>> NarrowSides;
			for (size_t I = 0; I < Candidates.size(); ++I)
			{
				if (PatchWidth(Candidates[I].Patch) <= Config.RailMaxWidth)
				{
					NarrowSides[Candidates[I].Patch].push_back(I);
				}
			}
			std::vector<Candidate> Rails;
			for (auto& [Patch, Sides] : NarrowSides)
			{
				if (Sides.size() < 2)
				{
					continue;
				}
				const double Width = PatchWidth(Patch);
				const double Reach = std::max(2 * Width, 4.0);
				std::sort(Sides.begin(), Sides.end(), [&](size_t A, size_t B) { return PolylineLength(Candidates[A].Points) > PolylineLength(Candidates[B].Points); });
				std::vector<std::vector<Vec3>> Crests;
				std::vector<bool> CrestClosed;
				auto Flush = [&](std::vector<Vec3>& Run, bool bClosedRun)
				{
					if (Run.size() >= 2)
					{
						Crests.push_back(Run);
						CrestClosed.push_back(bClosedRun);
					}
					Run.clear();
				};
				for (size_t Side : Sides)
				{
					const Candidate& C = Candidates[Side];
					// A side that only confirms the other lays no rail of its own.
					if (C.Strength == Grade::Partner)
					{
						continue;
					}
					const std::vector<Sample> Samples = Resample(C.Points, C.Outward, RailSampleSpacing);
					// The other side is one running the opposite way, across the top from this one rather than
					// ahead of it or behind it.
					std::vector<std::optional<Vec3>> Middle(Samples.size());
					for (size_t K = 0; K < Samples.size(); ++K)
					{
						const Sample& S = Samples[K];
						Nearest Best;
						for (size_t Other : Sides)
						{
							if (Other == Side)
							{
								continue;
							}
							const Nearest N = NearestFromAbove(S.Point, Candidates[Other].Points);
							const Vec3 Across = Flat(N.Point - S.Point);
							if (N.Distance < Best.Distance && Dot(N.Direction, S.Direction) < -0.5 && Dot(Across, S.Outward) < 0
								&& std::abs(Dot(Across, Normal(Flat(S.Direction)))) <= std::max(Width, 2.0))
							{
								Best = N;
							}
						}
						if (Best.Distance > Reach)
						{
							continue;
						}
						const Vec3 Mid = (S.Point + Best.Point) * 0.5;
						bool bCovered = false;
						for (const std::vector<Vec3>& Crest : Crests)
						{
							if (NearestFromAbove(Mid, Crest).Distance <= std::max(Width, 2.0))
							{
								bCovered = true;
								break;
							}
						}
						if (!bCovered)
						{
							Middle[K] = Mid;
						}
					}
					std::vector<Vec3> Run;
					if (C.bClosed && Samples.size() > 2)
					{
						// The last sample repeats the first. Start after a gap so no run is cut at the seam.
						const size_t Count = Samples.size() - 1;
						size_t Start = Count;
						for (size_t K = 0; K < Count && Start == Count; ++K)
						{
							Start = Middle[K] ? Count : K;
						}
						if (Start == Count)
						{
							for (size_t K = 0; K < Count; ++K)
							{
								Run.push_back(*Middle[K]);
							}
							Run.push_back(Run.front());
							Flush(Run, true);
							continue;
						}
						for (size_t Step = 1; Step <= Count; ++Step)
						{
							const std::optional<Vec3>& M = Middle[(Start + Step) % Count];
							if (M)
							{
								Run.push_back(*M);
							}
							else
							{
								Flush(Run, false);
							}
						}
					}
					else
					{
						for (const std::optional<Vec3>& M : Middle)
						{
							if (M)
							{
								Run.push_back(*M);
							}
							else
							{
								Flush(Run, false);
							}
						}
					}
					Flush(Run, false);
				}
				if (Crests.empty())
				{
					continue;
				}
				std::vector<double> SideDrops;
				for (size_t Side : Sides)
				{
					Candidates[Side].bKeep = false;
					SideDrops.push_back(Candidates[Side].Drop);
				}
				for (size_t K = 0; K < Crests.size(); ++K)
				{
					Candidate Rail;
					Rail.Kind = LineKind::Rail;
					Rail.Points = std::move(Crests[K]);
					Rail.bClosed = CrestClosed[K];
					Rail.Drop = Median(SideDrops);
					Rail.Patch = Patch;
					Rails.push_back(std::move(Rail));
				}
			}
			// A rail's crest is read from its own triangles, not from the world: collision is often
			// simpler than the visible tube, and the line belongs on the tube.
			if (!Rails.empty())
			{
				const TriangleField Own(std::vector<Mesh>{In});
				for (Candidate& C : Rails)
				{
					for (Vec3& P : C.Points)
					{
						const std::optional<double> Crest = Own.Below({P.X, P.Y, P.Z + Config.RailMaxWidth}, 2 * Config.RailMaxWidth);
						P.Z = Crest ? *Crest : P.Z;
					}
					if (C.bClosed)
					{
						C.Points.back() = C.Points.front();
					}
				}
			}
			std::vector<Candidate> Pieces;
			for (Candidate& C : Candidates)
			{
				// A narrow top's side that fell too little for a ledge and found no other side stays unlined.
				if (C.bKeep && C.Strength != Grade::Firm)
				{
					C.bKeep = false;
					if (Rejections)
					{
						Rejections->push_back({C.Points.front(), C.Points.back(), "the fall past the edge is too small", C.Drop, MeshIndex});
					}
				}
				if (C.bKeep)
				{
					Pieces.push_back(std::move(C));
				}
			}
			for (Candidate& C : Rails)
			{
				Pieces.push_back(std::move(C));
			}

			// Join pieces of one line across short gaps: the seam between two blocks of a wall, and a post or
			// bracket that breaks a rail's sides. A gap is crossed only where nothing stands over the line or
			// beside it, and a ledge must still fall away past it.
			auto GapClear = [&](const Vec3& From, const Vec3& To, const Candidate& Piece) -> bool
			{
				const Vec3 Along = Normal(Flat(To - From));
				std::vector<Vec3> Asides;
				if (Piece.Kind == LineKind::Rail)
				{
					Asides = {Vec3{-Along.Y, Along.X, 0}, Vec3{Along.Y, -Along.X, 0}};
				}
				else
				{
					Asides = {Piece.Outward.back()};
				}
				const double HalfWidth = Piece.Kind == LineKind::Rail ? PatchWidth(Piece.Patch) / 2 : 0;
				const int Count = std::max(1, int(std::ceil(Size(To - From) / Config.SampleSpacing * 2)));
				for (int K = 0; K < Count; ++K)
				{
					const Vec3 P = Lerp(From, To, (K + 0.5) / Count);
					if (Below({P.X, P.Y, P.Z + Config.Clearance}, Config.Clearance - 2 * CoverInset))
					{
						return false;
					}
					if (FirstHit && FirstHit(P + Vec3{0, 0, 1}, P + Vec3{0, 0, Config.Clearance}))
					{
						return false;
					}
					for (const Vec3& Aside : Asides)
					{
						const Vec3 Probe = P + Aside * (HalfWidth + Config.ProbeDistance);
						const std::optional<double> Hit = Below({Probe.X, Probe.Y, P.Z + Config.Clearance}, Config.Clearance + Config.MaxDropSearch);
						// Beside a rail nothing may stand above its crest; past a ledge the surface must fall.
						if (Piece.Kind == LineKind::Rail ? Hit && *Hit > P.Z + CoverInset : !Hit || P.Z - *Hit < Config.MinDrop)
						{
							return false;
						}
						const Vec3 Lifted = P + Vec3{0, 0, BesideLift};
						if (FirstHit && FirstHit(Lifted + Aside * HalfWidth, Lifted + Aside * (HalfWidth + Config.ProbeDistance)))
						{
							return false;
						}
					}
				}
				return true;
			};
			const double JoinCornerCos = std::cos(Radians(Config.MaxCornerDegrees));
			// Joins Second onto the end of First when Second starts where First ends and runs on the same way.
			auto TryJoin = [&](Candidate& First, Candidate& Second) -> bool
			{
				const Vec3 End = First.Points.back();
				const Vec3 Start = Second.Points.front();
				const Vec3 Gap = Start - End;
				const double FlatGap = FlatSize(Gap);
				if (FlatGap > (First.Kind == LineKind::Rail ? Config.RailJoinGap : Config.JoinGap))
				{
					return false;
				}
				const Vec3 EndDirection = Normal(Flat(First.Points.back() - First.Points[First.Points.size() - 2]));
				const Vec3 StartDirection = Normal(Flat(Second.Points[1] - Second.Points[0]));
				if (Dot(EndDirection, StartDirection) < JoinCornerCos || std::abs(Gap.Z) > std::max(2.0, FlatGap * MaxLineSlope))
				{
					return false;
				}
				// Beyond a touch, the gap must lie straight ahead: not a parallel line alongside.
				if (FlatGap > 1)
				{
					const double Sideways = std::abs(Gap.X * EndDirection.Y - Gap.Y * EndDirection.X);
					if (Dot(Normal(Flat(Gap)), EndDirection) < JoinCornerCos || Sideways > std::max(1.5, 0.2 * FlatGap) || !GapClear(End, Start, First))
					{
						return false;
					}
				}
				const size_t Skip = FlatGap <= 1 ? 1 : 0;
				First.Points.insert(First.Points.end(), Second.Points.begin() + Skip, Second.Points.end());
				if (!First.Outward.empty() && !Second.Outward.empty())
				{
					First.Outward.insert(First.Outward.end(), Second.Outward.begin() + Skip, Second.Outward.end());
				}
				First.Drop = std::min(First.Drop, Second.Drop);
				Second.bKeep = false;
				return true;
			};
			auto Reverse = [](Candidate& C)
			{
				std::reverse(C.Points.begin(), C.Points.end());
				std::reverse(C.Outward.begin(), C.Outward.end());
			};
			for (bool bJoined = true; bJoined;)
			{
				bJoined = false;
				for (size_t I = 0; I < Pieces.size(); ++I)
				{
					for (size_t J = 0; J < Pieces.size() && Pieces[I].bKeep && !Pieces[I].bClosed; ++J)
					{
						Candidate& First = Pieces[I];
						Candidate& Second = Pieces[J];
						if (I == J || !Second.bKeep || Second.bClosed || Second.Kind != First.Kind || First.Points.size() < 2 || Second.Points.size() < 2)
						{
							continue;
						}
						if (TryJoin(First, Second))
						{
							bJoined = true;
						}
						else if (First.Kind == LineKind::Rail)
						{
							// A rail has no side to keep to: its pieces may meet either way round.
							Reverse(Second);
							if (TryJoin(First, Second))
							{
								bJoined = true;
							}
							else
							{
								Reverse(Second);
							}
						}
					}
				}
			}
			// A piece whose ends meet across a clear gap closes, as a ring rail joined all the way round.
			for (Candidate& C : Pieces)
			{
				const double MaxGap = C.Kind == LineKind::Rail ? Config.RailJoinGap : Config.JoinGap;
				if (!C.bKeep || C.bClosed || C.Points.size() < 4 || PolylineLength(C.Points) < 4 * MaxGap)
				{
					continue;
				}
				const Vec3 Gap = C.Points.front() - C.Points.back();
				const double FlatGap = FlatSize(Gap);
				const Vec3 EndDirection = Normal(Flat(C.Points.back() - C.Points[C.Points.size() - 2]));
				const Vec3 StartDirection = Normal(Flat(C.Points[1] - C.Points[0]));
				if (FlatGap > MaxGap || std::abs(Gap.Z) > std::max(2.0, FlatGap * MaxLineSlope) || Dot(EndDirection, StartDirection) < JoinCornerCos)
				{
					continue;
				}
				if (FlatGap > 1 && (Dot(Normal(Flat(Gap)), EndDirection) < JoinCornerCos || !GapClear(C.Points.back(), C.Points.front(), C)))
				{
					continue;
				}
				if (FlatGap <= 1)
				{
					C.Points.back() = C.Points.front();
				}
				else
				{
					C.Points.push_back(C.Points.front());
					if (!C.Outward.empty())
					{
						C.Outward.push_back(C.Outward.front());
					}
				}
				C.bClosed = true;
			}

			for (const Candidate& C : Pieces)
			{
				if (!C.bKeep)
				{
					continue;
				}
				Line L;
				L.Kind = C.Kind;
				L.Points = Simplified(C.Points, Config.SimplifyTolerance);
				// A closed path needs at least three distinct points plus its repeated endpoint.
				if (C.bClosed && L.Points.size() < 4) L.Points = C.Points;
				L.bClosed = C.bClosed;
				L.MeshIndex = MeshIndex;
				L.Length = PolylineLength(L.Points);
				L.Drop = C.Drop;
				L.TopWidth = PatchWidth(C.Patch);
				if (L.Length >= Config.MinLength)
				{
					Out.push_back(std::move(L));
				}
				else if (Rejections)
				{
					Rejections->push_back({L.Points.front(), L.Points.back(), "shorter than the minimum length", L.Drop, MeshIndex});
				}
			}
		}
	}

	std::vector<Line> FindGrindLines(const std::vector<Mesh>& Meshes, const SurfaceBelow& Below, const SegmentTest& FirstHit, const Settings& Config, std::vector<Rejection>* Rejections)
	{
		std::vector<Line> Out;
		for (size_t I = 0; I < Meshes.size(); ++I)
		{
			FindInMesh(Meshes[I], I, Below, FirstHit, Config, Out, Rejections);
		}
		return Out;
	}

	std::vector<Line> FindGrindLines(const std::vector<Mesh>& Meshes, const SurfaceBelow& Below, const Settings& Config, std::vector<Rejection>* Rejections)
	{
		return FindGrindLines(Meshes, Below, SegmentTest(), Config, Rejections);
	}

	TriangleField::TriangleField(const std::vector<Mesh>& Meshes, double InCellSize)
		: CellSize(InCellSize)
	{
		for (const Mesh& M : Meshes)
		{
			for (size_t I = 0; I + 2 < M.Indices.size(); I += 3)
			{
				const Triangle T{M.Vertices[M.Indices[I]], M.Vertices[M.Indices[I + 1]], M.Vertices[M.Indices[I + 2]]};
				const uint32_t Index = uint32_t(Triangles.size());
				Triangles.push_back(T);
				const int64_t X0 = int64_t(std::floor(std::min({T.A.X, T.B.X, T.C.X}) / CellSize));
				const int64_t X1 = int64_t(std::floor(std::max({T.A.X, T.B.X, T.C.X}) / CellSize));
				const int64_t Y0 = int64_t(std::floor(std::min({T.A.Y, T.B.Y, T.C.Y}) / CellSize));
				const int64_t Y1 = int64_t(std::floor(std::max({T.A.Y, T.B.Y, T.C.Y}) / CellSize));
				for (int64_t X = X0; X <= X1; ++X)
				{
					for (int64_t Y = Y0; Y <= Y1; ++Y)
					{
						Cells[CellKey((X + 0.5) * CellSize, (Y + 0.5) * CellSize)].push_back(Index);
					}
				}
			}
		}
	}

	uint64_t TriangleField::CellKey(double X, double Y) const
	{
		const int64_t CX = int64_t(std::floor(X / CellSize));
		const int64_t CY = int64_t(std::floor(Y / CellSize));
		return (uint64_t(uint32_t(int32_t(CX))) << 32) | uint32_t(int32_t(CY));
	}

	std::optional<double> TriangleField::Below(const Vec3& From, double MaxDistance) const
	{
		const auto Cell = Cells.find(CellKey(From.X, From.Y));
		if (Cell == Cells.end())
		{
			return std::nullopt;
		}
		std::optional<double> Best;
		for (uint32_t Index : Cell->second)
		{
			const Triangle& T = Triangles[Index];
			// Barycentric coordinates of From in the triangle's plan view.
			const double D = (T.B.Y - T.C.Y) * (T.A.X - T.C.X) + (T.C.X - T.B.X) * (T.A.Y - T.C.Y);
			if (std::abs(D) < 1e-9)
			{
				continue;
			}
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
		return Best;
	}

	std::optional<SegmentHit> TriangleField::FirstHit(const Vec3& From, const Vec3& To) const
	{
		std::optional<SegmentHit> Best;
		const int64_t X0 = int64_t(std::floor(std::min(From.X, To.X) / CellSize));
		const int64_t X1 = int64_t(std::floor(std::max(From.X, To.X) / CellSize));
		const int64_t Y0 = int64_t(std::floor(std::min(From.Y, To.Y) / CellSize));
		const int64_t Y1 = int64_t(std::floor(std::max(From.Y, To.Y) / CellSize));
		for (int64_t X = X0; X <= X1; ++X)
		{
			for (int64_t Y = Y0; Y <= Y1; ++Y)
			{
				const auto Cell = Cells.find(CellKey((X + 0.5) * CellSize, (Y + 0.5) * CellSize));
				if (Cell == Cells.end())
				{
					continue;
				}
				for (uint32_t Index : Cell->second)
				{
					const Triangle& T = Triangles[Index];
					const std::optional<double> Along = SegmentHitsTriangle(From, To, T.A, T.B, T.C);
					if (Along && (!Best || *Along < Best->T))
					{
						Best = SegmentHit{*Along, Cross(T.B - T.A, T.C - T.A)};
					}
				}
			}
		}
		return Best;
	}
}
