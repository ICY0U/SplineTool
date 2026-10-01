// The detector: from triangles to grind lines.
//
// Each mesh is read on its own: its vertices are welded, stray triangles wound the wrong way are turned
// round, and faces flat enough to stand on are joined into tops. The outer edges of each top are tested
// against the world (does the surface fall away past the edge, is anything standing over or beside it)
// and against the mesh's own shape (does the surface turn down sharply there: convexity). Edges that pass
// are chained into lines; the two sides of a narrow or round top become one line along its crest. Then
// the pieces of every mesh are joined where they meet, so modular pieces placed side by side give one
// line, and each line is given a confidence that decides whether it is suggested for keeping.
#include "AutoGrindCore.h"
#include "AutoGrindMath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>

namespace AutoGrindCore
{
	using namespace Detail;

	namespace
	{
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
		// Faces within this of a top's mean normal are its flat part; a top with less of its area flat is round.
		constexpr double FlatNormalDegrees = 10;
		constexpr double RoundFlatShare = 0.6;
		// Convexity: how far past an edge the surface's turn is followed, and how far down it is looked for.
		constexpr double BendBand = 6;
		constexpr double BendSearch = 30;
		// A bend this little above Min Edge Angle marks a line as only just sharp enough.
		constexpr double SoftEdgeMargin = 10;
		// A fall up to this can be a stair step, whose next step lies within StairReach past or behind it and
		// moves by at least StairShare of the step and at most StairSpan times it.
		constexpr double MaxStairRise = 32;
		constexpr double StairReach = 50;
		constexpr double StairProbeStep = 5;
		constexpr double StairShare = 0.4;
		constexpr double StairSpan = 2.5;
		constexpr double TreadFlatness = 3;
		// Where a crest's body depth is sampled, and how much deeper than Rail Max Thickness it is looked for.
		constexpr double ThicknessSpacing = 20;
		constexpr double ThicknessReach = 60;
		// Round tops narrower than this share of Rail Max Width are tubes: rails even on a deep body (coping).
		constexpr double TubeShare = 0.5;
		// A ridge's faces must face this far up and slope at least this much.
		constexpr double RidgeMaxFaceTiltDegrees = 80;
		constexpr double RidgeMinFaceSlopeDegrees = 20;
		// Pieces this close alongside one another are the same line.
		constexpr double OverlapTolerance = 2.5;
		constexpr double RailOverlapTolerance = 3;
		// Crests this much shorter than their longest side only partly paired.
		constexpr double PartialRailShare = 0.8;
		// A lip climbing at least this much and this steeply, its fall growing by this share of the climb, is
		// the side of a ramp.
		constexpr double RampMinClimb = 10;
		constexpr double RampMinSlopeDegrees = 8;
		constexpr double RampDropShare = 0.6;
		constexpr double LengthSlack = 0.01;

		// What a lip can become. A ledge line needs a firm lip; a low lip makes a low-ledge line. The side of
		// a narrow top that falls less than a ledge must can still be a side of a rail, though never a line of
		// its own; a side that falls less than RailMinDrop, or onto nothing, only confirms the other side. A
		// soft lip rounds over too smoothly to be an edge, but two soft sides of a round top meet in a crest.
		enum class Grade : uint8_t
		{
			Partner,
			Rail,
			Soft,
			Low,
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
			double Bend = 90;
			Grade Strength = Grade::Firm;
			bool bSteep = false;
		};

		struct Chain
		{
			std::vector<uint32_t> Vertices;
			std::vector<Vec3> Outward; // per vertex
			std::vector<double> Drops;
			std::vector<double> Bends;
			int32_t Patch = -1;
			Grade Strength = Grade::Firm;
			bool bClosed = false;
			bool bSteep = false;
		};

		// A side of a top: a chain as points, before the sides of narrow tops pair into crests.
		struct Side
		{
			std::vector<Vec3> Points;
			std::vector<Vec3> Outward;
			bool bClosed = false;
			double Drop = 0;
			double Bend = 90;
			int32_t Patch = -1;
			Grade Strength = Grade::Firm;
			bool bSteep = false;
			// The side climbs while the ground beside it stays level: the side of a ramp, bank or kicker.
			bool bRampSide = false;
			bool bKeep = true;
		};

		// A line in the making, from one mesh or joined across several.
		struct Piece
		{
			LineKind Kind = LineKind::Stone;
			LineShape Shape = LineShape::Lip;
			std::vector<Vec3> Points;
			// Per point for a lip, which keeps its top on the left; empty for a crest or ridge, which can be
			// walked either way.
			std::vector<Vec3> Outward;
			bool bClosed = false;
			double Drop = 0;
			double TopWidth = 0;
			double Thickness = 0;
			double Bend = 90;
			uint32_t NoteBits = 0;
			// Length that falls only as far as a low ledge does.
			double LowLength = 0;
			std::vector<size_t> Meshes;
			bool bKeep = true;

			bool IsOriented() const { return !Outward.empty(); }
			double HalfWidth() const { return Shape == LineShape::Lip ? 0 : TopWidth / 2; }
		};

		void Reverse(Piece& P)
		{
			std::reverse(P.Points.begin(), P.Points.end());
			std::reverse(P.Outward.begin(), P.Outward.end());
		}

		// A lip's outward direction at a distance along it.
		Vec3 OutwardAt(const Piece& P, double Along)
		{
			double Walked = 0;
			for (size_t K = 0; K + 1 < P.Points.size(); ++K)
			{
				const double Length = Size(P.Points[K + 1] - P.Points[K]);
				if (Walked + Length >= Along)
				{
					return P.Outward[K];
				}
				Walked += Length;
			}
			return P.Outward.empty() ? Vec3{} : P.Outward.back();
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
				const Vec3 Direction = Unit(Points[I + 1] - Points[I]);
				const int Count = std::max(1, int(std::ceil(Size(Points[I + 1] - Points[I]) / Step)));
				for (int K = 0; K < Count; ++K)
				{
					const double T = double(K) / Count;
					Out.push_back({Lerp(Points[I], Points[I + 1], T), Unit(Lerp(Outward[I], Outward[I + 1], T)), Direction});
				}
			}
			if (!Points.empty())
			{
				Out.push_back({Points.back(), Outward.back(), Out.empty() ? Vec3{} : Out.back().Direction});
			}
			return Out;
		}

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
					Best = {D, On, Unit(Points[I + 1] - Points[I])};
				}
			}
			return Best;
		}

		Vec3 PointAlongPolyline(const std::vector<Vec3>& Points, double Along)
		{
			double Walked = 0;
			for (size_t I = 0; I + 1 < Points.size(); ++I)
			{
				const double Length = Size(Points[I + 1] - Points[I]);
				if (Walked + Length >= Along || I + 2 == Points.size())
				{
					return Lerp(Points[I], Points[I + 1], Length > 0 ? std::clamp((Along - Walked) / Length, 0.0, 1.0) : 0.0);
				}
				Walked += Length;
			}
			return Points.empty() ? Vec3{} : Points.front();
		}

		// ------------------------------------------------------------------------------------------------
		// One mesh.
		// ------------------------------------------------------------------------------------------------

		class MeshScan
		{
		public:
			MeshScan(const Mesh& InSource, size_t InMeshIndex, const SurfaceBelow& InBelow, const SegmentTest& InFirstHit, const Settings& InConfig, std::vector<Rejection>* InRejections)
				: Source(InSource), MeshIndex(InMeshIndex), Below(InBelow), FirstHit(InFirstHit), Config(InConfig), Rejections(InRejections)
			{
			}

			void Run(std::vector<Piece>& Out)
			{
				Build();
				if (Faces.empty())
				{
					return;
				}
				FindPatches();
				Own.emplace(std::vector<Mesh>{Source});
				std::vector<Lip> Lips;
				FindLips(Lips);
				std::vector<Side> Sides = MakeSides(ChainLips(Lips));
				PairCrests(Sides, Out);
				EmitLips(Sides, Out);
				if (Config.bDetectRidges)
				{
					FindRidges(Out);
				}
			}

		private:
			const Mesh& Source;
			size_t MeshIndex;
			const SurfaceBelow& Below;
			const SegmentTest& FirstHit;
			const Settings& Config;
			std::vector<Rejection>* Rejections;

			std::vector<Vec3> Points;
			std::vector<Face> Faces;
			EdgeMap EdgeFaces;
			std::vector<double> PatchArea;
			std::vector<double> PatchPerimeter;
			std::vector<double> PatchOpenPerimeter;
			std::vector<bool> PatchRound;
			std::optional<TriangleField> Own;
			double TopCos = 0;

			void RejectEdge(const Vec3& A, const Vec3& B, Reject Reason, double Drop)
			{
				if (Rejections)
				{
					Rejections->push_back({A, B, Reason, Drop, MeshIndex});
				}
			}

			bool IsTop(uint32_t F) const { return Faces[F].Normal.Z >= TopCos; }

			double PatchWidth(int32_t Patch) const
			{
				return Patch >= 0 && PatchPerimeter[size_t(Patch)] > 0 ? 2 * PatchArea[size_t(Patch)] / PatchPerimeter[size_t(Patch)] : 0.0;
			}

			// A top narrow enough for its sides to meet in one line along its crest.
			bool IsNarrowish(int32_t Patch) const
			{
				if (Patch < 0)
				{
					return false;
				}
				const double Width = PatchWidth(Patch);
				return Width <= Config.RailMaxWidth || (PatchRound[size_t(Patch)] && Width <= Config.RoundTopMaxWidth);
			}

			// Welds the vertices, drops degenerate and doubled triangles, and turns round triangles wound
			// against their neighbours.
			void Build()
			{
				std::vector<uint32_t> Remap;
				Points = Weld(Source.Vertices, Config.WeldTolerance, Remap);
				std::vector<uint32_t> Corners;
				Corners.reserve(Source.Indices.size());
				for (size_t I = 0; I + 2 < Source.Indices.size(); I += 3)
				{
					if (Source.Indices[I] < Remap.size() && Source.Indices[I + 1] < Remap.size() && Source.Indices[I + 2] < Remap.size())
					{
						Corners.insert(Corners.end(), {Remap[Source.Indices[I]], Remap[Source.Indices[I + 1]], Remap[Source.Indices[I + 2]]});
					}
				}
				BuildFaces(Points, Corners, Config.bRepairWinding, Faces, EdgeFaces);
			}

			// Tops, joined into patches wherever two tops share an edge. Half a degree of slack keeps a face
			// modelled at exactly the limit on one side of it: a 45 degree chamfer, or the matching facets
			// either side of a tube, whose rounding would otherwise split them between top and not.
			void FindPatches()
			{
				TopCos = std::cos(Radians(Config.MaxTopSlopeDegrees + TopSlopeSlackDegrees));
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
				const size_t Count = PatchIds.size();
				PatchArea.assign(Count, 0);
				PatchPerimeter.assign(Count, 0);
				PatchOpenPerimeter.assign(Count, 0);
				std::vector<Vec3> Mean(Count);
				for (const Face& F : Faces)
				{
					if (F.Patch >= 0)
					{
						PatchArea[size_t(F.Patch)] += F.Area;
						Mean[size_t(F.Patch)] = Mean[size_t(F.Patch)] + F.Normal * F.Area;
					}
				}
				// A patch's width, 2 x area / perimeter, tells a rail's narrow top from a ledge's broad one.
				for (const auto& [Key, Shared] : EdgeFaces)
				{
					const double Length = Size(Points[uint32_t(Key & 0xffffffff)] - Points[uint32_t(Key >> 32)]);
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
							PatchPerimeter[size_t(Faces[Top].Patch)] += Length;
							PatchOpenPerimeter[size_t(Faces[Top].Patch)] += Shared.size() == 1 ? Length : 0;
						}
					}
				}
				// A round top has most of its area turned away from its mean normal: a tube, a pipe, a bullnose.
				std::vector<double> FlatArea(Count, 0);
				const double FlatCos = std::cos(Radians(FlatNormalDegrees));
				for (Vec3& M : Mean)
				{
					M = Unit(M);
				}
				for (const Face& F : Faces)
				{
					if (F.Patch >= 0 && Dot(F.Normal, Mean[size_t(F.Patch)]) >= FlatCos)
					{
						FlatArea[size_t(F.Patch)] += F.Area;
					}
				}
				PatchRound.assign(Count, false);
				for (size_t P = 0; P < Count; ++P)
				{
					PatchRound[P] = PatchArea[P] > 0 && FlatArea[P] < RoundFlatShare * PatchArea[P];
				}
			}

			// Convexity: how sharply the surface turns down over an edge, in degrees. The top's slope a few
			// centimetres inside the edge is compared with the steepest fall of the mesh's own surface over the
			// next few centimetres outside it; where the surface ends, the fall is vertical. A box edge turns
			// 90 degrees, a bevelled or bullnosed one nearly as much, the shoulder of a dome or mound hardly at all.
			double EdgeBend(const Vec3& OnEdge, const Vec3& Outward, double Width) const
			{
				if (!Own)
				{
					return 90;
				}
				const double Inner = std::clamp(Width / 2, 1.0, 4.0);
				const Vec3 In = OnEdge - Outward * Inner;
				const double Rise = Inner * std::tan(Radians(std::min(89.0, Config.MaxTopSlopeDegrees + TopSlopeSlackDegrees))) + 1;
				const std::optional<double> InnerZ = Own->Below({In.X, In.Y, OnEdge.Z + Rise}, 2 * Rise);
				const double SlopeIn = InnerZ ? Degrees(std::atan2(*InnerZ - OnEdge.Z, Inner)) : 0;
				double SlopeOut = 0;
				double PreviousOut = 0;
				double PreviousZ = OnEdge.Z;
				for (double Out : {0.5, 1.0, 2.0, 3.0, 4.5, BendBand})
				{
					const Vec3 P = OnEdge + Outward * Out;
					const std::optional<double> Z = Own->Below({P.X, P.Y, OnEdge.Z + 0.05}, BendSearch);
					if (!Z)
					{
						SlopeOut = 90;
						break;
					}
					SlopeOut = std::max(SlopeOut, Degrees(std::atan2(PreviousZ - *Z, Out - PreviousOut)));
					PreviousOut = Out;
					PreviousZ = *Z;
				}
				return SlopeOut - SlopeIn;
			}

			// A stair nosing falls one step onto a narrow tread that falls again just past it, or has the next
			// step rising just behind it. A curb or manual pad falls onto ground that carries on level.
			bool IsStairNosing(const Vec3& OnEdge, const Vec3& Outward, double Drop) const
			{
				const double Landing = OnEdge.Z - Drop;
				const double MinStep = StairShare * Drop;
				const double MaxStep = StairSpan * Drop;
				for (double Out = Config.ProbeDistance + StairProbeStep; Out <= StairReach; Out += StairProbeStep)
				{
					const Vec3 P = OnEdge + Outward * Out;
					const std::optional<double> Z = Below({P.X, P.Y, OnEdge.Z + 1}, Drop + MaxStep + 2);
					if (!Z)
					{
						break;
					}
					if (std::abs(*Z - Landing) <= TreadFlatness)
					{
						continue;
					}
					if (*Z < Landing - MinStep && *Z >= Landing - MaxStep)
					{
						return true;
					}
					break;
				}
				for (double Back = StairProbeStep; Back <= StairReach; Back += StairProbeStep)
				{
					const Vec3 P = OnEdge - Outward * Back;
					const std::optional<double> Z = Below({P.X, P.Y, OnEdge.Z + MaxStep + 1}, MaxStep + TreadFlatness + 2);
					if (!Z)
					{
						break;
					}
					if (std::abs(*Z - OnEdge.Z) <= TreadFlatness)
					{
						continue;
					}
					if (*Z > OnEdge.Z + MinStep && *Z <= OnEdge.Z + MaxStep + 1)
					{
						return true;
					}
					break;
				}
				return false;
			}

			void FindLips(std::vector<Lip>& Lips)
			{
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
				const double SteepFall = std::tan(Radians(Config.SteepLipDegrees));
				constexpr double NoFloor = -std::numeric_limits<double>::max();
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
					const double Horizontal = FlatSize(Along);
					if (Horizontal <= 0 || std::abs(Along.Z) > MaxLineSlope * Horizontal)
					{
						RejectEdge(Points[A], Points[B], Reject::TooSteep, 0);
						continue;
					}
					uint32_t Top = Shared.front();
					for (uint32_t F : Shared)
					{
						if (IsTop(F))
						{
							Top = F;
							break;
						}
					}
					// Every other face on the edge must fall away below the top's plane.
					bool bConvex = true;
					for (uint32_t Other : Shared)
					{
						if (Other != Top && Dot(Points[ThirdVertex(Faces[Other], A, B)] - Points[A], Faces[Top].Normal) > -0.01)
						{
							bConvex = false;
							break;
						}
					}
					if (!bConvex)
					{
						RejectEdge(Points[A], Points[B], Reject::InsideCorner, 0);
						continue;
					}
					// Outward is level and square to the edge seen from above, on the side away from the top. The
					// side comes from the top's winding: seen from above a top runs anticlockwise, so it lies left
					// of each of its edges in order. The third vertex cannot tell on a long, thin facet: along a
					// sloped rail or a curved tube its far end sits past the edge's line.
					const bool bTopOnLeft = Runs(Faces[Top], A, B);
					const Vec3 LeftOfEdge = Unit(Vec3{-Along.Y, Along.X, 0});
					const Vec3 Outward = bTopOnLeft ? LeftOfEdge * -1 : LeftOfEdge;
					const int32_t Patch = Faces[Top].Patch;
					const Vec3 Up = Faces[Top].Normal;
					const bool bNarrow = IsNarrowish(Patch);
					const double CarryStep = bNarrow ? std::min(SeamStep, LevelStep) : SeamStep;
					const Vec3 EdgeStart = Points[A];
					const Vec3 EdgeEnd = Points[B];
					const double Bend = EdgeBend(Lerp(EdgeStart, EdgeEnd, 0.5), Outward, PatchWidth(Patch));
					// A tube's lip lies on its shoulder, below its crest: whether the surface past it carries the top
					// on is judged from the crest, so pipe coping a few centimetres proud of a deck is not a seam.
					double CrestRise = 0;
					if (bNarrow && PatchRound[size_t(Patch)])
					{
						const Vec3 Middle = Lerp(EdgeStart, EdgeEnd, 0.5) - Outward * (PatchWidth(Patch) / 2);
						const std::optional<double> CrestZ = Own->Below({Middle.X, Middle.Y, Middle.Z + PatchWidth(Patch) + 1}, 2 * PatchWidth(Patch) + 2);
						CrestRise = CrestZ ? std::max(0.0, *CrestZ - Middle.Z) : 0;
					}
					// Long edges are tested in spans, so an obstruction splits the line instead of hiding.
					const int SpanCount = std::max(1, int(std::ceil(Size(Along) / std::max(1.0, Config.SampleSpacing))));
					uint32_t Previous = A;
					for (int Span = 0; Span < SpanCount; ++Span)
					{
						const uint32_t SpanEnd = Span + 1 == SpanCount ? B : uint32_t(Points.size());
						if (Span + 1 != SpanCount)
						{
							Points.push_back(Lerp(EdgeStart, EdgeEnd, double(Span + 1) / SpanCount));
						}
						const uint32_t SpanA = Previous;
						const uint32_t SpanB = SpanEnd;
						Previous = SpanEnd;
						const Vec3 SpanStart = Points[SpanA];
						const Vec3 SpanFinish = Points[SpanB];
						// Just inside the edge the top must be the first surface from above: a top with another
						// part stacked on it is hidden. Just past the edge the surface must fall at least MinDrop,
						// with nothing standing above it, or fall steeply onto a steep slope (a bank's top edge).
						std::array<double, 3> Covers{};
						std::array<double, 3> Drops{};
						std::array<double, 3> LandingFalls{};
						bool bWalled = false;
						bool bBuried = false;
						bool bCarriesOn = false;
						size_t SampleIndex = 0;
						for (double T : {0.25, 0.5, 0.75})
						{
							const Vec3 OnEdge = Lerp(SpanStart, SpanFinish, T);
							const Vec3 Inside = OnEdge - Outward * CoverInset;
							const double TopHeight = OnEdge.Z - (Up.X * (Inside.X - OnEdge.X) + Up.Y * (Inside.Y - OnEdge.Y)) / Up.Z;
							const std::optional<double> Surface = Below({Inside.X, Inside.Y, OnEdge.Z + Config.Clearance}, Config.Clearance + 2 * CoverInset);
							Covers[SampleIndex] = Surface ? *Surface - TopHeight : 0;
							const Vec3 Probe = OnEdge + Outward * Config.ProbeDistance;
							const std::optional<double> Hit = Below({Probe.X, Probe.Y, OnEdge.Z + Config.Clearance}, Config.Clearance + Config.MaxDropSearch);
							Drops[SampleIndex] = Hit ? OnEdge.Z - *Hit : NoFloor;
							const Vec3 Further = Probe + Outward * LandingStep;
							const std::optional<double> FurtherHit = Below({Further.X, Further.Y, OnEdge.Z + Config.Clearance}, Config.Clearance + Config.MaxDropSearch);
							LandingFalls[SampleIndex] = Hit && FurtherHit ? *Hit - *FurtherHit : 0;
							++SampleIndex;
							// The space a skater grinds through: straight up from the edge, and across and along it just
							// above the top. The drop test cannot see a wall here when its traces start inside that wall.
							// Straight up, the first surface met from behind is the top of a part the edge is buried in.
							if (FirstHit && !bWalled && !bBuried)
							{
								const Vec3 Lifted = OnEdge + Vec3{0, 0, BesideLift};
								const Vec3 AlongEdge = Unit(Along);
								const std::optional<SegmentHit> Over = FirstHit(OnEdge + Vec3{0, 0, 1}, OnEdge + Vec3{0, 0, BuriedReach});
								if (Over && Over->Facing.Z > 0)
								{
									const std::optional<SegmentHit> Beyond = FirstHit(Lifted, Lifted + Outward * BuriedSideReach);
									bBuried = Beyond && Dot(Beyond->Facing, Outward) > 0;
								}
								// Across the edge the space starts over the top behind it, lifted with the top where a
								// round top rises, so a pipe's own crest is not taken for a wall.
								Vec3 Behind = Lifted - Outward * SkateReach;
								const std::optional<double> OwnTop = Own->Below({Behind.X, Behind.Y, OnEdge.Z + SkateReach * 2 + BesideLift}, SkateReach * 4 + BesideLift);
								if (OwnTop)
								{
									Behind.Z = std::max(Behind.Z, *OwnTop + BesideLift);
								}
								bWalled = (Over && Over->T * (BuriedReach - 1) < Config.Clearance - 1)
									|| FirstHit(Behind, Lifted + Outward * Config.ProbeDistance)
									|| FirstHit(Lifted - AlongEdge * SkateReach, Lifted + AlongEdge * SkateReach);
							}
							// A flat surface just past the edge, level with it or a small step below, carries the top on
							// across a seam, a step or the gap between slats. The first distance clears a rounded or
							// bevelled edge's own fall, and flatness tells that fall from a surface that carries on.
							for (double D : GapDistances)
							{
								const Vec3 NearPoint = OnEdge + Outward * (D - FlatStep / 2);
								const std::optional<double> NearHit = Below({NearPoint.X, NearPoint.Y, OnEdge.Z + Config.Clearance}, Config.Clearance + CarryStep);
								if (!NearHit || OnEdge.Z + CrestRise - *NearHit >= CarryStep)
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
							RejectEdge(SpanStart, SpanFinish, Reject::Covered, 0);
							continue;
						}
						if (bBuried)
						{
							RejectEdge(SpanStart, SpanFinish, Reject::Buried, 0);
							continue;
						}
						if (bWalled)
						{
							RejectEdge(SpanStart, SpanFinish, Reject::Walled, 0);
							continue;
						}
						if (bCarriesOn)
						{
							RejectEdge(SpanStart, SpanFinish, Reject::Seam, 0);
							continue;
						}
						const double Drop = *std::min_element(Drops.begin(), Drops.end());
						const bool bNoSurface = Drop == NoFloor;
						const bool bSteepLip = Config.SteepLipDegrees > 0 && !bNoSurface && Drop >= Config.ProbeDistance * SteepFall
							&& *std::min_element(LandingFalls.begin(), LandingFalls.end()) >= LandingStep * SteepFall;
						if (Config.MaxDrop > 0 && !bNoSurface && Drop > Config.MaxDrop)
						{
							RejectEdge(SpanStart, SpanFinish, Reject::TooHigh, Drop);
							continue;
						}
						Grade Strength = Grade::Firm;
						if (Drop < Config.MinDrop && !bSteepLip)
						{
							if (bNarrow)
							{
								if (Drop < 0 && !bNoSurface)
								{
									RejectEdge(SpanStart, SpanFinish, Reject::Obstructed, Drop);
									continue;
								}
								Strength = bNoSurface || Drop >= Config.RailMinDrop ? Grade::Rail : Grade::Partner;
							}
							else if (Config.LowLedges != LowLedgeMode::Off && !bNoSurface && Drop >= Config.LowLedgeMinDrop)
							{
								Strength = Grade::Low;
							}
							else
							{
								RejectEdge(SpanStart, SpanFinish, bNoSurface ? Reject::NoSurface : Drop < 0 ? Reject::Obstructed : Reject::SmallFall, bNoSurface ? 0 : Drop);
								continue;
							}
						}
						if (Config.bRejectStairs && (Strength == Grade::Firm || Strength == Grade::Low) && !bSteepLip && !bNoSurface && Drop > 0 && Drop <= MaxStairRise
							&& IsStairNosing(Lerp(SpanStart, SpanFinish, 0.5), Outward, Drop))
						{
							RejectEdge(SpanStart, SpanFinish, Reject::Stair, Drop);
							continue;
						}
						if (Config.MinEdgeAngleDegrees > 0 && Bend < Config.MinEdgeAngleDegrees && (Strength == Grade::Firm || Strength == Grade::Low))
						{
							Strength = Grade::Soft;
						}
						// Orient SpanA -> SpanB with the top on the left seen from above, so chains run one way round.
						const bool bFlip = Cross(Along, Outward).Z < 0;
						Lips.push_back({bFlip ? SpanB : SpanA, bFlip ? SpanA : SpanB, Patch, Outward, bNoSurface ? 0 : Drop, Bend, Strength, bSteepLip});
					}
				}
			}

			// Chains lips end to end. A chain stops at a fork, a sharp corner or a change of top or grade.
			std::vector<Chain> ChainLips(const std::vector<Lip>& Lips) const
			{
				std::unordered_map<uint32_t, std::vector<uint32_t>> AtVertex;
				for (uint32_t I = 0; I < Lips.size(); ++I)
				{
					AtVertex[Lips[I].A].push_back(I);
					AtVertex[Lips[I].B].push_back(I);
				}
				const double CornerCos = std::cos(Radians(Config.MaxCornerDegrees));
				std::vector<bool> Used(Lips.size(), false);
				auto Direction = [&](uint32_t L) { return Unit(Points[Lips[L].B] - Points[Lips[L].A]); };
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
						if (Used[size_t(L)])
						{
							break;
						}
						Used[size_t(L)] = true;
						Run.push_back(uint32_t(L));
					}
					if (!bClosed)
					{
						for (int64_t L = Next(Start, false); L >= 0 && !Used[size_t(L)]; L = Next(uint32_t(L), false))
						{
							Used[size_t(L)] = true;
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
						C.Outward.push_back(I + 1 < Run.size() ? Unit(L.Outward + Lips[Run[I + 1]].Outward) : L.Outward);
						C.Drops.push_back(L.Drop);
						C.Bends.push_back(L.Bend);
						C.bSteep |= L.bSteep;
					}
					if (C.bClosed)
					{
						const Vec3 SeamOutward = Unit(Lips[Run.back()].Outward + Lips[Run.front()].Outward);
						C.Outward.front() = C.Outward.back() = SeamOutward;
					}
					Chains.push_back(std::move(C));
				}
				return Chains;
			}

			std::vector<Side> MakeSides(const std::vector<Chain>& Chains) const
			{
				std::vector<Side> Sides;
				for (const Chain& C : Chains)
				{
					Side Made;
					Made.bClosed = C.bClosed;
					Made.Drop = Median(C.Drops);
					Made.Bend = Median(C.Bends);
					Made.Patch = C.Patch;
					Made.Strength = C.Strength;
					Made.bSteep = C.bSteep;
					for (uint32_t V : C.Vertices)
					{
						Made.Points.push_back(Points[V]);
					}
					Made.Outward = C.Outward;
					// A ledge running down a slope keeps its height over the ground; the side of a ramp rises out of
					// level ground, its fall growing as fast as it climbs.
					if (!C.bClosed && C.Drops.size() >= 2)
					{
						const double FirstZ = (Made.Points[0].Z + Made.Points[1].Z) / 2;
						const double LastZ = (Made.Points[Made.Points.size() - 2].Z + Made.Points.back().Z) / 2;
						double Horizontal = 0;
						for (size_t I = 0; I + 1 < Made.Points.size(); ++I)
						{
							Horizontal += FlatSize(Made.Points[I + 1] - Made.Points[I]);
						}
						const double Climb = LastZ - FirstZ;
						const double Grow = C.Drops.back() - C.Drops.front();
						Made.bRampSide = std::abs(Climb) >= RampMinClimb && Degrees(std::atan2(std::abs(Climb), Horizontal)) >= RampMinSlopeDegrees
							&& Grow * (Climb > 0 ? 1 : -1) >= RampDropShare * std::abs(Climb);
					}
					Sides.push_back(std::move(Made));
				}
				return Sides;
			}

			// How deep the body under a crest is: from just under the crest down to the mesh's own underside
			// (a tube's bottom, a wall's foot), or to whatever it rests on. A sheet with open edges has none.
			double CrestThickness(const std::vector<Vec3>& Crest, int32_t Patch) const
			{
				if (Patch >= 0 && PatchOpenPerimeter[size_t(Patch)] >= 0.5 * PatchPerimeter[size_t(Patch)])
				{
					return 0;
				}
				const double Length = PolylineLength(Crest);
				const int Count = std::clamp(int(Length / ThicknessSpacing), 1, 15);
				const double Reach = std::max(1.0, Config.RailMaxThickness) + ThicknessReach;
				std::vector<double> Values;
				for (int K = 0; K < Count; ++K)
				{
					const Vec3 P = PointAlongPolyline(Crest, Length * (K + 0.5) / Count);
					const Vec3 From = P - Vec3{0, 0, 0.5};
					const std::optional<SegmentHit> Hit = Own->FirstHit(From, P - Vec3{0, 0, Reach});
					if (Hit)
					{
						Values.push_back(0.5 + Hit->T * (Reach - 0.5));
						continue;
					}
					const std::optional<double> Ground = Below(From, Reach);
					Values.push_back(Ground ? P.Z - *Ground : Reach);
				}
				return Median(Values);
			}

			// On a narrow or round top, lips found on both sides are one line along the crest, midway between
			// them. The sides are matched point by point, so a side broken by a post or a rejected span still
			// pairs wherever the other side runs beside it, and only those stretches become crest. A top that
			// pairs anywhere loses its lips, so one side cannot carry a line past an obstruction on the other.
			// A narrow top with a lip on one side only, like coping level with a deck, stays a stone lip.
			void PairCrests(std::vector<Side>& Sides, std::vector<Piece>& Out)
			{
				std::unordered_map<int32_t, std::vector<size_t>> NarrowSides;
				for (size_t I = 0; I < Sides.size(); ++I)
				{
					if (IsNarrowish(Sides[I].Patch))
					{
						NarrowSides[Sides[I].Patch].push_back(I);
					}
				}
				std::vector<Piece> Crests;
				for (auto& [Patch, OnPatch] : NarrowSides)
				{
					if (OnPatch.size() < 2)
					{
						continue;
					}
					const double Width = PatchWidth(Patch);
					const double Reach = std::max(2 * Width, 4.0);
					const double Spread = std::max(Width, 2.0);
					std::sort(OnPatch.begin(), OnPatch.end(), [&](size_t L, size_t R) { return PolylineLength(Sides[L].Points) > PolylineLength(Sides[R].Points); });
					std::vector<std::vector<Vec3>> Runs;
					std::vector<bool> RunClosed;
					auto Flush = [&](std::vector<Vec3>& Run, bool bClosedRun)
					{
						if (Run.size() >= 2)
						{
							Runs.push_back(Run);
							RunClosed.push_back(bClosedRun);
						}
						Run.clear();
					};
					for (size_t SideIndex : OnPatch)
					{
						const Side& S = Sides[SideIndex];
						// A side that only confirms the other lays no crest of its own.
						if (S.Strength == Grade::Partner)
						{
							continue;
						}
						const std::vector<Sample> Samples = Resample(S.Points, S.Outward, RailSampleSpacing);
						// The other side is one running the opposite way, across the top from this one rather than
						// ahead of it or behind it.
						std::vector<std::optional<Vec3>> Middle(Samples.size());
						for (size_t K = 0; K < Samples.size(); ++K)
						{
							const Sample& Here = Samples[K];
							Nearest Best;
							for (size_t Other : OnPatch)
							{
								if (Other == SideIndex)
								{
									continue;
								}
								const Nearest N = NearestFromAbove(Here.Point, Sides[Other].Points);
								const Vec3 Across = Flat(N.Point - Here.Point);
								if (N.Distance < Best.Distance && Dot(N.Direction, Here.Direction) < -0.5 && Dot(Across, Here.Outward) < 0
									&& std::abs(Dot(Across, Unit(Flat(Here.Direction)))) <= Spread)
								{
									Best = N;
								}
							}
							if (Best.Distance > Reach)
							{
								continue;
							}
							const Vec3 Mid = (Here.Point + Best.Point) * 0.5;
							bool bCovered = false;
							for (const std::vector<Vec3>& Run : Runs)
							{
								if (NearestFromAbove(Mid, Run).Distance <= Spread)
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
						if (S.bClosed && Samples.size() > 2)
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
					if (Runs.empty())
					{
						continue;
					}
					std::vector<double> SideDrops;
					double LongestSide = 0;
					bool bPartner = false;
					bool bSoft = false;
					double SideBend = 90;
					for (size_t SideIndex : OnPatch)
					{
						Sides[SideIndex].bKeep = false;
						SideDrops.push_back(Sides[SideIndex].Drop);
						LongestSide = std::max(LongestSide, PolylineLength(Sides[SideIndex].Points));
						bPartner |= Sides[SideIndex].Strength == Grade::Partner;
						bSoft |= Sides[SideIndex].Strength == Grade::Soft;
						SideBend = std::min(SideBend, Sides[SideIndex].Bend);
					}
					double CrestLength = 0;
					for (const std::vector<Vec3>& Run : Runs)
					{
						CrestLength += PolylineLength(Run);
					}
					// A crest is read from the mesh's own triangles, not from the world: collision is often simpler
					// than the visible tube, and the line belongs on the tube.
					const double Lift = std::max(Config.RailMaxWidth, Width / 2 + 5);
					for (size_t K = 0; K < Runs.size(); ++K)
					{
						Piece Crest;
						Crest.Shape = LineShape::Crest;
						Crest.Points = std::move(Runs[K]);
						Crest.bClosed = RunClosed[K];
						for (Vec3& P : Crest.Points)
						{
							const std::optional<double> Top = Own->Below({P.X, P.Y, P.Z + Lift}, 2 * Lift);
							P.Z = Top ? *Top : P.Z;
						}
						if (Crest.bClosed)
						{
							Crest.Points.back() = Crest.Points.front();
						}
						Crest.Drop = Median(SideDrops);
						Crest.TopWidth = Width;
						Crest.Bend = SideBend;
						Crest.Thickness = CrestThickness(Crest.Points, Patch);
						const bool bRound = PatchRound[size_t(Patch)];
						const bool bThin = Config.RailMaxThickness <= 0 || Crest.Thickness <= Config.RailMaxThickness;
						const bool bTube = bRound && Width <= Config.RailMaxWidth * TubeShare;
						Crest.Kind = Width <= Config.RailMaxWidth && (bThin || bTube) ? LineKind::Rail : LineKind::Stone;
						Crest.NoteBits |= Width > Config.RailMaxWidth ? Notes::RoundTop : 0;
						Crest.NoteBits |= bPartner && Crest.Kind == LineKind::Rail ? Notes::Coping : 0;
						Crest.NoteBits |= CrestLength < PartialRailShare * LongestSide ? Notes::PartialRail : 0;
						Crest.NoteBits |= bSoft ? Notes::SoftEdge : 0;
						Crest.Meshes = {MeshIndex};
						Out.push_back(std::move(Crest));
					}
				}
			}

			void EmitLips(std::vector<Side>& Sides, std::vector<Piece>& Out)
			{
				for (Side& S : Sides)
				{
					if (!S.bKeep)
					{
						continue;
					}
					// A narrow top's side that fell too little for a ledge and found no other side stays unlined, and
					// so does an edge the surface only rounds over.
					if (S.Strength == Grade::Partner || S.Strength == Grade::Rail || S.Strength == Grade::Soft)
					{
						RejectEdge(S.Points.front(), S.Points.back(), S.Strength == Grade::Soft ? Reject::RoundedEdge : Reject::Unpaired, S.Drop);
						continue;
					}
					if (S.bRampSide)
					{
						RejectEdge(S.Points.front(), S.Points.back(), Reject::RampSide, S.Drop);
						continue;
					}
					Piece Lip;
					Lip.Kind = LineKind::Stone;
					Lip.Shape = LineShape::Lip;
					Lip.Points = std::move(S.Points);
					Lip.Outward = std::move(S.Outward);
					Lip.bClosed = S.bClosed;
					Lip.Drop = S.Drop;
					Lip.TopWidth = PatchWidth(S.Patch);
					Lip.Bend = S.Bend;
					Lip.LowLength = S.Strength == Grade::Low ? PolylineLength(Lip.Points) : 0;
					Lip.NoteBits |= S.bSteep ? Notes::SteepLip : 0;
					Lip.NoteBits |= Config.MinEdgeAngleDegrees > 0 && S.Bend < Config.MinEdgeAngleDegrees + SoftEdgeMargin ? Notes::SoftEdge : 0;
					Lip.Meshes = {MeshIndex};
					Out.push_back(std::move(Lip));
				}
			}

			// A sharp ridge where two sloping faces meet with no top between them, both falling away from it.
			void FindRidges(std::vector<Piece>& Out)
			{
				const double UpCos = std::cos(Radians(RidgeMaxFaceTiltDegrees));
				const double MinSlopeSin = std::sin(Radians(RidgeMinFaceSlopeDegrees));
				const double MaxLineSlope = std::tan(Radians(Config.MaxLineSlopeDegrees));
				std::vector<std::pair<uint32_t, uint32_t>> Ridges;
				std::vector<double> RidgeDrops;
				for (const auto& [Key, Shared] : EdgeFaces)
				{
					if (Shared.size() != 2)
					{
						continue;
					}
					const Face& First = Faces[Shared[0]];
					const Face& Second = Faces[Shared[1]];
					if (First.Normal.Z < UpCos || Second.Normal.Z < UpCos || FlatSize(First.Normal) < MinSlopeSin || FlatSize(Second.Normal) < MinSlopeSin)
					{
						continue;
					}
					if (First.Patch >= 0 && First.Patch == Second.Patch && IsNarrowish(First.Patch))
					{
						continue;
					}
					if (AngleBetween(First.Normal, Second.Normal) < Config.MinRidgeAngleDegrees)
					{
						continue;
					}
					const uint32_t A = uint32_t(Key >> 32);
					const uint32_t B = uint32_t(Key & 0xffffffff);
					const Vec3 Along = Points[B] - Points[A];
					if (FlatSize(Along) <= 0 || std::abs(Along.Z) > MaxLineSlope * FlatSize(Along))
					{
						continue;
					}
					// Each face falls away on its own side: its third vertex lies below the other face's plane, and
					// its downhill direction leads away from the edge.
					bool bRidge = true;
					for (size_t K = 0; K < 2 && bRidge; ++K)
					{
						const uint32_t Mine = Shared[K];
						const uint32_t Theirs = Shared[1 - K];
						const Vec3 Third = Points[ThirdVertex(Faces[Mine], A, B)];
						const Vec3 Across = Flat(Third - Points[A]);
						const Vec3 EdgeFlat = Unit(Flat(Along));
						const Vec3 Away = Unit(Across - EdgeFlat * Dot(Across, EdgeFlat));
						bRidge = Dot(Third - Points[A], Faces[Theirs].Normal) < -0.01 && Dot(Flat(Faces[Mine].Normal), Away) > 0;
					}
					if (!bRidge)
					{
						continue;
					}
					// Nothing stands over it, and the surface falls away on both sides.
					const Vec3 Middle = Lerp(Points[A], Points[B], 0.5);
					if (FirstHit && FirstHit(Middle + Vec3{0, 0, 1}, Middle + Vec3{0, 0, Config.Clearance}))
					{
						RejectEdge(Points[A], Points[B], Reject::Walled, 0);
						continue;
					}
					const Vec3 Left = Unit(Vec3{-Along.Y, Along.X, 0});
					double Drop = std::numeric_limits<double>::max();
					for (const Vec3& Aside : {Left, Left * -1})
					{
						const Vec3 Probe = Middle + Aside * Config.ProbeDistance;
						const std::optional<double> Hit = Below({Probe.X, Probe.Y, Middle.Z + 0.5}, Config.MaxDropSearch);
						Drop = std::min(Drop, Hit ? Middle.Z - *Hit : Config.MaxDropSearch);
					}
					if (Drop < Config.RailMinDrop)
					{
						RejectEdge(Points[A], Points[B], Reject::SmallFall, Drop);
						continue;
					}
					Ridges.push_back({A, B});
					RidgeDrops.push_back(Drop);
				}
				// Chain ridge edges through vertices where exactly two meet without turning sharply.
				std::unordered_map<uint32_t, std::vector<size_t>> AtVertex;
				for (size_t I = 0; I < Ridges.size(); ++I)
				{
					AtVertex[Ridges[I].first].push_back(I);
					AtVertex[Ridges[I].second].push_back(I);
				}
				const double CornerCos = std::cos(Radians(Config.MaxCornerDegrees));
				std::vector<bool> Used(Ridges.size(), false);
				for (size_t Start = 0; Start < Ridges.size(); ++Start)
				{
					if (Used[Start])
					{
						continue;
					}
					Used[Start] = true;
					std::vector<uint32_t> Run{Ridges[Start].first, Ridges[Start].second};
					std::vector<double> Drops{RidgeDrops[Start]};
					bool bClosed = false;
					for (int Direction = 0; Direction < 2 && !bClosed; ++Direction)
					{
						while (true)
						{
							const uint32_t Joint = Run.back();
							const uint32_t Before = Run[Run.size() - 2];
							const std::vector<size_t>& Here = AtVertex[Joint];
							if (Here.size() != 2)
							{
								break;
							}
							const size_t Onward = Used[Here[0]] ? Here[1] : Here[0];
							if (Used[Onward])
							{
								bClosed = Run.front() == Joint || (Ridges[Onward].first == Run.front() || Ridges[Onward].second == Run.front());
								break;
							}
							const uint32_t NextVertex = Ridges[Onward].first == Joint ? Ridges[Onward].second : Ridges[Onward].first;
							if (Dot(Unit(Points[Joint] - Points[Before]), Unit(Points[NextVertex] - Points[Joint])) < CornerCos)
							{
								break;
							}
							Used[Onward] = true;
							Run.push_back(NextVertex);
							Drops.push_back(RidgeDrops[Onward]);
							if (NextVertex == Run.front())
							{
								bClosed = true;
								break;
							}
						}
						if (!bClosed)
						{
							std::reverse(Run.begin(), Run.end());
						}
					}
					Piece Ridge;
					Ridge.Kind = LineKind::Rail;
					Ridge.Shape = LineShape::Ridge;
					for (uint32_t V : Run)
					{
						Ridge.Points.push_back(Points[V]);
					}
					Ridge.bClosed = bClosed && Ridge.Points.size() > 3 && Size(Ridge.Points.front() - Ridge.Points.back()) < 1e-6;
					Ridge.Drop = *std::min_element(Drops.begin(), Drops.end());
					Ridge.NoteBits = Notes::Ridge;
					Ridge.Meshes = {MeshIndex};
					Out.push_back(std::move(Ridge));
				}
			}
		};

		// ------------------------------------------------------------------------------------------------
		// Every mesh together: joining pieces that meet.
		// ------------------------------------------------------------------------------------------------

		class Joiner
		{
		public:
			Joiner(std::vector<Piece>& InPieces, const SurfaceBelow& InBelow, const SegmentTest& InFirstHit, const Settings& InConfig)
				: Pieces(InPieces), Below(InBelow), FirstHit(InFirstHit), Config(InConfig)
			{
				MaxLineSlope = std::tan(Radians(Config.MaxLineSlopeDegrees));
				CornerCos = std::cos(Radians(Config.MaxCornerDegrees));
			}

			void Run()
			{
				Deduplicate();
				JoinEnds();
				CloseRings();
			}

		private:
			std::vector<Piece>& Pieces;
			const SurfaceBelow& Below;
			const SegmentTest& FirstHit;
			const Settings& Config;
			double MaxLineSlope = 1;
			double CornerCos = 0.7;

			bool SharesMesh(const Piece& A, const Piece& B) const
			{
				for (size_t M : A.Meshes)
				{
					if (std::find(B.Meshes.begin(), B.Meshes.end(), M) != B.Meshes.end())
					{
						return true;
					}
				}
				return false;
			}

			bool MayMeet(const Piece& A, const Piece& B) const
			{
				// A low ledge is reviewed on its own: it does not ride on a firm ledge's line.
				return A.Kind == B.Kind && (A.LowLength > 0) == (B.LowLength > 0) && (Config.bJoinAcrossMeshes || SharesMesh(A, B));
			}

			// Takes on what Second adds: its meshes, notes and smallest fall.
			void Absorb(Piece& First, const Piece& Second)
			{
				for (size_t M : Second.Meshes)
				{
					if (std::find(First.Meshes.begin(), First.Meshes.end(), M) == First.Meshes.end())
					{
						First.Meshes.push_back(M);
					}
				}
				First.NoteBits |= Second.NoteBits;
				First.Drop = std::min(First.Drop, Second.Drop);
				First.TopWidth = std::max(First.TopWidth, Second.TopWidth);
				First.Thickness = std::max(First.Thickness, Second.Thickness);
				First.Bend = std::min(First.Bend, Second.Bend);
			}

			void Append(Piece& First, Piece& Second, size_t Skip)
			{
				First.Points.insert(First.Points.end(), Second.Points.begin() + std::ptrdiff_t(Skip), Second.Points.end());
				if (First.IsOriented() && Second.IsOriented())
				{
					First.Outward.insert(First.Outward.end(), Second.Outward.begin() + std::ptrdiff_t(Skip), Second.Outward.end());
				}
				else
				{
					First.Outward.clear();
				}
				First.LowLength += Second.LowLength;
				Absorb(First, Second);
				Second.bKeep = false;
			}

			// A gap is crossed only where nothing stands over the line or beside it, and a ledge must still fall
			// away past it.
			bool GapClear(const Vec3& From, const Vec3& To, const Piece& P) const
			{
				const Vec3 Along = Unit(Flat(To - From));
				std::vector<Vec3> Asides;
				if (P.IsOriented())
				{
					Asides = {P.Outward.back()};
				}
				else
				{
					Asides = {Vec3{-Along.Y, Along.X, 0}, Vec3{Along.Y, -Along.X, 0}};
				}
				const double HalfWidth = P.HalfWidth();
				const double LedgeDrop = std::min(Config.MinDrop, std::max(Config.LowLedgeMinDrop, P.Drop * 0.8));
				const int Count = std::max(1, int(std::ceil(Size(To - From) / Config.SampleSpacing * 2)));
				for (int K = 0; K < Count; ++K)
				{
					const Vec3 Point = Lerp(From, To, (K + 0.5) / Count);
					if (Below({Point.X, Point.Y, Point.Z + Config.Clearance}, Config.Clearance - 2 * CoverInset))
					{
						return false;
					}
					if (FirstHit && FirstHit(Point + Vec3{0, 0, 1}, Point + Vec3{0, 0, Config.Clearance}))
					{
						return false;
					}
					for (const Vec3& Aside : Asides)
					{
						const Vec3 Probe = Point + Aside * (HalfWidth + Config.ProbeDistance);
						const std::optional<double> Hit = Below({Probe.X, Probe.Y, Point.Z + Config.Clearance}, Config.Clearance + Config.MaxDropSearch);
						// Beside a crest nothing may stand above it; past a ledge the surface must fall.
						if (P.IsOriented() ? !Hit || Point.Z - *Hit < LedgeDrop : Hit && *Hit > Point.Z + CoverInset)
						{
							return false;
						}
						const Vec3 Lifted = Point + Vec3{0, 0, BesideLift};
						if (FirstHit && FirstHit(Lifted + Aside * HalfWidth, Lifted + Aside * (HalfWidth + Config.ProbeDistance)))
						{
							return false;
						}
					}
				}
				return true;
			}

			// Joins Second onto the end of First when Second starts where First ends, or a little behind it as
			// modules pushed into one another do, and runs on the same way.
			bool TryJoin(Piece& First, Piece& Second)
			{
				if (First.Points.size() < 2 || Second.Points.size() < 2)
				{
					return false;
				}
				const Vec3 End = First.Points.back();
				const Vec3 Start = Second.Points.front();
				const Vec3 EndDirection = Unit(Flat(End - First.Points[First.Points.size() - 2]));
				const Vec3 StartDirection = Unit(Flat(Second.Points[1] - Start));
				if (Size(EndDirection) == 0 || Size(StartDirection) == 0 || Dot(EndDirection, StartDirection) < CornerCos)
				{
					return false;
				}
				if (First.IsOriented() && Second.IsOriented() && Dot(First.Outward.back(), Second.Outward.front()) < 0)
				{
					return false;
				}
				const double MaxGap = First.Kind == LineKind::Rail ? Config.RailJoinGap : Config.JoinGap;
				const Vec3 Gap = Start - End;
				const double FlatGap = FlatSize(Gap);
				const double Ahead = Dot(Flat(Gap), EndDirection);
				if (Ahead >= -1)
				{
					if (FlatGap > MaxGap || std::abs(Gap.Z) > std::max(2.0, FlatGap * MaxLineSlope))
					{
						return false;
					}
					// Beyond a touch, the gap must lie straight ahead: not a parallel line alongside.
					if (FlatGap > 1)
					{
						const double Sideways = std::abs(Gap.X * EndDirection.Y - Gap.Y * EndDirection.X);
						if (Dot(Unit(Flat(Gap)), EndDirection) < CornerCos || Sideways > std::max(1.5, 0.2 * FlatGap) || !GapClear(End, Start, First))
						{
							return false;
						}
					}
					Append(First, Second, FlatGap <= 1 ? 1 : 0);
					return true;
				}
				// Second starts behind First's end: every point of it up to First's end must lie along First.
				const double Tolerance = First.IsOriented() ? OverlapTolerance : RailOverlapTolerance;
				size_t Cut = 0;
				while (Cut < Second.Points.size() && Dot(Flat(Second.Points[Cut] - End), EndDirection) <= 0)
				{
					++Cut;
				}
				for (size_t K = 0; K < Cut; ++K)
				{
					const std::optional<PolylinePoint> On = NearestOnPolyline(First.Points, Second.Points[K]);
					if (!On || On->Distance > Tolerance)
					{
						return false;
					}
				}
				if (Cut == Second.Points.size())
				{
					// All of Second lies along First's end.
					First.LowLength += Second.LowLength;
					Absorb(First, Second);
					Second.bKeep = false;
					return true;
				}
				const Vec3 P0 = Second.Points[Cut - 1];
				const Vec3 P1 = Second.Points[Cut];
				const double A0 = Dot(Flat(P0 - End), EndDirection);
				const double A1 = Dot(Flat(P1 - End), EndDirection);
				const Vec3 Crossing = Lerp(P0, P1, A1 > A0 ? std::clamp(-A0 / (A1 - A0), 0.0, 1.0) : 0.0);
				if (Size(Crossing - End) > Tolerance)
				{
					return false;
				}
				Second.Points.erase(Second.Points.begin(), Second.Points.begin() + std::ptrdiff_t(Cut));
				if (Second.IsOriented())
				{
					Second.Outward.erase(Second.Outward.begin(), Second.Outward.begin() + std::ptrdiff_t(Cut));
				}
				Append(First, Second, Size(Second.Points.front() - End) <= 1 ? 1 : 0);
				return true;
			}

			// Drops pieces that lie wholly along a longer piece of the same kind: the same edge found on two
			// stacked or duplicated meshes.
			void Deduplicate()
			{
				std::vector<size_t> Order(Pieces.size());
				for (size_t I = 0; I < Order.size(); ++I)
				{
					Order[I] = I;
				}
				std::sort(Order.begin(), Order.end(), [&](size_t L, size_t R) { return PolylineLength(Pieces[L].Points) > PolylineLength(Pieces[R].Points); });
				std::vector<std::array<Vec3, 2>> Bounds(Pieces.size());
				for (size_t I = 0; I < Pieces.size(); ++I)
				{
					Vec3 Low{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
					Vec3 High = Low * -1;
					for (const Vec3& P : Pieces[I].Points)
					{
						Low = {std::min(Low.X, P.X), std::min(Low.Y, P.Y), std::min(Low.Z, P.Z)};
						High = {std::max(High.X, P.X), std::max(High.Y, P.Y), std::max(High.Z, P.Z)};
					}
					Bounds[I] = {Low, High};
				}
				for (size_t OI = 0; OI < Order.size(); ++OI)
				{
					Piece& Long = Pieces[Order[OI]];
					if (!Long.bKeep)
					{
						continue;
					}
					for (size_t OJ = OI + 1; OJ < Order.size(); ++OJ)
					{
						Piece& Short = Pieces[Order[OJ]];
						if (!Short.bKeep || !MayMeet(Long, Short) || Long.IsOriented() != Short.IsOriented())
						{
							continue;
						}
						const std::array<Vec3, 2>& LB = Bounds[Order[OI]];
						const std::array<Vec3, 2>& SB = Bounds[Order[OJ]];
						const double Slack = RailOverlapTolerance;
						if (SB[0].X > LB[1].X + Slack || SB[1].X < LB[0].X - Slack || SB[0].Y > LB[1].Y + Slack || SB[1].Y < LB[0].Y - Slack || SB[0].Z > LB[1].Z + Slack || SB[1].Z < LB[0].Z - Slack)
						{
							continue;
						}
						const double Tolerance = Long.IsOriented() ? OverlapTolerance : RailOverlapTolerance;
						bool bAlong = true;
						for (size_t K = 0; K + 1 < Short.Points.size() && bAlong; ++K)
						{
							const int Steps = std::max(1, int(std::ceil(Size(Short.Points[K + 1] - Short.Points[K]) / 5)));
							for (int S = 0; S <= Steps && bAlong; ++S)
							{
								const std::optional<PolylinePoint> On = NearestOnPolyline(Long.Points, Lerp(Short.Points[K], Short.Points[K + 1], double(S) / Steps));
								bAlong = On && On->Distance <= Tolerance;
							}
						}
						// Two lips along one edge must also face the same way: the same top's edge, not two tops back to back.
						if (bAlong && Long.IsOriented())
						{
							const std::optional<PolylinePoint> On = NearestOnPolyline(Long.Points, Short.Points.front());
							bAlong = On && Dot(OutwardAt(Long, On->Along), Short.Outward.front()) > 0;
						}
						if (bAlong)
						{
							Absorb(Long, Short);
							Short.bKeep = false;
						}
					}
				}
			}

			void JoinEnds()
			{
				const double CellSize = std::max({Config.JoinGap, Config.RailJoinGap, 10.0}) + 5;
				auto CellOf = [&](const Vec3& P) { return std::pair<int64_t, int64_t>{int64_t(std::floor(P.X / CellSize)), int64_t(std::floor(P.Y / CellSize))}; };
				auto KeyOf = [](int64_t X, int64_t Y) { return (uint64_t(uint32_t(int32_t(X))) << 32) | uint32_t(int32_t(Y)); };
				for (bool bJoined = true; bJoined;)
				{
					bJoined = false;
					// Every open piece's ends, by grid cell.
					std::unordered_map<uint64_t, std::vector<size_t>> Ends;
					for (size_t I = 0; I < Pieces.size(); ++I)
					{
						if (!Pieces[I].bKeep || Pieces[I].bClosed || Pieces[I].Points.size() < 2)
						{
							continue;
						}
						for (const Vec3* P : {&Pieces[I].Points.front(), &Pieces[I].Points.back()})
						{
							const auto [X, Y] = CellOf(*P);
							std::vector<size_t>& Here = Ends[KeyOf(X, Y)];
							if (std::find(Here.begin(), Here.end(), I) == Here.end())
							{
								Here.push_back(I);
							}
						}
					}
					auto Nearby = [&](const Vec3& P)
					{
						std::vector<size_t> Found;
						const auto [X, Y] = CellOf(P);
						for (int64_t OX = -1; OX <= 1; ++OX)
						{
							for (int64_t OY = -1; OY <= 1; ++OY)
							{
								const auto Cell = Ends.find(KeyOf(X + OX, Y + OY));
								if (Cell != Ends.end())
								{
									Found.insert(Found.end(), Cell->second.begin(), Cell->second.end());
								}
							}
						}
						std::sort(Found.begin(), Found.end());
						Found.erase(std::unique(Found.begin(), Found.end()), Found.end());
						return Found;
					};
					for (size_t I = 0; I < Pieces.size(); ++I)
					{
						for (int Turn = 0; Turn < 2; ++Turn)
						{
							// An unoriented piece may grow from either end: the second time round, from its start.
							if (Turn == 1)
							{
								if (Pieces[I].IsOriented() || !Pieces[I].bKeep || Pieces[I].bClosed)
								{
									break;
								}
								Reverse(Pieces[I]);
							}
							for (bool bGrew = true; bGrew && Pieces[I].bKeep && !Pieces[I].bClosed;)
							{
								bGrew = false;
								// Nearest candidates first, so a chain of modules joins in order.
								std::vector<size_t> Candidates = Nearby(Pieces[I].Points.back());
								const Vec3 Tip = Pieces[I].Points.back();
								std::sort(Candidates.begin(), Candidates.end(), [&](size_t L, size_t R)
								{
									const double DL = std::min(Size(Pieces[L].Points.front() - Tip), Size(Pieces[L].Points.back() - Tip));
									const double DR = std::min(Size(Pieces[R].Points.front() - Tip), Size(Pieces[R].Points.back() - Tip));
									return DL < DR;
								});
								for (size_t J : Candidates)
								{
									if (J == I || !Pieces[J].bKeep || Pieces[J].bClosed || !MayMeet(Pieces[I], Pieces[J]) || Pieces[J].Points.size() < 2)
									{
										continue;
									}
									if (TryJoin(Pieces[I], Pieces[J]))
									{
										bGrew = true;
									}
									else if (!Pieces[J].IsOriented())
									{
										// A crest has no side to keep to: its pieces may meet either way round.
										Reverse(Pieces[J]);
										if (TryJoin(Pieces[I], Pieces[J]))
										{
											bGrew = true;
										}
										else
										{
											Reverse(Pieces[J]);
										}
									}
									if (bGrew)
									{
										bJoined = true;
										break;
									}
								}
							}
						}
					}
				}
			}

			// A piece whose ends meet across a clear gap closes, as a ring rail joined all the way round.
			void CloseRings()
			{
				for (Piece& C : Pieces)
				{
					const double MaxGap = C.Kind == LineKind::Rail ? Config.RailJoinGap : Config.JoinGap;
					if (!C.bKeep || C.bClosed || C.Points.size() < 4 || PolylineLength(C.Points) < 4 * MaxGap)
					{
						continue;
					}
					const Vec3 Gap = C.Points.front() - C.Points.back();
					const double FlatGap = FlatSize(Gap);
					const Vec3 EndDirection = Unit(Flat(C.Points.back() - C.Points[C.Points.size() - 2]));
					const Vec3 StartDirection = Unit(Flat(C.Points[1] - C.Points[0]));
					if (FlatGap > MaxGap || std::abs(Gap.Z) > std::max(2.0, FlatGap * MaxLineSlope) || Dot(EndDirection, StartDirection) < CornerCos)
					{
						continue;
					}
					if (FlatGap > 1 && (Dot(Unit(Flat(Gap)), EndDirection) < CornerCos || !GapClear(C.Points.back(), C.Points.front(), C)))
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
						if (C.IsOriented())
						{
							C.Outward.push_back(C.Outward.front());
						}
					}
					C.bClosed = true;
				}
			}
		};

		double ConfidenceFor(uint32_t NoteBits, const Settings& Config)
		{
			double Confidence = 1;
			if ((NoteBits & Notes::LowLedge) && Config.LowLedges != LowLedgeMode::Keep)
			{
				Confidence *= 0.45;
			}
			Confidence *= NoteBits & Notes::HighDrop ? 0.65 : 1;
			Confidence *= NoteBits & Notes::SoftEdge ? 0.75 : 1;
			Confidence *= NoteBits & Notes::PartialRail ? 0.85 : 1;
			Confidence *= NoteBits & Notes::Ridge ? 0.45 : 1;
			Confidence *= NoteBits & Notes::RoundTop ? 0.85 : 1;
			Confidence *= NoteBits & Notes::Short ? 0.85 : 1;
			Confidence *= NoteBits & Notes::SteepLip ? 0.9 : 1;
			return Confidence;
		}
	}

	std::vector<Line> FindGrindLines(const std::vector<Mesh>& Meshes, const SurfaceBelow& Below, const SegmentTest& FirstHit, const Settings& Config, const ScanOptions& Options)
	{
		if (Options.bCancelled)
		{
			*Options.bCancelled = false;
		}
		std::vector<Piece> Pieces;
		for (size_t I = 0; I < Meshes.size(); ++I)
		{
			if (Options.Progress && !Options.Progress(I, Meshes.size()))
			{
				if (Options.bCancelled)
				{
					*Options.bCancelled = true;
				}
				break;
			}
			MeshScan(Meshes[I], I, Below, FirstHit, Config, Options.Rejections).Run(Pieces);
		}
		Joiner(Pieces, Below, FirstHit, Config).Run();

		std::vector<Line> Out;
		for (Piece& P : Pieces)
		{
			if (!P.bKeep || P.Points.size() < 2)
			{
				continue;
			}
			Line L;
			L.Kind = P.Kind;
			L.Shape = P.Shape;
			L.bClosed = P.bClosed;
			L.Points = SimplifyPolyline(P.Points, Config.SimplifyTolerance, P.bClosed);
			L.Length = PolylineLength(L.Points);
			L.MeshIndices = P.Meshes;
			L.MeshIndex = P.Meshes.empty() ? 0 : P.Meshes.front();
			L.Drop = P.Drop;
			L.TopWidth = P.TopWidth;
			L.Thickness = P.Thickness;
			L.EdgeAngle = P.Bend;
			// A sub-millimetre allowance: an edge exactly Min Length long must not come and go with rounding as the
			// mesh is turned.
			if (L.Length < Config.MinLength - LengthSlack)
			{
				if (Options.Rejections)
				{
					Options.Rejections->push_back({L.Points.front(), L.Points.back(), Reject::Short, L.Drop, L.MeshIndex});
				}
				continue;
			}
			uint32_t NoteBits = P.NoteBits;
			NoteBits &= ~uint32_t(Notes::LowLedge);
			NoteBits |= P.LowLength >= 0.5 * PolylineLength(P.Points) ? Notes::LowLedge : 0;
			NoteBits |= Config.HighDrop > 0 && L.Drop >= Config.HighDrop ? Notes::HighDrop : 0;
			NoteBits |= L.Length < 2 * Config.MinLength - LengthSlack ? Notes::Short : 0;
			NoteBits |= L.MeshIndices.size() > 1 ? Notes::Joined : 0;
			L.LineNotes = NoteBits;
			L.Confidence = ConfidenceFor(NoteBits, Config);
			L.bSuggested = L.Confidence >= Config.KeepConfidence;
			Out.push_back(std::move(L));
		}
		return Out;
	}

	std::vector<Line> FindGrindLines(const std::vector<Mesh>& Meshes, const SurfaceBelow& Below, const SegmentTest& FirstHit, const Settings& Config, std::vector<Rejection>* Rejections)
	{
		ScanOptions Options;
		Options.Rejections = Rejections;
		return FindGrindLines(Meshes, Below, FirstHit, Config, Options);
	}

	std::vector<Line> FindGrindLines(const std::vector<Mesh>& Meshes, const SurfaceBelow& Below, const Settings& Config, std::vector<Rejection>* Rejections)
	{
		return FindGrindLines(Meshes, Below, SegmentTest(), Config, Rejections);
	}
}
