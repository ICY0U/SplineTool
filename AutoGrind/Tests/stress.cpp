// Timing on heavy inputs: a dense high-poly ledge, a park of hundreds of modular pieces, and a huge floor
// under small props. Prints how long each scan takes and what it found; fails if a scan finds the wrong
// number of lines or takes far longer than it should.
//
//   stress [time-limit-seconds]
#include "scenes.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace AutoGrindCore;

namespace
{
	// A box whose every face is a Steps x Steps grid of quads.
	Mesh DenseBox(double X0, double Y0, double Z0, double X1, double Y1, double Z1, int Steps)
	{
		Mesh M = Scenes::Named("dense box");
		auto Face = [&](const Vec3& Origin, const Vec3& U, const Vec3& V)
		{
			const uint32_t Base = uint32_t(M.Vertices.size());
			for (int J = 0; J <= Steps; ++J)
			{
				for (int I = 0; I <= Steps; ++I)
				{
					const double A = double(I) / Steps, B = double(J) / Steps;
					M.Vertices.push_back({Origin.X + U.X * A + V.X * B, Origin.Y + U.Y * A + V.Y * B, Origin.Z + U.Z * A + V.Z * B});
				}
			}
			const uint32_t Row = uint32_t(Steps + 1);
			for (uint32_t J = 0; J < uint32_t(Steps); ++J)
			{
				for (uint32_t I = 0; I < uint32_t(Steps); ++I)
				{
					const uint32_t P = Base + J * Row + I;
					Scenes::Quad(M, P, P + 1, P + Row + 1, P + Row);
				}
			}
		};
		const double W = X1 - X0, D = Y1 - Y0, H = Z1 - Z0;
		Face({X0, Y0, Z1}, {W, 0, 0}, {0, D, 0}); // top
		Face({X0, Y0, Z0}, {0, D, 0}, {W, 0, 0}); // bottom
		Face({X0, Y0, Z0}, {W, 0, 0}, {0, 0, H}); // front (-Y)
		Face({X0, Y1, Z0}, {0, 0, H}, {W, 0, 0}); // back (+Y)
		Face({X0, Y0, Z0}, {0, 0, H}, {0, D, 0}); // left (-X)
		Face({X1, Y0, Z0}, {0, D, 0}, {0, 0, H}); // right (+X)
		return M;
	}

	struct Result
	{
		size_t Lines = 0;
		double Seconds = 0;
	};

	Result Run(const std::vector<Mesh>& Scan, std::vector<Mesh> Context)
	{
		Context.insert(Context.end(), Scan.begin(), Scan.end());
		const auto Started = std::chrono::steady_clock::now();
		const TriangleField Field(Context);
		const Settings Config;
		const std::vector<Line> Lines = FindGrindLines(Scan, [&](const Vec3& From, double Max) { return Field.Below(From, Max); },
			[&](const Vec3& From, const Vec3& To) { return Field.FirstHit(From, To); }, Config);
		Result Out;
		Out.Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Started).count();
		for (const Line& L : Lines)
		{
			Out.Lines += L.bSuggested ? 1 : 0;
		}
		return Out;
	}
}

int main(int ArgCount, char** Args)
{
	const double Limit = ArgCount > 1 ? std::atof(Args[1]) : 20.0;
	bool bPass = true;
	auto Report = [&](const char* Name, const Result& R, size_t Expected, size_t Triangles)
	{
		const bool bOk = R.Lines == Expected && R.Seconds <= Limit;
		bPass &= bOk;
		std::printf("%s %-34s %8zu triangles  %3zu kept lines (want %zu)  %7.2f s\n", bOk ? "PASS" : "FAIL", Name, Triangles, R.Lines, Expected, R.Seconds);
	};

	{
		const Mesh Dense = DenseBox(0, 0, 0, 300, 100, 50, 200);
		Report("dense high-poly ledge", Run({Dense}, {Scenes::Ground()}), 4, Dense.Indices.size() / 3);
	}
	{
		// 25 rows of 8 modules each 150 cm long, rows 3 m apart: 25 x 4 lines.
		std::vector<Mesh> Park;
		size_t Triangles = 0;
		for (int Row = 0; Row < 25; ++Row)
		{
			for (int K = 0; K < 8; ++K)
			{
				Mesh M = Scenes::Named("module");
				Scenes::Box(M, K * 150.0, Row * 300.0, 0, K * 150.0 + 150, Row * 300.0 + 60, 50);
				Triangles += M.Indices.size() / 3;
				Park.push_back(M);
			}
		}
		Report("200 modular ledge pieces", Run(Park, {Scenes::Ground(0, 20000)}), 100, Triangles);
	}
	{
		// A 1 km floor in two triangles under 100 small boxes, each with its four top edges.
		Mesh Floor = Scenes::Ground(0, 50000);
		std::vector<Mesh> Props;
		size_t Triangles = 2;
		for (int K = 0; K < 100; ++K)
		{
			Mesh M = Scenes::Named("crate");
			const double X = (K % 10) * 500.0, Y = (K / 10) * 500.0;
			Scenes::Box(M, X, Y, 0, X + 120, Y + 80, 60);
			Triangles += M.Indices.size() / 3;
			Props.push_back(M);
		}
		Report("huge floor under 100 crates", Run(Props, {Floor}), 400, Triangles);
	}
	std::printf(bPass ? "AUTOGRIND_STRESS_PASS\n" : "AUTOGRIND_STRESS_FAIL\n");
	return bPass ? 0 : 1;
}
