// Checks for drawing lines by hand: ray picks, snapping to sharp edges, following edges between clicks
// across modular pieces, picking an edge's whole run, and the polyline helpers the editor uses.
#include "scenes.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace AutoGrindCore;

namespace
{
	int Failures = 0;

	void Check(bool bValue, const char* What)
	{
		if (!bValue)
		{
			std::printf("FAIL %s\n", What);
			++Failures;
		}
	}

	bool Near(double A, double B, double Tolerance) { return std::abs(A - B) <= Tolerance; }
	bool Near(const Vec3& A, const Vec3& B, double Tolerance) { return Near(A.X, B.X, Tolerance) && Near(A.Y, B.Y, Tolerance) && Near(A.Z, B.Z, Tolerance); }

	std::vector<Mesh> Modules(double Step, double Length, int Count)
	{
		std::vector<Mesh> Out;
		for (int I = 0; I < Count; ++I)
		{
			Mesh M = Scenes::Named("module");
			Scenes::Box(M, I * Step, 0, 0, I * Step + Length, 60, 50);
			Out.push_back(M);
		}
		return Out;
	}

	// From a click on the front edge of the first module to one on the last: one straight run along the front.
	void CheckPathAlongFront(const std::vector<Mesh>& Pieces, double EndX, const char* What)
	{
		const EdgeGraph Graph(Pieces);
		const std::optional<EdgeGraph::Snap> From = Graph.Nearest({20, 1, 51}, 5);
		const std::optional<EdgeGraph::Snap> To = Graph.Nearest({EndX, 1, 51}, 5);
		Check(From && To, What);
		if (!From || !To)
		{
			return;
		}
		const std::vector<Vec3> Path = Graph.Path(*From, *To);
		Check(!Path.empty(), What);
		if (Path.empty())
		{
			return;
		}
		Check(Near(PolylineLength(Path), EndX - 20, 3), What);
		for (const Vec3& P : Path)
		{
			Check(Near(P.Y, 0, 2.1) && Near(P.Z, 50, 0.6), What);
		}
	}
}

int main()
{
	// Rays.
	{
		Mesh Box = Scenes::Named("box");
		Scenes::Box(Box, 0, 0, 0, 300, 100, 45);
		const TriangleField Field({Box});
		const std::optional<RayHit> Down = Field.Raycast({50, 50, 500}, {0, 0, -1}, 1000);
		Check(Down && Near(Down->Point, {50, 50, 45}, 1e-6) && Near(Down->Normal, {0, 0, 1}, 1e-6) && Near(Down->Distance, 455, 1e-6), "a ray down meets the box top");
		const std::optional<RayHit> Side = Field.Raycast({-100, 50, 20}, {1, 0, 0}, 1000);
		Check(Side && Near(Side->Point, {0, 50, 20}, 1e-6) && Near(Side->Normal, {-1, 0, 0}, 1e-6), "a ray from the side meets the box end");
		Check(!Field.Raycast({50, 50, 500}, {0, 0, 1}, 1000), "a ray away from the box meets nothing");
		Check(!Field.Raycast({50, 50, 500}, {0, 0, -1}, 100), "a ray too short to reach the box meets nothing");
		const std::optional<RayHit> Slant = Field.Raycast({-200, -200, 300}, {1, 1, -1}, 2000);
		Check(Slant && Near(Slant->Point.Z, 45, 1e-6) && Slant->Point.X > 0 && Slant->Point.Y > 0, "a slanting ray meets the top");
	}
	std::printf("PASS rays\n");

	// One box: twelve sharp edges, a snap onto the front edge and its whole run.
	{
		Mesh Box = Scenes::Named("box");
		Scenes::Box(Box, 0, 0, 0, 300, 100, 45);
		const EdgeGraph Graph({Box});
		Check(Graph.EdgeCount() >= 12, "a box has its twelve sharp edges");
		const std::optional<EdgeGraph::Snap> Snapped = Graph.Nearest({120, 3, 47}, 10);
		Check(Snapped && Near(Snapped->Point, {120, 0, 45}, 1e-6), "a click near the front top edge snaps onto it");
		Check(!Graph.Nearest({150, 50, 200}, 10), "a click far from every edge does not snap");
		if (Snapped)
		{
			const std::vector<Vec3> Run = Graph.Chain(*Snapped);
			Check(Run.size() == 2 && Near(PolylineLength(Run), 300, 1e-6), "picking the front edge takes its whole run and stops at the corners");
			const std::optional<EdgeGraph::Snap> Corner = Graph.Nearest({298, 50, 46}, 5);
			Check(Corner.has_value(), "a click on the right end edge snaps");
			if (Corner)
			{
				const std::vector<Vec3> Path = Graph.Path(*Snapped, *Corner);
				Check(Path.size() == 3 && Near(Path[1], {300, 0, 45}, 1e-6) && Near(PolylineLength(Path), 180 + 50, 1e-3), "a path round a corner goes through it");
			}
		}
	}
	std::printf("PASS snapping and picking on a box\n");

	// Modular pieces: touching, with 1 cm gaps, pushed 4 cm into one another.
	CheckPathAlongFront(Modules(150, 150, 3), 430, "a path runs along the front of touching modules");
	CheckPathAlongFront(Modules(151, 150, 3), 432, "a path bridges 1 cm gaps between modules");
	CheckPathAlongFront(Modules(146, 150, 3), 422, "a path runs on through modules pushed into one another");
	{
		std::vector<Mesh> Apart = Modules(200, 150, 2);
		const EdgeGraph Graph(Apart);
		const std::optional<EdgeGraph::Snap> From = Graph.Nearest({20, 1, 51}, 5);
		const std::optional<EdgeGraph::Snap> To = Graph.Nearest({320, 1, 51}, 5);
		Check(From && To && Graph.Path(*From, *To).empty(), "no path jumps a 50 cm gap between pieces");
	}
	std::printf("PASS paths across modular pieces\n");

	// A round planter's rim: one closed run all the way round.
	{
		Mesh Planter = Scenes::Named("round planter");
		Scenes::Revolve(Planter, {{0, 0}, {100, 0}, {100, 50}, {0, 50}}, 48);
		const EdgeGraph Graph({Planter});
		const std::optional<EdgeGraph::Snap> Rim = Graph.Nearest({101, 0, 51}, 5);
		Check(Rim.has_value(), "a click on the rim snaps");
		if (Rim)
		{
			const std::vector<Vec3> Ring = Graph.Chain(*Rim);
			Check(Ring.size() == 49 && Near(Ring.front(), Ring.back(), 1e-9), "the rim is one closed run");
			Check(Near(PolylineLength(Ring), 2 * 3.14159265 * 100, 3), "the rim run goes all the way round");
			const std::optional<EdgeGraph::Snap> Across = Graph.Nearest({-101, 0, 51}, 5);
			if (Across)
			{
				const std::vector<Vec3> Half = Graph.Path(*Rim, *Across);
				Check(Near(PolylineLength(Half), 3.14159265 * 100, 3), "a path across the rim follows it half way round");
			}
		}
	}
	std::printf("PASS closed runs\n");

	// Polyline helpers.
	{
		const std::vector<Vec3> Line{{0, 0, 0}, {100, 0, 0}, {100, 100, 0}};
		const std::optional<PolylinePoint> On = NearestOnPolyline(Line, {120, 30, 0});
		Check(On && Near(On->Point, {100, 30, 0}, 1e-9) && Near(On->Along, 130, 1e-9) && Near(On->Distance, 20, 1e-9), "nearest point on a polyline");
		const std::vector<Vec3> Part = SubPolyline(Line, 50, 150);
		Check(Part.size() == 3 && Near(Part.front(), {50, 0, 0}, 1e-9) && Near(Part[1], {100, 0, 0}, 1e-9) && Near(Part.back(), {100, 50, 0}, 1e-9), "a part of a polyline keeps its corner");
		const std::vector<Vec3> Back = SubPolyline(Line, 150, 50);
		Check(Back.size() == 3 && Near(Back.front(), {100, 50, 0}, 1e-9) && Near(Back.back(), {50, 0, 0}, 1e-9), "a part walked backwards");
		const std::vector<Vec3> Square{{0, 0, 0}, {100, 0, 0}, {100, 100, 0}, {0, 100, 0}, {0, 0, 0}};
		const std::vector<Vec3> Wrap = SubPolyline(Square, 350, 50, true);
		Check(Near(PolylineLength(Wrap), 100, 1e-9) && Near(Wrap.front(), {0, 50, 0}, 1e-9) && Near(Wrap.back(), {50, 0, 0}, 1e-9), "a part of a loop wraps past its seam the short way");
		const std::vector<Vec3> Simple = SimplifyPolyline(Square, 1e6, true);
		Check(Simple.size() >= 4 && Near(Simple.front(), Simple.back(), 1e-9), "a simplified loop keeps three distinct points");
	}
	std::printf("PASS polyline helpers\n");

	if (Failures > 0)
	{
		std::printf("%d check(s) failed\n", Failures);
		return 1;
	}
	std::printf("AUTOGRIND_DRAWING_PASS\n");
	return 0;
}
