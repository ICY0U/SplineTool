#include "../Source/AutoGrind/Private/Core/AutoGrindCore.h"
#include <cmath>
#include <iostream>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>
using namespace AutoGrindCore;
static void Require(bool Value, const char* Message) { if (!Value) { std::cerr << "FAIL " << Message << std::endl; throw std::runtime_error(Message); } }
static Mesh Deck(double Width = 200) {
    return {"deck", {{0,0,100},{1000,0,100},{1000,Width,100},{0,Width,100}}, {0,1,2,0,2,3}};
}
// Sweeps a profile, anticlockwise in (Y, Z) seen from +X, along X from X0 to X1 and caps both ends.
static void Extrude(Mesh& M, const std::vector<std::pair<double,double>>& Profile, double X0, double X1) {
    const uint32_t Base = uint32_t(M.Vertices.size()), N = uint32_t(Profile.size());
    for (const auto& [Y, Z] : Profile) M.Vertices.push_back({X0, Y, Z});
    for (const auto& [Y, Z] : Profile) M.Vertices.push_back({X1, Y, Z});
    for (uint32_t I = 0; I < N; ++I) { const uint32_t J = (I + 1) % N; M.Indices.insert(M.Indices.end(), {Base+I, Base+J, Base+N+J, Base+I, Base+N+J, Base+N+I}); }
    for (uint32_t I = 1; I + 1 < N; ++I) { M.Indices.insert(M.Indices.end(), {Base+N, Base+N+I, Base+N+I+1, Base, Base+I+1, Base+I}); }
}
static void Box(Mesh& M, double X0, double Y0, double Z0, double X1, double Y1, double Z1) { Extrude(M, {{Y0,Z0},{Y1,Z0},{Y1,Z1},{Y0,Z1}}, X0, X1); }
// A tube's cross-section: 12 sides, the top one flat.
static std::vector<std::pair<double,double>> Tube(double Radius, double Height) {
    std::vector<std::pair<double,double>> Out;
    for (int K = 0; K < 12; ++K) { const double T = (15 + 30 * K) * 3.14159265358979 / 180; Out.push_back({-Radius * std::sin(T), Height + Radius * std::cos(T)}); }
    return Out;
}
static Mesh Ground() { return {"ground", {{-5000,-5000,0},{5000,-5000,0},{5000,5000,0},{-5000,5000,0}}, {0,1,2,0,2,3}}; }
// Scans Scan with the drop test and the wall test both seeing Scan, Context and the ground.
static std::vector<Line> ScanIn(const std::vector<Mesh>& Scan, std::vector<Mesh> Context, const Settings& S, std::vector<Rejection>* Rejected = nullptr) {
    Context.insert(Context.end(), Scan.begin(), Scan.end());
    Context.push_back(Ground());
    const TriangleField Field(Context);
    return FindGrindLines(Scan, [&](const Vec3& From, double Max) { return Field.Below(From, Max); },
        [&](const Vec3& From, const Vec3& To) { return Field.FirstHit(From, To); }, S, Rejected);
}
static double MinY(const Line& L) { double V = 1e9; for (const auto& P : L.Points) V = std::min(V, P.Y); return V; }
static double MaxY(const Line& L) { double V = -1e9; for (const auto& P : L.Points) V = std::max(V, P.Y); return V; }
int main() {
    Settings S;
    auto Ground = [](const Vec3&, double) -> std::optional<double> { return 0; };
    auto Lines = FindGrindLines({Deck()}, Ground, S);
    Require(Lines.size() == 4, "deck must have four edges");
    for (const auto& L : Lines) Require(L.Points.size() == 2, "straight lines should simplify to endpoints");
    std::cout << "PASS unobstructed deck and straight spline simplification\n";
    auto Obstacle = [](const Vec3& P, double) -> std::optional<double> {
        return P.Y < 0 && P.X > 110 && P.X < 160 ? 200 : 0;
    };
    std::vector<Rejection> Rejected;
    Lines = FindGrindLines({Deck()}, Obstacle, S, &Rejected);
    Require(Lines.size() == 5, "local obstruction should split only one edge");
    for (const auto& L : Lines) for (size_t I = 1; I < L.Points.size(); ++I) {
        const auto A = L.Points[I-1], B = L.Points[I];
        if (std::abs(A.Y) < 0.01 && std::abs(B.Y) < 0.01)
            Require(std::max(A.X,B.X) <= 110 || std::min(A.X,B.X) >= 160, "spline crosses obstacle");
    }
    Require(!Rejected.empty(), "obstruction needs a rejection explanation");
    std::cout << "PASS localized obstruction splits safe spans\n";
    Lines = FindGrindLines({Deck()}, [](const Vec3&, double)->std::optional<double>{ return 90; }, S);
    Require(Lines.empty(), "shallow stair drop must be rejected");
    Lines = FindGrindLines({Deck()}, [](const Vec3&, double)->std::optional<double>{ return {}; }, S);
    Require(Lines.empty(), "missing landing must be rejected");
    std::cout << "PASS stairs and missing ground rejected\n";
    Lines = FindGrindLines({Deck(10)}, Ground, S);
    Require(Lines.size() == 1 && Lines[0].Kind == LineKind::Rail, "narrow rail must collapse to one crest");
    Require(std::abs(Lines[0].Length - 1000) < 1, "rail must retain length");
    std::cout << "PASS narrow rail crest\n";
    Lines = FindGrindLines({Deck(10)}, Obstacle, S);
    for (const auto& L : Lines) if (L.Kind == LineKind::Rail) {
        double Low = L.Points.front().X, High = Low;
        for (const auto& P : L.Points) { Low = std::min(Low, P.X); High = std::max(High, P.X); }
        Require(High <= 110 || Low >= 160, "rail pairing restored a blocked span");
    }
    std::cout << "PASS rail pairing respects interrupted side\n";
    Mesh Ring; Ring.Name = "round deck"; Ring.Vertices.push_back({0,0,100});
    for (int I=0; I<32; ++I) { double A=I*6.283185307179586/32; Ring.Vertices.push_back({200*std::cos(A),200*std::sin(A),100}); }
    for (uint32_t I=0; I<32; ++I) Ring.Indices.insert(Ring.Indices.end(), {0,I+1,(I+1)%32+1});
    S.SimplifyTolerance = 10000;
    Lines = FindGrindLines({Ring}, Ground, S);
    Require(Lines.size() == 1 && Lines[0].bClosed && Lines[0].Points.size() >= 4, "closed spline collapsed");
    const auto A=Lines[0].Points.front(), B=Lines[0].Points.back();
    Require(std::abs(A.X-B.X)+std::abs(A.Y-B.Y)+std::abs(A.Z-B.Z)<0.01, "closed seam drifted");
    std::cout << "PASS closed path survives extreme simplification\n";
    Mesh Rim; Rim.Name = "curved rail";
    for (int I=0; I<32; ++I) {
        double Angle=I*6.283185307179586/32;
        for (double Radius : {200.0, 195.0}) Rim.Vertices.push_back({Radius*std::cos(Angle),Radius*std::sin(Angle),100});
    }
    for (uint32_t I=0; I<32; ++I) { uint32_t J=(I+1)%32; Rim.Indices.insert(Rim.Indices.end(), {2*I,2*J,2*J+1,2*I,2*J+1,2*I+1}); }
    S.SimplifyTolerance = 1;
    Lines = FindGrindLines({Rim}, Ground, S);
    Require(Lines.size() == 1 && Lines[0].Kind == LineKind::Rail && Lines[0].bClosed, "curved rail must be one closed crest");
    const auto C=Lines[0].Points.front(), D=Lines[0].Points.back();
    Require(std::abs(C.X-D.X)+std::abs(C.Y-D.Y)+std::abs(C.Z-D.Z)<0.001, "curved rail has an open seam");
    std::cout << "PASS curved rail seam\n";

    // A bench seat of five slats with 1.5 cm gaps: only the seat's two outer edges run its length.
    Settings Defaults;
    Mesh Seat; Seat.Name = "slatted seat";
    for (int K = 0; K < 5; ++K) Box(Seat, 0, K * 11.5, 40, 150, K * 11.5 + 10, 45);
    Lines = ScanIn({Seat}, {}, Defaults);
    int Long = 0;
    for (const auto& L : Lines) {
        if (L.Length < 100) continue;
        ++Long;
        Require(L.Kind == LineKind::Stone, "a seat edge is a ledge, not a rail");
        Require(MaxY(L) < 1 || MinY(L) > 55, "a line runs along the gap between two slats");
    }
    Require(Long == 2, "a slatted seat must have exactly its two outer long edges");
    std::cout << "PASS slat gaps are not lips\n";

    // A ledge with a 3 cm chamfer at exactly 45 degrees along one side: one line, at the chamfer's foot.
    Mesh Chamfered; Chamfered.Name = "chamfered ledge";
    Extrude(Chamfered, {{0,0},{40,0},{40,97},{37,100},{0,100}}, 0, 200);
    Lines = ScanIn({Chamfered}, {}, Defaults);
    Require(Lines.size() == 2, "a chamfered ledge must have one line per long side");
    bool bFoot = false;
    for (const auto& L : Lines) {
        Require(!(MinY(L) > 36 && MaxY(L) < 38), "a line runs along the top of the chamfer");
        bFoot |= MinY(L) > 39.5 && std::abs(L.Points.front().Z - 97) < 0.1;
    }
    Require(bFoot, "the chamfered side's line must be at the chamfer's foot");
    std::cout << "PASS a 45 degree chamfer is part of the top\n";

    // A tube rail with a bracket meeting it at crest height: one rail over the bracket.
    Mesh Railing; Railing.Name = "tube rail";
    Extrude(Railing, Tube(3, 100), 0, 400);
    Box(Railing, 196, 2.5, 90, 214, 20, 101.5);
    Lines = ScanIn({Railing}, {}, Defaults);
    Require(Lines.size() == 1 && Lines[0].Kind == LineKind::Rail, "a tube with a bracket must be one rail");
    Require(Lines[0].Length > 390, "the rail must run past the bracket");
    std::cout << "PASS rail joined across a bracket\n";

    // A ledge whose back edge is buried 30 cm deep in a wall 4 m tall and 2.6 m thick.
    Mesh Ledge; Ledge.Name = "ledge"; Box(Ledge, 0, 0, 0, 300, 70, 60);
    Mesh Wall; Wall.Name = "wall"; Box(Wall, -50, 40, 0, 350, 300, 400);
    Lines = ScanIn({Ledge}, {Wall}, Defaults);
    Long = 0;
    for (const auto& L : Lines) {
        Require(MaxY(L) < 40, "a line is inside the wall");
        Long += L.Length > 100;
    }
    Require(Long == 1, "the ledge's free front edge must stay a line");
    std::cout << "PASS an edge buried in a wall is not a line\n";

    // Two blocks of one wall with a 1 cm seam: each long side is one line.
    Mesh Blocks; Blocks.Name = "wall blocks";
    Box(Blocks, 0, 0, 0, 200, 40, 60); Box(Blocks, 201, 0, 0, 400, 40, 60);
    Lines = ScanIn({Blocks}, {}, Defaults);
    Require(Lines.size() == 2, "a seamed wall top must have one line per long side");
    for (const auto& L : Lines) Require(L.Length > 395, "a seam split a wall line");
    std::cout << "PASS lines joined across a block seam\n";

    // A ring of tube built from long facets: each facet's chord bows out past its narrow top side, so
    // the side a top lies on must come from winding, not from the third vertex.
    Mesh Hoop; Hoop.Name = "tube hoop";
    const auto Section = Tube(3, 100);
    constexpr int Segments = 24;
    for (int S2 = 0; S2 < Segments; ++S2) {
        const double Phi = S2 * 6.283185307179586 / Segments;
        for (const auto& [Y, Z] : Section) Hoop.Vertices.push_back({(300 - Y) * std::cos(Phi), (300 - Y) * std::sin(Phi), Z});
    }
    for (uint32_t S2 = 0; S2 < Segments; ++S2) {
        const uint32_t A0 = S2 * 12, B0 = ((S2 + 1) % Segments) * 12;
        for (uint32_t I = 0; I < 12; ++I) { const uint32_t J = (I + 1) % 12; Hoop.Indices.insert(Hoop.Indices.end(), {A0+I, A0+J, B0+J, A0+I, B0+J, B0+I}); }
    }
    Lines = ScanIn({Hoop}, {}, Defaults);
    Require(Lines.size() == 1 && Lines[0].Kind == LineKind::Rail && Lines[0].bClosed, "a hoop of tube must be one closed rail");
    Require(std::abs(Lines[0].Length - 6.283185307179586 * 300) < 40, "the hoop rail must run all the way round");
    std::cout << "PASS curved tube of long facets\n";

    // A stair handrail: one straight tube rising at 30 degrees, each facet a single 3 m sliver.
    Mesh Handrail; Handrail.Name = "handrail";
    for (double X : {0.0, 260.0}) for (const auto& [Y, Z] : Section) Handrail.Vertices.push_back({X, Y, Z + X * 150 / 260});
    for (uint32_t I = 0; I < 12; ++I) { const uint32_t J = (I + 1) % 12; Handrail.Indices.insert(Handrail.Indices.end(), {I, J, 12+J, I, 12+J, 12+I}); }
    Lines = ScanIn({Handrail}, {}, Defaults);
    Require(Lines.size() == 1 && Lines[0].Kind == LineKind::Rail, "a sloped handrail must be one rail");
    Require(std::abs(Lines[0].Length - 300) < 5, "the handrail's rail must run its whole length");
    std::cout << "PASS sloped handrail of long facets\n";

    // 1.0: modular pieces, classification, filtering and the scan options.
    auto Module = [](double X0, double X1) { Mesh M; M.Name = "module"; Box(M, X0, 0, 0, X1, 60, 50); return M; };
    Lines = ScanIn({Module(0, 150), Module(150, 300)}, {}, Defaults);
    Require(Lines.size() == 4, "two touching modules give four lines");
    int Joined = 0;
    for (const auto& L : Lines) Joined += L.MeshIndices.size() == 2 && std::abs(L.Length - 300) < 0.5 && (L.LineNotes & Notes::Joined);
    Require(Joined == 2, "the long sides of touching modules are one line each, across both meshes");
    Settings Apart = Defaults; Apart.bJoinAcrossMeshes = false;
    Require(ScanIn({Module(0, 150), Module(150, 300)}, {}, Apart).size() == 6, "with joining off each module keeps its own lines");
    std::cout << "PASS lines joined across meshes, and not when turned off\n";

    Mesh Pad; Pad.Name = "pad"; Box(Pad, 0, 0, 0, 300, 120, 18);
    Lines = ScanIn({Pad}, {}, Defaults);
    Require(Lines.size() == 4, "a low pad's edges are listed");
    for (const auto& L : Lines) Require(!L.bSuggested && (L.LineNotes & Notes::LowLedge) && L.Confidence < Defaults.KeepConfidence, "a low ledge is listed for review, not kept");
    Settings KeepLow = Defaults; KeepLow.LowLedges = LowLedgeMode::Keep;
    for (const auto& L : ScanIn({Pad}, {}, KeepLow)) Require(L.bSuggested, "Low Ledges Keep keeps them");
    Settings NoLow = Defaults; NoLow.LowLedges = LowLedgeMode::Off;
    Require(ScanIn({Pad}, {}, NoLow).empty(), "Low Ledges Off leaves them out");
    std::cout << "PASS low ledges: suggested, kept or off\n";

    Mesh Tall; Tall.Name = "tall"; Box(Tall, 0, 0, 0, 300, 100, 600);
    Settings Capped = Defaults; Capped.MaxDrop = 400;
    Rejected.clear();
    Require(ScanIn({Tall}, {}, Capped, &Rejected).empty(), "edges falling more than Max Drop are left out");
    bool bTooHigh = false;
    for (const auto& R : Rejected) bTooHigh |= R.Reason == Reject::TooHigh;
    Require(bTooHigh, "they are reported as too high");
    for (const auto& L : ScanIn({Tall}, {}, Defaults)) Require(L.LineNotes & Notes::HighDrop, "without a cap a 6 m fall is flagged as high");
    std::cout << "PASS Max Drop and High Drop\n";

    Mesh Flipped; Flipped.Name = "flipped"; Box(Flipped, 0, 0, 0, 300, 100, 45);
    for (size_t I = 0; I + 2 < Flipped.Indices.size(); I += 3) {
        const Vec3 &P0 = Flipped.Vertices[Flipped.Indices[I]], &P1 = Flipped.Vertices[Flipped.Indices[I + 1]], &P2 = Flipped.Vertices[Flipped.Indices[I + 2]];
        if ((P1.X - P0.X) * (P2.Y - P0.Y) - (P1.Y - P0.Y) * (P2.X - P0.X) > 0 && P0.Z > 44) { std::swap(Flipped.Indices[I + 1], Flipped.Indices[I + 2]); break; }
    }
    Require(ScanIn({Flipped}, {}, Defaults).size() == 4, "a top triangle wound the wrong way is turned round");
    Settings Raw = Defaults; Raw.bRepairWinding = false;
    Require(ScanIn({Flipped}, {}, Raw).size() < 4, "without repair the flipped triangle breaks the top");
    std::cout << "PASS winding repair\n";

    Mesh Wall14; Wall14.Name = "wall"; Box(Wall14, 0, -7, 0, 300, 7, 100);
    Lines = ScanIn({Wall14}, {}, Defaults);
    Require(Lines.size() == 1 && Lines[0].Kind == LineKind::Stone && Lines[0].Shape == LineShape::Crest && Lines[0].Thickness > Defaults.RailMaxThickness, "a narrow wall is one stone line on its crest");
    Settings AllRails = Defaults; AllRails.RailMaxThickness = 0;
    Lines = ScanIn({Wall14}, {}, AllRails);
    Require(Lines.size() == 1 && Lines[0].Kind == LineKind::Rail, "with Rail Max Thickness 0 every narrow top is a rail");
    std::cout << "PASS narrow walls are stone, thin bars rail\n";

    // Progress and cancelling.
    {
        std::vector<Mesh> Many;
        for (int K = 0; K < 10; ++K) { Mesh M; M.Name = "crate"; Box(M, K * 400.0, 0, 0, K * 400.0 + 120, 80, 60); Many.push_back(M); }
        std::vector<Mesh> Context = Many; Context.push_back(::Ground());
        const TriangleField Field(Context);
        ScanOptions Options;
        size_t Calls = 0;
        bool bCancelled = false;
        Options.bCancelled = &bCancelled;
        Options.Progress = [&](size_t Done, size_t Total) { Require(Total == 10 && Done == Calls, "progress counts meshes in order"); ++Calls; return Done < 4; };
        Lines = FindGrindLines(Many, [&](const Vec3& From, double Max) { return Field.Below(From, Max); }, [&](const Vec3& From, const Vec3& To) { return Field.FirstHit(From, To); }, Defaults, Options);
        Require(bCancelled && Calls == 5 && Lines.size() == 16, "cancelling after four meshes keeps their lines and stops");
    }
    std::cout << "PASS progress and cancel\n";

    for (int R = 0; R < int(Reject::Count); ++R) Require(std::string(Describe(Reject(R))).size() > 5 && std::string(SettingFor(Reject(R))).size() > 2, "every reason has a description and a setting");
    for (uint32_t N = 0; N < Notes::Count; ++N) Require(std::string(DescribeNote(1u << N)).size() > 3, "every note has a description");
    std::cout << "PASS reasons and notes are described\nAUTOGRIND_CORE_PASS\n";
}
