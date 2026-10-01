#pragma once

#include "CoreMinimal.h"
#include "AutoGrindScan.h"
#include "EdMode.h"

#include <memory>

class UStaticMeshComponent;

// What the panel and the Draw mode share: the panel's scanned lines, to snap to and follow, and news of
// lines placed by hand.
class FAutoGrindDrawShared
{
public:
	static FAutoGrindDrawShared& Get();

	// Replaces the lines the Draw mode snaps to.
	void SetSnapLines(const TArray<TSharedPtr<FAutoGrindLine>>& Lines);
	const TArray<TSharedPtr<FAutoGrindLine>>& GetSnapLines() const { return SnapLines; }
	uint32 GetSnapVersion() const { return SnapVersion; }

	// A line was placed, or placing one failed: the message says which.
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDrawn, AActor* /*Placed*/, const FText& /*Message*/);
	FOnDrawn OnDrawn;
	// The Draw mode was entered or left.
	FSimpleMulticastDelegate OnModeChanged;

private:
	TArray<TSharedPtr<FAutoGrindLine>> SnapLines;
	uint32 SnapVersion = 0;
};

// Draws grind lines by hand in the level viewport: click points on the level and a grind actor is placed
// along them. Clicks snap to scanned lines, then to sharp edges, then to the surface; between two clicks the
// line follows the scanned line or the edges they lie on.
//
//   Click            add a point             Ctrl+Click   take a whole edge or line and place it
//   Shift+Click      add a point, no snap    Enter        place the line (double-click too)
//   Backspace        remove the last point   Esc          drop the points, then leave the mode
//   T                rail, stone or auto     F            follow edges between points, or go straight
class FAutoGrindDrawMode : public FEdMode
{
public:
	static const FEditorModeID ModeId;

	FAutoGrindDrawMode();
	virtual ~FAutoGrindDrawMode() override;

	static bool IsActive();
	static void SetActive(bool bActive);

	// FEdMode
	virtual void Enter() override;
	virtual void Exit() override;
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 X, int32 Y) override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;
	virtual bool ShouldDrawWidget() const override;
	virtual bool GetCursor(EMouseCursor::Type& OutCursor) const override;
	// Not marked override: these are declared differently across engine versions, and match the base where it has them.
	virtual bool UsesToolkits() const;
	virtual FString GetReferencerName() const;

private:
	// The triangles of one component in the world, for picking, and the sharp edges of it and the components
	// around it, for snapping and following.
	struct FTarget
	{
		TWeakObjectPtr<const UStaticMeshComponent> Component;
		FTransform Transform;
		FBox Bounds;
		std::shared_ptr<AutoGrindCore::TriangleField> Field;
		std::shared_ptr<AutoGrindCore::EdgeGraph> Edges;
		bool bEdgesBuilt = false;
	};

	enum class ESnap : uint8
	{
		Surface,
		Edge,
		Line
	};

	struct FDrawPoint
	{
		FVector Location = FVector::ZeroVector;
		ESnap Snap = ESnap::Surface;
		// For an edge snap: the graph and where on it.
		std::shared_ptr<AutoGrindCore::EdgeGraph> Edges;
		AutoGrindCore::EdgeGraph::Snap EdgeSnap;
		// For a line snap: the scanned line and how far along it.
		TSharedPtr<FAutoGrindLine> Line;
		double Along = 0;
	};

	struct FCandidate
	{
		TWeakObjectPtr<const UStaticMeshComponent> Component;
		FBox Bounds;
	};

	// The scanned lines in the core's form, rebuilt when the panel scans again.
	struct FSnapLine
	{
		TSharedPtr<FAutoGrindLine> Line;
		std::vector<AutoGrindCore::Vec3> Points;
		FBox Bounds;
	};

	AutoGrind::FMeshLibrary Library;
	TArray<FCandidate> Candidates;
	double CandidatesTime = -1;
	TMap<TWeakObjectPtr<const UStaticMeshComponent>, TSharedPtr<FTarget>> Targets;
	TArray<FSnapLine> SnapLines;
	uint32 SnapVersion = MAX_uint32;

	TArray<FDrawPoint> Points;
	// The way from each point to the next, worked out once when the point is added: Segments[I] runs from
	// Points[I] to Points[I + 1].
	TArray<TArray<FVector>> Segments;
	// Whether Segments followed edges, to work them out again when that setting changes.
	bool bSegmentsFollow = true;
	TOptional<FDrawPoint> Hover;
	// The way from the last point to the hover point.
	TArray<FVector> HoverSegment;
	TWeakObjectPtr<const UStaticMeshComponent> HoverComponent;
	FText LastMessage;
	double LastMessageTime = -1;
	bool bFreeHover = false;

	UWorld* GetEditorWorld() const;
	void RefreshCandidates(UWorld& World);
	void RefreshSnapLines();
	TSharedPtr<FTarget> GetTarget(const UStaticMeshComponent& Component);
	std::shared_ptr<AutoGrindCore::EdgeGraph> GetEdges(FTarget& Target);
	// The nearest surface along a ray, and the component it belongs to.
	bool Pick(UWorld& World, const FVector& Origin, const FVector& Direction, FVector& OutLocation, double& OutDistance, TSharedPtr<FTarget>& OutTarget);
	void UpdateHover(const FVector& Origin, const FVector& Direction, bool bFree);
	// The way from one point to the next: along a scanned line or edges when both lie on one, else straight.
	TArray<FVector> Segment(const FDrawPoint& From, const FDrawPoint& To) const;
	TArray<FVector> Polyline(bool bWithHover) const;
	// KnownWay, when given, is the way from the last point to this one, already worked out.
	void AddPoint(const FDrawPoint& Point, const TArray<FVector>* KnownWay = nullptr);
	void RemoveLastPoint();
	void ClearPoints();
	// Works the segments out again if the follow-edges setting changed since.
	void RefreshSegments();
	bool PickRun();
	bool Commit(const TArray<FVector>& Line, bool bAutoRail);
	bool CommitPoints();
	bool ResolveRail(bool bAutoRail) const;
	bool AutoRailFromPoints() const;
	void Say(const FText& Message);
	void Reset();
};
