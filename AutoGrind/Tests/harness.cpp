// Runs the AutoGrind detector over a map's extracted geometry (an .agt file from tools/geometry.py)
// and writes the lines it finds as JSON, for score.py to compare with the map's own GrindActors.
//
//   harness <map.agt> <out.json> <scan-name-part>... [-- <context-exclude-part>...]
//
// Objects whose names contain a scan part are searched for lines. Every object not excluded is
// ground the drop test can land on, so a ledge's fall is measured to the real floor. Excluded
// objects (scenery, gameplay markers) are neither searched nor landed on.
#include "../Source/AutoGrind/Private/Core/AutoGrindCore.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace AutoGrindCore;

static bool ReadAgt(const char* Path, std::vector<Mesh>& Out)
{
	std::ifstream File(Path, std::ios::binary);
	char Magic[4];
	uint32_t Count = 0;
	if (!File.read(Magic, 4) || std::memcmp(Magic, "AGT1", 4) != 0 || !File.read(reinterpret_cast<char*>(&Count), 4))
	{
		return false;
	}
	for (uint32_t I = 0; I < Count; ++I)
	{
		Mesh M;
		uint32_t NameBytes = 0, VertexCount = 0, TriangleCount = 0;
		File.read(reinterpret_cast<char*>(&NameBytes), 4);
		M.Name.resize(NameBytes);
		File.read(M.Name.data(), NameBytes);
		File.read(reinterpret_cast<char*>(&VertexCount), 4);
		std::vector<float> Raw(size_t(VertexCount) * 3);
		File.read(reinterpret_cast<char*>(Raw.data()), Raw.size() * 4);
		for (uint32_t V = 0; V < VertexCount; ++V)
		{
			M.Vertices.push_back({Raw[V * 3], Raw[V * 3 + 1], Raw[V * 3 + 2]});
		}
		File.read(reinterpret_cast<char*>(&TriangleCount), 4);
		M.Indices.resize(size_t(TriangleCount) * 3);
		File.read(reinterpret_cast<char*>(M.Indices.data()), M.Indices.size() * 4);
		if (!File)
		{
			return false;
		}
		Out.push_back(std::move(M));
	}
	return true;
}

static bool Contains(const std::string& Name, const std::vector<std::string>& Parts)
{
	for (const std::string& P : Parts)
	{
		if (Name.find(P) != std::string::npos)
		{
			return true;
		}
	}
	return false;
}

int main(int ArgCount, char** Args)
{
	if (ArgCount < 4)
	{
		std::cerr << "usage: harness <map.agt> <out.json> <scan-part>... [-- <exclude-part>...]\n";
		return 2;
	}
	std::vector<std::string> ScanParts, ExcludeParts;
	bool bExcluding = false;
	for (int I = 3; I < ArgCount; ++I)
	{
		if (std::strcmp(Args[I], "--") == 0)
		{
			bExcluding = true;
		}
		else
		{
			(bExcluding ? ExcludeParts : ScanParts).push_back(Args[I]);
		}
	}

	std::vector<Mesh> All;
	if (!ReadAgt(Args[1], All))
	{
		std::cerr << "cannot read " << Args[1] << "\n";
		return 1;
	}
	std::vector<Mesh> Scan, Context;
	for (const Mesh& M : All)
	{
		if (Contains(M.Name, ExcludeParts))
		{
			continue;
		}
		Context.push_back(M);
		if (Contains(M.Name, ScanParts))
		{
			Scan.push_back(M);
		}
	}

	const auto Started = std::chrono::steady_clock::now();
	const TriangleField Field(Context);
	const Settings Config;
	std::vector<Rejection> Rejected;
	const std::vector<Line> Lines = FindGrindLines(Scan, [&](const Vec3& From, double Max) { return Field.Below(From, Max); },
		[&](const Vec3& From, const Vec3& To) { return Field.FirstHit(From, To); }, Config, &Rejected);
	const double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Started).count();

	FILE* Out = std::fopen(Args[2], "w");
	if (!Out)
	{
		return 1;
	}
	std::fprintf(Out, "[\n");
	for (size_t I = 0; I < Lines.size(); ++I)
	{
		const Line& L = Lines[I];
		std::fprintf(Out, " {\"mesh\": \"%s\", \"kind\": \"%s\", \"closed\": %s, \"length\": %.1f, \"drop\": %.1f, \"width\": %.1f, \"points\": [",
			Scan[L.MeshIndex].Name.c_str(), L.Kind == LineKind::Rail ? "rail" : "stone", L.bClosed ? "true" : "false", L.Length, L.Drop, L.TopWidth);
		for (size_t P = 0; P < L.Points.size(); ++P)
		{
			std::fprintf(Out, "%s[%.2f, %.2f, %.2f]", P ? ", " : "", L.Points[P].X, L.Points[P].Y, L.Points[P].Z);
		}
		std::fprintf(Out, "]}%s\n", I + 1 < Lines.size() ? "," : "");
	}
	std::fprintf(Out, "]\n");
	std::fclose(Out);

	// Near misses beside the result: <out>.rejected.json
	FILE* Misses = std::fopen((std::string(Args[2]) + ".rejected.json").c_str(), "w");
	if (!Misses)
	{
		return 1;
	}
	std::fprintf(Misses, "[\n");
	for (size_t I = 0; I < Rejected.size(); ++I)
	{
		const Rejection& R = Rejected[I];
		std::fprintf(Misses, " {\"mesh\": \"%s\", \"reason\": \"%s\", \"drop\": %.1f, \"a\": [%.2f, %.2f, %.2f], \"b\": [%.2f, %.2f, %.2f]}%s\n",
			Scan[R.MeshIndex].Name.c_str(), R.Reason, R.Drop < -1e9 ? -1e9 : R.Drop, R.A.X, R.A.Y, R.A.Z, R.B.X, R.B.Y, R.B.Z, I + 1 < Rejected.size() ? "," : "");
	}
	std::fprintf(Misses, "]\n");
	std::fclose(Misses);
	std::printf("scanned %zu meshes against %zu context meshes: %zu lines in %.2f s\n", Scan.size(), Context.size(), Lines.size(), Seconds);
	return 0;
}
