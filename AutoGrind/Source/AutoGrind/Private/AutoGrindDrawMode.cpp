#include "AutoGrindDrawMode.h"

#include "AutoGrindGenerate.h"
#include "AutoGrindSettings.h"
#include "CanvasTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "TimerManager.h"
#include "UnrealClient.h"

#define LOCTEXT_NAMESPACE "AutoGrindDraw"

namespace AutoGrindDraw
{
	// How far a click may be from a line or edge to snap to it: this share of its distance from the camera,
	// within these bounds in centimetres.
	constexpr double SnapShare = 0.015;
	constexpr double MinSnap = 2;
	constexpr double MaxSnap = 40;
	// How far rays reach, and how far back an orthographic view's rays start.
	constexpr double MaxPick = 2000000;
	constexpr double OrthoBack = 1000000;
	// Components within this of the one under the cursor share its edge graph, so edges can be followed
	// from one modular piece onto the next.
	constexpr double NeighbourReach = 30;
	constexpr int32 MaxNeighbours = 24;
	// Edge graphs are not built over more triangles than this: a whole-level mesh would take too long.
	constexpr int64 MaxGraphTriangles = 300000;
	// Picking keeps the triangles of this many components at most.
	constexpr int32 MaxTargets = 256;
	constexpr double CandidateRefreshSeconds = 2;
	// A path may wind this many times as far as the straight way between two clicks.
	constexpr double MaxDetour = 4;
	constexpr double MessageSeconds = 5;

	AutoGrindCore::Vec3 ToCore(const FVector& V) { return {V.X, V.Y, V.Z}; }
	FVector FromCore(const AutoGrindCore::Vec3& V) { return FVector(V.X, V.Y, V.Z); }

	TArray<FVector> FromCore(const std::vector<AutoGrindCore::Vec3>& Points)
	{
		TArray<FVector> Out;
		Out.Reserve(int32(Points.size()));
		for (const AutoGrindCore::Vec3& P : Points)
		{
			Out.Add(FromCore(P));
		}
		return Out;
	}

	// Where a ray enters a box, if it does within MaxDistance.
	bool RayEntersBox(const FBox& Box, const FVector& Origin, const FVector& Ray, double MaxDistance, double& OutEntry)
	{
		double Near = 0;
		double Far = MaxDistance;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const double Start = Origin[Axis];
			const double Step = Ray[Axis];
			if (FMath::Abs(Step) < 1e-12)
			{
				if (Start < Box.Min[Axis] || Start > Box.Max[Axis])
				{
					return false;
				}
				continue;
			}
			double T0 = (Box.Min[Axis] - Start) / Step;
			double T1 = (Box.Max[Axis] - Start) / Step;
			if (T0 > T1)
			{
				Swap(T0, T1);
			}
			Near = FMath::Max(Near, T0);
			Far = FMath::Min(Far, T1);
			if (Near > Far)
			{
				return false;
			}
		}
		OutEntry = Near;
		return true;
	}

	FText KindName(EAutoGrindDrawKind Kind)
	{
		switch (Kind)
		{
		case EAutoGrindDrawKind::Rail: return LOCTEXT("KindRail", "Rail");
		case EAutoGrindDrawKind::Stone: return LOCTEXT("KindStone", "Stone");
		case EAutoGrindDrawKind::Auto:
		default: return LOCTEXT("KindAuto", "Auto");
		}
	}
}

FAutoGrindDrawShared& FAutoGrindDrawShared::Get()
{
	static FAutoGrindDrawShared Shared;
	return Shared;
}

void FAutoGrindDrawShared::SetSnapLines(const TArray<TSharedPtr<FAutoGrindLine>>& Lines)
{
	SnapLines = Lines;
	++SnapVersion;
}

const FEditorModeID FAutoGrindDrawMode::ModeId(TEXT("EM_AutoGrindDraw"));

FAutoGrindDrawMode::FAutoGrindDrawMode()
{
}

FAutoGrindDrawMode::~FAutoGrindDrawMode()
{
}

bool FAutoGrindDrawMode::IsActive()
{
	return GLevelEditorModeTools().IsModeActive(ModeId);
}

void FAutoGrindDrawMode::SetActive(bool bActive)
{
	if (bActive == IsActive())
	{
		return;
	}
	if (bActive)
	{
		GLevelEditorModeTools().ActivateMode(ModeId);
	}
	else
	{
		GLevelEditorModeTools().DeactivateMode(ModeId);
	}
}

FString FAutoGrindDrawMode::GetReferencerName() const
{
	return TEXT("FAutoGrindDrawMode");
}

void FAutoGrindDrawMode::Enter()
{
	FEdMode::Enter();
	Reset();
	Library = AutoGrind::FMeshLibrary();
	Say(LOCTEXT("Entered", "Click points on the level to draw a grind line."));
	FAutoGrindDrawShared::Get().OnModeChanged.Broadcast();
}

void FAutoGrindDrawMode::Exit()
{
	Reset();
	Targets.Empty();
	Candidates.Empty();
	SnapLines.Empty();
	SnapVersion = MAX_uint32;
	Library = AutoGrind::FMeshLibrary();
	FEdMode::Exit();
	FAutoGrindDrawShared::Get().OnModeChanged.Broadcast();
}

void FAutoGrindDrawMode::Reset()
{
	ClearPoints();
	Hover.Reset();
	HoverComponent.Reset();
	CandidatesTime = -1;
}

void FAutoGrindDrawMode::AddPoint(const FDrawPoint& Point, const TArray<FVector>* KnownWay)
{
	RefreshSegments();
	if (Points.Num() > 0)
	{
		Segments.Add(KnownWay && KnownWay->Num() >= 2 ? *KnownWay : Segment(Points.Last(), Point));
	}
	Points.Add(Point);
	HoverSegment.Reset();
}

void FAutoGrindDrawMode::RemoveLastPoint()
{
	if (Points.Num() > 0)
	{
		Points.Pop();
	}
	if (Segments.Num() > FMath::Max(0, Points.Num() - 1))
	{
		Segments.Pop();
	}
	HoverSegment.Reset();
}

void FAutoGrindDrawMode::ClearPoints()
{
	Points.Reset();
	Segments.Reset();
	HoverSegment.Reset();
}

void FAutoGrindDrawMode::RefreshSegments()
{
	const bool bFollow = GetDefault<UAutoGrindSettings>()->bDrawFollowEdges;
	if (bFollow == bSegmentsFollow)
	{
		return;
	}
	bSegmentsFollow = bFollow;
	Segments.Reset();
	for (int32 I = 0; I + 1 < Points.Num(); ++I)
	{
		Segments.Add(Segment(Points[I], Points[I + 1]));
	}
	HoverSegment = Hover && Points.Num() > 0 ? Segment(Points.Last(), *Hover) : TArray<FVector>();
}

bool FAutoGrindDrawMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	return false;
}

bool FAutoGrindDrawMode::UsesToolkits() const
{
	return false;
}

bool FAutoGrindDrawMode::ShouldDrawWidget() const
{
	return false;
}

bool FAutoGrindDrawMode::GetCursor(EMouseCursor::Type& OutCursor) const
{
	OutCursor = EMouseCursor::Crosshairs;
	return true;
}

UWorld* FAutoGrindDrawMode::GetEditorWorld() const
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

void FAutoGrindDrawMode::Say(const FText& Message)
{
	LastMessage = Message;
	LastMessageTime = FPlatformTime::Seconds();
}

void FAutoGrindDrawMode::RefreshCandidates(UWorld& World)
{
	const double Now = FPlatformTime::Seconds();
	if (CandidatesTime >= 0 && Now - CandidatesTime < AutoGrindDraw::CandidateRefreshSeconds)
	{
		return;
	}
	CandidatesTime = Now;
	Candidates.Reset();
	const UAutoGrindSettings& Settings = *GetDefault<UAutoGrindSettings>();
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		const AActor* Actor = *It;
		if (!Actor || !AutoGrind::IsScannable(*Actor, Settings) || Actor->IsHiddenEd())
		{
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
		for (const UStaticMeshComponent* Component : Components)
		{
			if (Component && Component->GetStaticMesh() && Component->IsVisibleInEditor())
			{
				Candidates.Add({Component, Component->Bounds.GetBox()});
			}
		}
	}
}

TSharedPtr<FAutoGrindDrawMode::FTarget> FAutoGrindDrawMode::GetTarget(const UStaticMeshComponent& Component)
{
	if (const TSharedPtr<FTarget>* Found = Targets.Find(&Component))
	{
		// A moved component is read again.
		if ((*Found)->Transform.Equals(Component.GetComponentTransform()))
		{
			return *Found;
		}
		Targets.Remove(&Component);
	}
	if (Targets.Num() >= AutoGrindDraw::MaxTargets)
	{
		Targets.Empty();
	}
	TSharedPtr<FTarget> Target = MakeShared<FTarget>();
	Target->Component = &Component;
	Target->Transform = Component.GetComponentTransform();
	Target->Bounds = Component.Bounds.GetBox();
	std::vector<AutoGrindCore::Mesh> Meshes;
	if (Library.Place(Component, Meshes))
	{
		Target->Field = std::make_shared<AutoGrindCore::TriangleField>(Meshes);
	}
	Targets.Add(&Component, Target);
	return Target;
}

std::shared_ptr<AutoGrindCore::EdgeGraph> FAutoGrindDrawMode::GetEdges(FTarget& Target)
{
	if (Target.bEdgesBuilt)
	{
		return Target.Edges;
	}
	Target.bEdgesBuilt = true;
	const UStaticMeshComponent* Centre = Target.Component.Get();
	if (!Centre)
	{
		return nullptr;
	}
	std::vector<AutoGrindCore::Mesh> Meshes;
	if (!Library.Place(*Centre, Meshes))
	{
		return nullptr;
	}
	int64 Triangles = 0;
	for (const AutoGrindCore::Mesh& Mesh : Meshes)
	{
		Triangles += int64(Mesh.Indices.size() / 3);
	}
	if (Triangles > AutoGrindDraw::MaxGraphTriangles)
	{
		return nullptr;
	}
	// The pieces around it, so an edge can be followed from one modular piece onto the next.
	const FBox Reach = Target.Bounds.ExpandBy(AutoGrindDraw::NeighbourReach);
	int32 Neighbours = 0;
	std::vector<AutoGrindCore::Mesh> Around;
	for (const FCandidate& Candidate : Candidates)
	{
		const UStaticMeshComponent* Other = Candidate.Component.Get();
		if (!Other || Other == Centre || !Candidate.Bounds.Intersect(Reach) || Neighbours >= AutoGrindDraw::MaxNeighbours)
		{
			continue;
		}
		Around.clear();
		if (!Library.Place(*Other, Around))
		{
			continue;
		}
		int64 More = 0;
		for (const AutoGrindCore::Mesh& Mesh : Around)
		{
			More += int64(Mesh.Indices.size() / 3);
		}
		if (Triangles + More > AutoGrindDraw::MaxGraphTriangles)
		{
			continue;
		}
		Triangles += More;
		++Neighbours;
		for (AutoGrindCore::Mesh& Mesh : Around)
		{
			Meshes.push_back(MoveTemp(Mesh));
		}
	}
	Target.Edges = std::make_shared<AutoGrindCore::EdgeGraph>(Meshes);
	return Target.Edges;
}

void FAutoGrindDrawMode::RefreshSnapLines()
{
	const FAutoGrindDrawShared& Shared = FAutoGrindDrawShared::Get();
	if (SnapVersion == Shared.GetSnapVersion())
	{
		return;
	}
	SnapVersion = Shared.GetSnapVersion();
	SnapLines.Reset();
	for (const TSharedPtr<FAutoGrindLine>& Line : Shared.GetSnapLines())
	{
		if (!Line || Line->Points.Num() < 2)
		{
			continue;
		}
		FSnapLine& Snap = SnapLines.AddDefaulted_GetRef();
		Snap.Line = Line;
		Snap.Bounds = FBox(Line->Points);
		for (const FVector& P : Line->Points)
		{
			Snap.Points.push_back(AutoGrindDraw::ToCore(P));
		}
	}
}

bool FAutoGrindDrawMode::Pick(UWorld& World, const FVector& Origin, const FVector& Direction, FVector& OutLocation, double& OutDistance, TSharedPtr<FTarget>& OutTarget)
{
	RefreshCandidates(World);
	const FVector Ray = Direction.GetSafeNormal();
	if (Ray.IsNearlyZero())
	{
		return false;
	}
	struct FEntry
	{
		double Distance = 0;
		int32 Index = 0;
	};
	TArray<FEntry> Entries;
	for (int32 I = 0; I < Candidates.Num(); ++I)
	{
		double Entry = 0;
		if (AutoGrindDraw::RayEntersBox(Candidates[I].Bounds, Origin, Ray, AutoGrindDraw::MaxPick, Entry))
		{
			Entries.Add({Entry, I});
		}
	}
	Entries.Sort([](const FEntry& L, const FEntry& R) { return L.Distance < R.Distance; });
	bool bFound = false;
	for (const FEntry& Entry : Entries)
	{
		// Boxes are in order of where the ray enters them: one beyond the nearest hit cannot be nearer.
		if (bFound && Entry.Distance > OutDistance)
		{
			break;
		}
		const UStaticMeshComponent* Component = Candidates[Entry.Index].Component.Get();
		if (!Component)
		{
			continue;
		}
		const TSharedPtr<FTarget> Target = GetTarget(*Component);
		if (!Target || !Target->Field)
		{
			continue;
		}
		const std::optional<AutoGrindCore::RayHit> Hit = Target->Field->Raycast(AutoGrindDraw::ToCore(Origin), AutoGrindDraw::ToCore(Ray), AutoGrindDraw::MaxPick);
		if (Hit && (!bFound || Hit->Distance < OutDistance))
		{
			bFound = true;
			OutDistance = Hit->Distance;
			OutLocation = AutoGrindDraw::FromCore(Hit->Point);
			OutTarget = Target;
		}
	}
	return bFound;
}

void FAutoGrindDrawMode::UpdateHover(const FVector& Origin, const FVector& Direction, bool bFree)
{
	Hover.Reset();
	HoverSegment.Reset();
	HoverComponent.Reset();
	bFreeHover = bFree;
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return;
	}
	FVector Location = FVector::ZeroVector;
	double Distance = 0;
	TSharedPtr<FTarget> Target;
	if (!Pick(*World, Origin, Direction, Location, Distance, Target))
	{
		return;
	}
	HoverComponent = Target->Component;
	FDrawPoint Point;
	Point.Location = Location;
	Point.Snap = ESnap::Surface;
	const UAutoGrindSettings& Settings = *GetDefault<UAutoGrindSettings>();
	if (!bFree && Settings.bDrawSnap)
	{
		const double Radius = FMath::Clamp(Distance * AutoGrindDraw::SnapShare, AutoGrindDraw::MinSnap, AutoGrindDraw::MaxSnap);
		// Scanned lines first: they already run where a line belongs, across modular pieces.
		RefreshSnapLines();
		double Best = Radius;
		for (const FSnapLine& Snap : SnapLines)
		{
			if (!Snap.Bounds.ExpandBy(Radius).IsInsideOrOn(Location))
			{
				continue;
			}
			const std::optional<AutoGrindCore::PolylinePoint> Near = AutoGrindCore::NearestOnPolyline(Snap.Points, AutoGrindDraw::ToCore(Location));
			if (Near && Near->Distance <= Best)
			{
				Best = Near->Distance;
				Point.Snap = ESnap::Line;
				Point.Line = Snap.Line;
				Point.Along = Near->Along;
				Point.Location = AutoGrindDraw::FromCore(Near->Point);
			}
		}
		// Then the sharp edges of what is under the cursor and the pieces around it.
		if (Point.Snap != ESnap::Line)
		{
			if (const std::shared_ptr<AutoGrindCore::EdgeGraph> Edges = GetEdges(*Target))
			{
				if (const std::optional<AutoGrindCore::EdgeGraph::Snap> Snapped = Edges->Nearest(AutoGrindDraw::ToCore(Location), Radius))
				{
					Point.Snap = ESnap::Edge;
					Point.Edges = Edges;
					Point.EdgeSnap = *Snapped;
					Point.Location = AutoGrindDraw::FromCore(Snapped->Point);
				}
			}
		}
	}
	Hover = Point;
	RefreshSegments();
	if (Points.Num() > 0)
	{
		HoverSegment = Segment(Points.Last(), Point);
	}
}

TArray<FVector> FAutoGrindDrawMode::Segment(const FDrawPoint& From, const FDrawPoint& To) const
{
	if (From.Snap == ESnap::Line && To.Snap == ESnap::Line && From.Line && From.Line == To.Line)
	{
		for (const FSnapLine& Snap : SnapLines)
		{
			if (Snap.Line == From.Line)
			{
				return AutoGrindDraw::FromCore(AutoGrindCore::SubPolyline(Snap.Points, From.Along, To.Along, From.Line->bClosed));
			}
		}
	}
	if (bSegmentsFollow && From.Snap == ESnap::Edge && To.Snap == ESnap::Edge && From.Edges && To.Edges)
	{
		// Both points on sharp edges: follow them. The first point may have snapped on another piece's graph;
		// it is found again on the second point's, which spans the pieces around it.
		std::optional<AutoGrindCore::EdgeGraph::Snap> Start = From.EdgeSnap;
		if (From.Edges != To.Edges)
		{
			Start = To.Edges->Nearest(AutoGrindDraw::ToCore(From.Location), 1.0);
		}
		if (Start)
		{
			const std::vector<AutoGrindCore::Vec3> Path = To.Edges->Path(*Start, To.EdgeSnap, AutoGrindDraw::MaxDetour);
			if (Path.size() >= 2)
			{
				return AutoGrindDraw::FromCore(Path);
			}
		}
	}
	return {From.Location, To.Location};
}

TArray<FVector> FAutoGrindDrawMode::Polyline(bool bWithHover) const
{
	TArray<FVector> Out;
	auto Append = [&Out](const TArray<FVector>& Part)
	{
		for (const FVector& P : Part)
		{
			if (Out.Num() == 0 || !Out.Last().Equals(P, 0.01))
			{
				Out.Add(P);
			}
		}
	};
	for (const TArray<FVector>& Part : Segments)
	{
		Append(Part);
	}
	if (Points.Num() == 1)
	{
		Out.Add(Points[0].Location);
	}
	if (bWithHover)
	{
		Append(HoverSegment);
	}
	return Out;
}

bool FAutoGrindDrawMode::AutoRailFromPoints() const
{
	int32 Rails = 0;
	int32 Stones = 0;
	for (const FDrawPoint& Point : Points)
	{
		if (Point.Snap == ESnap::Line && Point.Line)
		{
			(Point.Line->bRail ? Rails : Stones) += 1;
		}
	}
	return Rails > Stones;
}

bool FAutoGrindDrawMode::ResolveRail(bool bAutoRail) const
{
	switch (GetDefault<UAutoGrindSettings>()->DrawKind)
	{
	case EAutoGrindDrawKind::Rail: return true;
	case EAutoGrindDrawKind::Stone: return false;
	case EAutoGrindDrawKind::Auto:
	default: return bAutoRail;
	}
}

bool FAutoGrindDrawMode::Commit(const TArray<FVector>& Line, bool bAutoRail)
{
	UWorld* World = GetEditorWorld();
	if (!World || Line.Num() < 2)
	{
		return false;
	}
	const UAutoGrindSettings& Settings = *GetDefault<UAutoGrindSettings>();
	const bool bClosed = Line.Num() > 3 && Line[0].Equals(Line.Last(), 0.01);
	std::vector<AutoGrindCore::Vec3> Core;
	for (const FVector& P : Line)
	{
		Core.push_back(AutoGrindDraw::ToCore(P));
	}
	const TArray<FVector> Simple = AutoGrindDraw::FromCore(AutoGrindCore::SimplifyPolyline(Core, FMath::Min(0.5, Settings.SimplifyTolerance), bClosed));
	AutoGrind::FPlaceOptions Options;
	FString Error;
	AActor* Placed = nullptr;
	const bool bRail = ResolveRail(bAutoRail);
	if (AutoGrind::MakePlaceOptions(Settings, Options, Error))
	{
		Placed = AutoGrind::PlaceDrawnLine(*World, Simple, bRail, Options, Error);
	}
	double Length = 0;
	for (int32 I = 0; I + 1 < Simple.Num(); ++I)
	{
		Length += FVector::Dist(Simple[I], Simple[I + 1]);
	}
	const FText Message = Placed
		? FText::Format(LOCTEXT("Placed", "Placed a {0} grind {1} m long. Ctrl+Z undoes it."), bRail ? LOCTEXT("RailWord", "rail") : LOCTEXT("StoneWord", "stone"),
			FText::AsNumber(Length / 100, &FNumberFormattingOptions().SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2)))
		: FText::FromString(Error);
	Say(Message);
	FAutoGrindDrawShared::Get().OnDrawn.Broadcast(Placed, Message);
	if (Placed)
	{
		ClearPoints();
	}
	return Placed != nullptr;
}

bool FAutoGrindDrawMode::CommitPoints()
{
	if (Points.Num() < 2)
	{
		Say(LOCTEXT("TwoPoints", "Click at least two points before placing the line."));
		return false;
	}
	TArray<FVector> Line = Polyline(false);
	// Ending on the first point closes the line.
	if (Points.Num() >= 3 && Points[0].Location.Equals(Points.Last().Location, 1.0) && Line.Num() > 3)
	{
		Line.Last() = Line[0];
	}
	return Commit(Line, AutoRailFromPoints());
}

bool FAutoGrindDrawMode::PickRun()
{
	if (!Hover)
	{
		Say(LOCTEXT("PickNothing", "Ctrl+Click on a scanned line or a sharp edge to take all of it."));
		return false;
	}
	if (Hover->Snap == ESnap::Line && Hover->Line)
	{
		return Commit(Hover->Line->Points, Hover->Line->bRail);
	}
	if (Hover->Snap == ESnap::Edge && Hover->Edges)
	{
		const std::vector<AutoGrindCore::Vec3> Run = Hover->Edges->Chain(Hover->EdgeSnap, GetDefault<UAutoGrindSettings>()->MaxCorner);
		if (Run.size() >= 2)
		{
			return Commit(AutoGrindDraw::FromCore(Run), false);
		}
	}
	Say(LOCTEXT("PickNothing", "Ctrl+Click on a scanned line or a sharp edge to take all of it."));
	return false;
}

bool FAutoGrindDrawMode::MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 X, int32 Y)
{
	if (ViewportClient && Viewport)
	{
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Viewport, ViewportClient->GetScene(), ViewportClient->EngineShowFlags).SetRealtimeUpdate(ViewportClient->IsRealtime()));
		FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
		const FViewportCursorLocation Cursor(View, ViewportClient, X, Y);
		FVector Origin = Cursor.GetOrigin();
		if (ViewportClient->IsOrtho())
		{
			Origin -= Cursor.GetDirection() * AutoGrindDraw::OrthoBack;
		}
		const bool bShift = Viewport->KeyState(EKeys::LeftShift) || Viewport->KeyState(EKeys::RightShift);
		UpdateHover(Origin, Cursor.GetDirection(), bShift);
	}
	return false;
}

bool FAutoGrindDrawMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (Click.GetKey() != EKeys::LeftMouseButton)
	{
		return false;
	}
	FVector Origin = Click.GetOrigin();
	if (InViewportClient && InViewportClient->IsOrtho())
	{
		Origin -= Click.GetDirection() * AutoGrindDraw::OrthoBack;
	}
	UpdateHover(Origin, Click.GetDirection(), Click.IsShiftDown());
	if (Click.IsControlDown())
	{
		PickRun();
		return true;
	}
	if (Click.GetEvent() == IE_DoubleClick)
	{
		if (Hover && (Points.Num() == 0 || !Points.Last().Location.Equals(Hover->Location, 0.5)))
		{
			AddPoint(*Hover, &HoverSegment);
		}
		CommitPoints();
		return true;
	}
	if (!Hover)
	{
		Say(LOCTEXT("NoSurface", "Nothing here to draw on: click on a mesh."));
		return true;
	}
	if (Points.Num() > 0 && Points.Last().Location.Equals(Hover->Location, 0.5))
	{
		return true;
	}
	AddPoint(*Hover, &HoverSegment);
	return true;
}

bool FAutoGrindDrawMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	const bool bPressed = Event == IE_Pressed;
	// Letters are left to the camera while it flies (right mouse button held).
	const bool bFlying = Viewport && Viewport->KeyState(EKeys::RightMouseButton);
	UAutoGrindSettings* Settings = GetMutableDefault<UAutoGrindSettings>();
	if (Key == EKeys::Enter)
	{
		if (bPressed)
		{
			CommitPoints();
		}
		return true;
	}
	if (Key == EKeys::BackSpace)
	{
		if (bPressed && Points.Num() > 0)
		{
			RemoveLastPoint();
		}
		return true;
	}
	if (Key == EKeys::Escape)
	{
		if (bPressed)
		{
			if (Points.Num() > 0)
			{
				ClearPoints();
				Say(LOCTEXT("Cancelled", "Points dropped. Esc again leaves Draw mode."));
			}
			else if (GEditor)
			{
				// Leaving the mode destroys it: not from inside its own input handling.
				GEditor->GetTimerManager()->SetTimerForNextTick([]() { FAutoGrindDrawMode::SetActive(false); });
			}
		}
		return true;
	}
	if (!bFlying && Key == EKeys::T)
	{
		if (bPressed)
		{
			Settings->DrawKind = Settings->DrawKind == EAutoGrindDrawKind::Auto ? EAutoGrindDrawKind::Rail : Settings->DrawKind == EAutoGrindDrawKind::Rail ? EAutoGrindDrawKind::Stone : EAutoGrindDrawKind::Auto;
			Settings->SaveConfig();
			Say(FText::Format(LOCTEXT("KindChanged", "Lines drawn now: {0}"), AutoGrindDraw::KindName(Settings->DrawKind)));
		}
		return true;
	}
	if (!bFlying && Key == EKeys::F)
	{
		if (bPressed)
		{
			Settings->bDrawFollowEdges = !Settings->bDrawFollowEdges;
			Settings->SaveConfig();
			RefreshSegments();
			Say(Settings->bDrawFollowEdges ? LOCTEXT("FollowOn", "Following edges between points.") : LOCTEXT("FollowOff", "Straight between points."));
		}
		return true;
	}
	if (!bFlying && Key == EKeys::S)
	{
		if (bPressed)
		{
			Settings->bDrawSnap = !Settings->bDrawSnap;
			Settings->SaveConfig();
			Say(Settings->bDrawSnap ? LOCTEXT("SnapOn", "Snapping to lines and edges.") : LOCTEXT("SnapOff", "Snapping off: points go where you click."));
		}
		return true;
	}
	return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

void FAutoGrindDrawMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);
	if (!PDI)
	{
		return;
	}
	const FLinearColor Drawn(FColor(255, 220, 0));
	const FLinearColor Ahead(FColor(255, 240, 150));
	const TArray<FVector> Line = Polyline(false);
	for (int32 I = 0; I + 1 < Line.Num(); ++I)
	{
		PDI->DrawLine(Line[I], Line[I + 1], Drawn, SDPG_Foreground, 3);
	}
	for (const FDrawPoint& Point : Points)
	{
		PDI->DrawPoint(Point.Location, Drawn, 10, SDPG_Foreground);
	}
	if (!Hover)
	{
		return;
	}
	for (int32 I = 0; I + 1 < HoverSegment.Num(); ++I)
	{
		PDI->DrawLine(HoverSegment[I], HoverSegment[I + 1], Ahead, SDPG_Foreground, 2);
	}
	FLinearColor Marker = FLinearColor::White;
	if (Hover->Snap == ESnap::Line && Hover->Line)
	{
		Marker = FLinearColor(FColor(80, 255, 120));
		for (int32 I = 0; I + 1 < Hover->Line->Points.Num(); ++I)
		{
			PDI->DrawLine(Hover->Line->Points[I], Hover->Line->Points[I + 1], Marker, SDPG_Foreground, 1.5f);
		}
	}
	else if (Hover->Snap == ESnap::Edge && Hover->Edges)
	{
		Marker = FLinearColor(FColor(60, 230, 255));
		const std::pair<AutoGrindCore::Vec3, AutoGrindCore::Vec3> Ends = Hover->Edges->EdgeEnds(Hover->EdgeSnap.Edge);
		PDI->DrawLine(AutoGrindDraw::FromCore(Ends.first), AutoGrindDraw::FromCore(Ends.second), Marker, SDPG_Foreground, 2);
	}
	PDI->DrawPoint(Hover->Location, Marker, 14, SDPG_Foreground);
}

void FAutoGrindDrawMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	FEdMode::DrawHUD(ViewportClient, Viewport, View, Canvas);
	const UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Canvas || !Font)
	{
		return;
	}
	const UAutoGrindSettings& Settings = *GetDefault<UAutoGrindSettings>();
	TArray<FString> Lines;
	Lines.Add(FText::Format(LOCTEXT("HudTitle", "AutoGrind Draw    type: {0} (T)    follow edges: {1} (F)    snapping: {2} (S)"), AutoGrindDraw::KindName(Settings.DrawKind),
		Settings.bDrawFollowEdges ? LOCTEXT("On", "on") : LOCTEXT("Off", "off"), Settings.bDrawSnap ? LOCTEXT("On", "on") : LOCTEXT("Off", "off")).ToString());
	Lines.Add(LOCTEXT("HudClicks", "Click: add point    Ctrl+Click: take a whole edge or line    Shift+Click: point without snapping").ToString());
	Lines.Add(LOCTEXT("HudKeys", "Enter or double-click: place    Backspace: remove point    Esc: drop points, then leave").ToString());
	FString Status;
	if (Points.Num() > 0)
	{
		const TArray<FVector> Line = Polyline(true);
		double Length = 0;
		for (int32 I = 0; I + 1 < Line.Num(); ++I)
		{
			Length += FVector::Dist(Line[I], Line[I + 1]);
		}
		Status = FString::Printf(TEXT("%d point(s), %.2f m"), Points.Num(), Length / 100);
	}
	if (Hover)
	{
		const TCHAR* Where = Hover->Snap == ESnap::Line ? (Hover->Line && Hover->Line->bRail ? TEXT("on a scanned rail") : TEXT("on a scanned line"))
			: Hover->Snap == ESnap::Edge ? TEXT("on a sharp edge") : bFreeHover ? TEXT("free point") : TEXT("on the surface");
		Status += (Status.IsEmpty() ? FString() : FString(TEXT("    "))) + Where;
	}
	if (!Status.IsEmpty())
	{
		Lines.Add(Status);
	}
	if (!LastMessage.IsEmpty() && FPlatformTime::Seconds() - LastMessageTime < AutoGrindDraw::MessageSeconds)
	{
		Lines.Add(LastMessage.ToString());
	}
	float Y = 40;
	for (int32 I = 0; I < Lines.Num(); ++I)
	{
		const FLinearColor Colour = I == 0 ? FLinearColor(FColor(255, 220, 0)) : FLinearColor::White;
		Canvas->DrawShadowedString(12, Y, *Lines[I], Font, Colour);
		Y += 16;
	}
}

#undef LOCTEXT_NAMESPACE
