#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/AutoGrindCore.h"
#include "AutoGrindSettings.generated.h"

// What counts as grindable. Shown in the AutoGrind panel and remembered per user and project.
// The defaults find every hand-placed grind line on OutdoorSkatepark and TheBigHall (Tests/README.md).
UCLASS(config = EditorPerProjectUserSettings)
class UAutoGrindSettings : public UObject
{
	GENERATED_BODY()

	public:
	/** A face this flat or flatter is a top. Only the outer edge of a top can carry a grind line. */
	UPROPERTY(EditAnywhere, config, Category = "Edges", meta = (ClampMin = "0", ClampMax = "89", Units = "Degrees"))
	double MaxTopSlope = 45;

	/** How far the surface must fall just past an edge. Stair nosings fall less than this, so steps are not lines. */
	UPROPERTY(EditAnywhere, config, Category = "Edges", meta = (ClampMin = "0", Units = "Centimeters"))
	double MinDrop = 25;

	/** An edge that falls onto a slope this steep is a lip even when it falls less than Min Drop: the top of a bank or of a quarter pipe with no coping. 0 turns this off. */
	UPROPERTY(EditAnywhere, config, Category = "Edges", meta = (ClampMin = "0", ClampMax = "89", Units = "Degrees"))
	double SteepLip = 40;

	/** Lines shorter than this are dropped, such as the caps of rail posts. */
	UPROPERTY(EditAnywhere, config, Category = "Edges", meta = (ClampMin = "0", Units = "Centimeters"))
	double MinLength = 50;

	/** Edges steeper than this cannot be ridden and are skipped. */
	UPROPERTY(EditAnywhere, config, Category = "Edges", meta = (ClampMin = "0", ClampMax = "89", Units = "Degrees"))
	double MaxLineSlope = 45;

	/** A sharper turn ends one line and starts the next, as at the corner of a box. */
	UPROPERTY(EditAnywhere, config, Category = "Edges", meta = (ClampMin = "0", ClampMax = "180", Units = "Degrees"))
	double MaxCorner = 40;

	/** A flat surface this close past an edge, level with it or a small step below, carries the top on: the edge is a seam, a step or the gap between the slats of a bench or table, not a lip. */
	UPROPERTY(EditAnywhere, config, Category = "Edges", meta = (ClampMin = "0", ClampMax = "50", Units = "Centimeters"))
	double GapBridge = 6;

	/** Pieces of one ledge line are joined across a gap this short, such as the seam between two blocks of a wall. */
	UPROPERTY(EditAnywhere, config, Category = "Edges", meta = (ClampMin = "0", Units = "Centimeters"))
	double JoinGap = 10;

	/** Also test the space over and beside each edge against the meshes around it, so an edge buried in a wall or pressed against one is not a line. Slower on big selections. */
	UPROPERTY(EditAnywhere, config, Category = "Edges")
	bool bCheckWalls = true;

	/** A top narrower than this with a fall on both sides is a rail: one line along its crest, placed as a rail grind. Retail rails measure 3 to 8 cm. */
	UPROPERTY(EditAnywhere, config, Category = "Rails", meta = (ClampMin = "0", Units = "Centimeters"))
	double RailMaxWidth = 15;

	/** How far the surface must fall past a rail. Less than Min Drop, so a railing along a wall or a low flat bar is still a rail. */
	UPROPERTY(EditAnywhere, config, Category = "Rails", meta = (ClampMin = "0", Units = "Centimeters"))
	double RailMinDrop = 10;

	/** A rail continues across a gap this short, where a post or bracket meets it and breaks the tube's top. */
	UPROPERTY(EditAnywhere, config, Category = "Rails", meta = (ClampMin = "0", Units = "Centimeters"))
	double RailJoinGap = 40;

	/** How far past an edge its fall is measured. */
	UPROPERTY(EditAnywhere, config, Category = "Advanced", AdvancedDisplay, meta = (ClampMin = "1", Units = "Centimeters"))
	double ProbeDistance = 15;

	/** Long edges are checked in spans this long. Smaller spans find smaller obstructions but take more time. */
	UPROPERTY(EditAnywhere, config, Category = "Edges", meta = (ClampMin = "1", ClampMax = "500", Units = "Centimeters"))
	double SampleSpacing = 25;

	/** Height above an edge that must be clear just past it, so the edge of a box pushed against a wall is not a lip. */
	UPROPERTY(EditAnywhere, config, Category = "Advanced", AdvancedDisplay, meta = (ClampMin = "0", Units = "Centimeters"))
	double Clearance = 150;

	/** With no surface this far below an edge, it overhangs nothing and is skipped. */
	UPROPERTY(EditAnywhere, config, Category = "Advanced", AdvancedDisplay, meta = (ClampMin = "0", Units = "Centimeters"))
	double MaxDropSearch = 5000;

	/** Vertices closer than this are treated as one, so split normals and UV seams do not break an edge. */
	UPROPERTY(EditAnywhere, config, Category = "Advanced", AdvancedDisplay, meta = (ClampMin = "0.001", Units = "Centimeters"))
	double WeldTolerance = 0.1;

	/** Points closer than this to a straight line through their neighbours are removed from a line. */
	UPROPERTY(EditAnywhere, config, Category = "Advanced", AdvancedDisplay, meta = (ClampMin = "0", Units = "Centimeters"))
	double SimplifyTolerance = 1;

	/** Also draw the edges that were considered and turned down, in grey, to see why an edge has no line. */
	UPROPERTY(EditAnywhere, config, Category = "Preview")
	bool bShowNearMisses = false;

	AutoGrindCore::Settings ToCore() const;
};
