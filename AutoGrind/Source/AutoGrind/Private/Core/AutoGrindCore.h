// Finds grindable lines on triangle meshes: ledge lips, box edges, coping and rails.
//
// Engine-free on purpose: the editor module and the standalone scorer in Tests/ compile this same
// code, so the detector can be measured against the retail maps' hand-placed GrindActors in seconds.
// Units are centimetres with Z up, as in UE world space.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
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
		// renderer draws. A top is recognised by its normal, so flipped winding hides every line.
		std::vector<uint32_t> Indices;
	};

	enum class LineKind : uint8_t
	{
		Stone, // a ledge, box edge or coping: GrindActor's default GrindType
		Rail // a narrow top with a fall on both sides: GrindType 0
	};

	struct Line
	{
		LineKind Kind = LineKind::Stone;
		std::vector<Vec3> Points;
		bool bClosed = false;
		size_t MeshIndex = 0;
		double Length = 0;
		double Drop = 0; // median fall just past the edge
		double TopWidth = 0; // 2 x area / perimeter of the top the line runs along
	};

	struct Settings
	{
		// A face this flat or flatter is a top; only a top's outer edge can carry a line.
		double MaxTopSlopeDegrees = 45;
		// How far the surface must fall just past the edge. Stair nosings fall less than this.
		double MinDrop = 25;
		// How far past the edge the fall is measured.
		double ProbeDistance = 15;
		// An edge that falls onto a slope this steep is a lip even when it falls less than MinDrop,
		// as at the top of a bank or a quarter without coping. 0 turns this off.
		double SteepLipDegrees = 40;
		// Height above the edge that must be clear at the probe, so an edge against a wall is not a lip.
		double Clearance = 150;
		// With no surface this far below the probe, the edge overhangs nothing and is skipped.
		double MaxDropSearch = 5000;
		double MinLength = 50;
		double MaxLineSlopeDegrees = 45;
		// A sharper turn ends one line and starts the next, as at a box corner.
		double MaxCornerDegrees = 40;
		// A top narrower than this with a fall on both sides is a rail: one line along its crest.
		double RailMaxWidth = 15;
		// How far the surface must fall past the side of a top that narrow. A rail stands clear of what is
		// below it, such as the wall a railing runs along, by less than a ledge must.
		double RailMinDrop = 10;
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

	// An edge of a top that was considered and turned down, for showing near misses.
	struct Rejection
	{
		Vec3 A;
		Vec3 B;
		const char* Reason = "";
		double Drop = 0;
		size_t MeshIndex = 0;
	};

	// FirstHit may be empty; then walls and buried edges are only caught where the drop test sees them.
	std::vector<Line> FindGrindLines(const std::vector<Mesh>& Meshes, const SurfaceBelow& Below, const SegmentTest& FirstHit, const Settings& Config, std::vector<Rejection>* Rejections = nullptr);
	std::vector<Line> FindGrindLines(const std::vector<Mesh>& Meshes, const SurfaceBelow& Below, const Settings& Config, std::vector<Rejection>* Rejections = nullptr);

	// A SurfaceBelow and SegmentTest over triangles, for callers without a physics scene. Faces are hit
	// from either side.
	class TriangleField
	{
	public:
		explicit TriangleField(const std::vector<Mesh>& Meshes, double CellSize = 100);
		std::optional<double> Below(const Vec3& From, double MaxDistance) const;
		std::optional<SegmentHit> FirstHit(const Vec3& From, const Vec3& To) const;
		size_t TriangleCount() const { return Triangles.size(); }

	private:
		struct Triangle
		{
			Vec3 A, B, C;
		};
		std::vector<Triangle> Triangles;
		std::unordered_map<uint64_t, std::vector<uint32_t>> Cells;
		double CellSize;
		uint64_t CellKey(double X, double Y) const;
	};
}
