// Finds grindable lines on triangle meshes: ledge lips, box edges, coping, rails, pipes and ridges.
//
// Engine-free on purpose: the editor module, the standalone scorer and the benchmark in Tests/ compile
// this same code, so the detector can be measured without the engine. Units are centimetres with Z up,
// as in UE world space.
//
// Every translation unit of the editor module is compiled with Unreal's shared precompiled header, so
// names here must not meet engine or Windows macros: no all-caps identifiers and none of near, far,
// small, interface or the Windows API names.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace AutoGrindCore
{
	struct Vec3
	{
		double X = 0;
		double Y = 0;
		double Z = 0;
	};

	struct Mesh
	{
		std::string Name;
		std::vector<Vec3> Vertices; // world space
		// Three per triangle, wound so Cross(B - A, C - A) points out of the surface: the side the
		// renderer draws. Stray triangles wound the other way are repaired (Settings::bRepairWinding).
		std::vector<uint32_t> Indices;
	};

	enum class LineKind : uint8_t
	{
		Stone, // a ledge, box edge, wall top, pipe or coping lip: GrindActor's stone grind
		Rail // a narrow round or thin top with a fall on both sides: GrindActor's rail grind
	};

	// Where on the geometry a line runs.
	enum class LineShape : uint8_t
	{
		Lip, // the outer edge of a top: a ledge, a box edge, a bank or quarter-pipe lip
		Crest, // along the middle of a narrow or round top: a rail, a pipe, a thin wall
		Ridge // a sharp convex ridge with no top: both faces fall away from it
	};

	// Why a line may deserve a second look. Each lowers its confidence; see Settings::KeepConfidence.
	namespace Notes
	{
		constexpr uint32_t LowLedge = 1u << 0; // falls less than Min Drop: a curb, a manual pad or a low step
		constexpr uint32_t HighDrop = 1u << 1; // falls further than High Drop: often the back of a deck or a wall top
		constexpr uint32_t SoftEdge = 1u << 2; // the edge is rounded, only just sharp enough to count
		constexpr uint32_t PartialRail = 1u << 3; // only part of a narrow top paired into a rail
		constexpr uint32_t Ridge = 1u << 4; // a ridge with no top to stand on
		constexpr uint32_t RoundTop = 1u << 5; // along the crest of a round top wider than a rail
		constexpr uint32_t Joined = 1u << 6; // runs across more than one mesh
		constexpr uint32_t SteepLip = 1u << 7; // drops onto a steep slope: a bank or quarter-pipe lip
		constexpr uint32_t Short = 1u << 8; // less than twice Min Length
		constexpr uint32_t Coping = 1u << 9; // a rail with a deck or ledge just below one side
		constexpr uint32_t Count = 10;
	}
	// A short description of one Notes bit, or "" for an unknown one.
	const char* DescribeNote(uint32_t Note);

	struct Line
	{
		LineKind Kind = LineKind::Stone;
		LineShape Shape = LineShape::Lip;
		std::vector<Vec3> Points;
		// A closed line repeats its first point at the end.
		bool bClosed = false;
		// The mesh most of the line runs along, and every mesh it runs along with that one first.
		size_t MeshIndex = 0;
		std::vector<size_t> MeshIndices;
		double Length = 0;
		double Drop = 0; // smallest fall just past the edge (for a crest, past either side)
		double TopWidth = 0; // 2 x area / perimeter of the top the line runs along
		double Thickness = 0; // depth of the body under a crest; 0 when not measured
		double EdgeAngle = 0; // how sharply the surface turns over the edge, in degrees (convexity)
		double Confidence = 1; // 0 to 1, lowered by Notes
		uint32_t LineNotes = 0;
		// Confidence reaches Settings::KeepConfidence: the line should be kept, not just reviewed.
		bool bSuggested = true;
	};

	// What to do with an edge that falls less than Min Drop onto a wide flat landing (not a stair).
	enum class LowLedgeMode : uint8_t
	{
		Off, // leave it out
		Suggest, // list it unticked for review
		Keep // list it ticked
	};

	struct Settings
	{
		// A face this flat or flatter is a top; only a top's outer edge can carry a line.
		double MaxTopSlopeDegrees = 45;
		// How far the surface must fall just past the edge.
		double MinDrop = 25;
		// How far past the edge the fall is measured.
		double ProbeDistance = 15;
		// An edge that falls onto a slope this steep is a lip even when it falls less than MinDrop,
		// as at the top of a bank or a quarter without coping. 0 turns this off.
		double SteepLipDegrees = 40;
		// Height above the edge that must be clear at the probe, so an edge against a wall is not a lip.
		double Clearance = 150;
		// With no surface this far below the probe, the edge overhangs nothing.
		double MaxDropSearch = 5000;
		double MinLength = 50;
		double MaxLineSlopeDegrees = 45;
		// A sharper turn ends one line and starts the next, as at a box corner.
		double MaxCornerDegrees = 40;
		// A top narrower than this with a fall on both sides gives one line along its crest: a rail when
		// the top is round or the body under it is thin, otherwise a stone line (a thin wall or beam).
		double RailMaxWidth = 15;
		// How far the surface must fall past the side of a top that narrow. A rail stands clear of what is
		// below it, such as the wall a railing runs along, by less than a ledge must.
		double RailMinDrop = 10;
		// A flat narrow top on a body deeper than this is a wall or beam: stone. 0 makes every narrow top a rail.
		double RailMaxThickness = 20;
		// A round top up to this wide (a pipe) gives one line along its crest instead of two on its shoulders.
		double RoundTopMaxWidth = 40;
		double WeldTolerance = 0.1;
		double SimplifyTolerance = 1;
		// Test long edges in separate spans; failed spans split the line instead of hiding an obstacle.
		double SampleSpacing = 25;
		// A flat surface this close past an edge and less than MinDrop below it carries the top on: the edge
		// is a seam, a small step or the gap between two slats of a bench, not a lip.
		double GapBridge = 6;
		// Pieces of one ledge line are joined across a gap this short, such as the seam between two blocks.
		double JoinGap = 10;
		// Pieces of one rail are joined across a gap this short, where a post or bracket meets the tube.
		double RailJoinGap = 40;
		// Join lines of separate meshes placed next to each other (modular pieces) into one line.
		bool bJoinAcrossMeshes = true;
		// Convexity: the surface must turn down over an edge by at least this much, measured across a few
		// centimetres, so domes, mounds and rounded shapes are not lined. 0 turns this off.
		double MinEdgeAngleDegrees = 30;
		// Edges falling further than this are left out (roofs, cliffs). 0 means no limit.
		double MaxDrop = 0;
		// Lines falling further than this are flagged for review: often the back of a deck.
		double HighDrop = 150;
		// Leave out stair nosings: an edge whose landing drops again just past it, or that has another step
		// rising just behind it.
		bool bRejectStairs = true;
		// Edges falling between LowLedgeMinDrop and MinDrop onto a wide landing: curbs and manual pads.
		LowLedgeMode LowLedges = LowLedgeMode::Suggest;
		double LowLedgeMinDrop = 12;
		// Sharp convex ridges with no top, such as an A-frame or a roof ridge. Suggested for review.
		bool bDetectRidges = false;
		double MinRidgeAngleDegrees = 70;
		// Triangles wound against their neighbours are turned round, and double-sided copies dropped.
		bool bRepairWinding = true;
		// Lines whose confidence reaches this are suggested for keeping; the rest are listed unticked.
		double KeepConfidence = 0.5;
	};

	// Height of the first surface straight down from From, at most MaxDistance below it.
	using SurfaceBelow = std::function<std::optional<double>(const Vec3& From, double MaxDistance)>;

	// The first surface the straight segment From -> To passes through, met from either side: how far along
	// the segment (0 to 1) and which way the surface faces (its outward normal, not normalised).
	struct SegmentHit
	{
		double T = 0;
		Vec3 Facing;
	};
	// Unlike the drop test, this sees a wall beside an edge and a part the edge is buried in.
	using SegmentTest = std::function<std::optional<SegmentHit>(const Vec3& From, const Vec3& To)>;

	// Called before each mesh with how many are done; return false to stop the scan.
	using ProgressCallback = std::function<bool(size_t Done, size_t Total)>;

	// Why an edge was turned down.
	enum class Reject : uint8_t
	{
		TooSteep,
		InsideCorner,
		Covered,
		Buried,
		Walled,
		Seam,
		NoSurface,
		Obstructed,
		SmallFall,
		TooHigh,
		Stair,
		RoundedEdge,
		Unpaired,
		RampSide,
		Short,
		Count
	};
	const char* Describe(Reject Reason);
	// A short name for the settings that decide it, for grouping in the panel.
	const char* SettingFor(Reject Reason);

	// An edge of a top that was considered and turned down, for showing near misses.
	struct Rejection
	{
		Vec3 A;
		Vec3 B;
		Reject Reason = Reject::SmallFall;
		double Drop = 0;
		size_t MeshIndex = 0;
	};

	struct ScanOptions
	{
		std::vector<Rejection>* Rejections = nullptr;
		ProgressCallback Progress;
		// Set when Progress stopped the scan; the lines found so far are returned.
		bool* bCancelled = nullptr;
	};

	// FirstHit may be empty; then walls and buried edges are only caught where the drop test sees them.
	std::vector<Line> FindGrindLines(const std::vector<Mesh>& Meshes, const SurfaceBelow& Below, const SegmentTest& FirstHit, const Settings& Config, const ScanOptions& Options);
	std::vector<Line> FindGrindLines(const std::vector<Mesh>& Meshes, const SurfaceBelow& Below, const SegmentTest& FirstHit, const Settings& Config, std::vector<Rejection>* Rejections = nullptr);
	std::vector<Line> FindGrindLines(const std::vector<Mesh>& Meshes, const SurfaceBelow& Below, const Settings& Config, std::vector<Rejection>* Rejections = nullptr);

	// A ray that met a triangle: how far along it, where, the face's unit normal and which triangle.
	struct RayHit
	{
		double Distance = 0;
		Vec3 Point;
		Vec3 Normal;
		size_t MeshIndex = 0;
		uint32_t Triangle = 0; // index of the triangle's first corner in Mesh::Indices, divided by 3
	};

	// A SurfaceBelow, SegmentTest and ray test over triangles, for callers without a physics scene. Faces
	// are hit from either side. Probes straight down search a grid seen from above (with CellSize 0 its
	// cells are sized to the triangles, and triangles too big for it, such as a road, are kept aside);
	// segments and rays search a bounding volume hierarchy, so dense walls stay quick.
	class TriangleField
	{
	public:
		explicit TriangleField(const std::vector<Mesh>& Meshes, double CellSize = 0);
		std::optional<double> Below(const Vec3& From, double MaxDistance) const;
		std::optional<SegmentHit> FirstHit(const Vec3& From, const Vec3& To) const;
		// The nearest triangle along a ray (Direction need not be unit length).
		std::optional<RayHit> Raycast(const Vec3& Origin, const Vec3& Direction, double MaxDistance) const;
		size_t TriangleCount() const { return Triangles.size(); }

	private:
		struct Triangle
		{
			Vec3 A, B, C;
			size_t Mesh = 0;
			uint32_t Index = 0;
		};
		struct Node
		{
			Vec3 Min;
			Vec3 Max;
			uint32_t Start = 0;
			uint32_t Count = 0; // triangles in a leaf; 0 for a node with two children
			uint32_t Child = 0; // the first of its two children
		};
		static constexpr uint32_t LeafSize = 4;
		std::vector<Triangle> Triangles;
		// For segments and rays, a bounding volume hierarchy over every triangle.
		std::vector<Node> Nodes;
		std::vector<uint32_t> Order;
		// For probes straight down, a grid seen from above without the walls, which such a probe never meets.
		std::unordered_map<uint64_t, std::vector<uint32_t>> PlanCells;
		std::vector<uint32_t> PlanBig;
		double CellSize;
		uint64_t CellKey(double X, double Y) const;
		void BuildTree();
		// The first triangle the segment meets: how far along it (0 to 1) and which.
		std::optional<std::pair<double, uint32_t>> Nearest(const Vec3& From, const Vec3& To) const;
	};

	// ---------------------------------------------------------------------------------------------
	// Drawing by hand: snapping clicks to edges and following the edges between them.
	// ---------------------------------------------------------------------------------------------

	// The sharp convex edges of a set of meshes, welded across meshes, so a path can follow an edge from
	// one modular piece onto the next.
	class EdgeGraph
	{
	public:
		// An edge is sharp when the faces either side turn by at least FeatureAngleDegrees and it sticks out
		// (convex); open boundary edges count too. Vertices closer than WeldTolerance are one.
		EdgeGraph(const std::vector<Mesh>& Meshes, double FeatureAngleDegrees = 30, double WeldTolerance = 0.5);

		struct Snap
		{
			Vec3 Point;
			int64_t Edge = -1; // the sharp edge snapped to, or -1
			double T = 0; // where along it, 0 to 1
			double Distance = 0; // how far the snap moved the point
		};
		// The nearest point on a sharp edge within Radius of Point, if any.
		std::optional<Snap> Nearest(const Vec3& Point, double Radius) const;
		// The shortest way from one snap to another along sharp edges, if it is no longer than MaxDetour
		// times the straight distance. Empty when there is none.
		std::vector<Vec3> Path(const Snap& From, const Snap& To, double MaxDetour = 3) const;
		// The whole run of sharp edges through a snap, continued while it turns less than MaxCornerDegrees
		// at each vertex and does not fork.
		std::vector<Vec3> Chain(const Snap& At, double MaxCornerDegrees = 40) const;
		size_t EdgeCount() const { return Edges.size(); }
		// The two ends of a sharp edge, to show the edge a click snapped to.
		std::pair<Vec3, Vec3> EdgeEnds(int64_t Index) const;

	private:
		struct Edge
		{
			uint32_t A = 0;
			uint32_t B = 0;
		};
		std::vector<Vec3> Points;
		std::vector<Edge> Edges;
		std::vector<std::vector<uint32_t>> AtVertex;
		std::unordered_map<uint64_t, std::vector<uint32_t>> Cells;
		double CellSize = 50;
		uint64_t CellKey(int64_t X, int64_t Y, int64_t Z) const;
	};

	// Polyline helpers shared by the detector, the editor and the tests.
	double PolylineLength(const std::vector<Vec3>& Points);
	// Douglas-Peucker, keeping both ends; a closed line keeps at least three distinct points.
	std::vector<Vec3> SimplifyPolyline(const std::vector<Vec3>& Points, double Tolerance, bool bClosed = false);

	struct PolylinePoint
	{
		Vec3 Point;
		double Distance = 0; // from the query point
		double Along = 0; // distance along the polyline from its first point
	};
	// The point of a polyline nearest P.
	std::optional<PolylinePoint> NearestOnPolyline(const std::vector<Vec3>& Points, const Vec3& P);
	// The part of a polyline between two distances along it, in that order (From may exceed To to walk it
	// backwards). A closed polyline may wrap past its seam the shorter way round.
	std::vector<Vec3> SubPolyline(const std::vector<Vec3>& Points, double From, double To, bool bClosed = false);
}
