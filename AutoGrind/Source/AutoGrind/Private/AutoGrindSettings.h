#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "Core/AutoGrindCore.h"
#include "AutoGrindSettings.generated.h"

// Which actors a scan searches for lines.
UENUM()
enum class EAutoGrindScanScope : uint8
{
	Selected UMETA(DisplayName = "Selected Actors"),
	Level UMETA(DisplayName = "Whole Level")
};

// Edges that fall less than Min Drop onto a wide, level landing: curbs, manual pads, low steps.
UENUM()
enum class EAutoGrindLowLedges : uint8
{
	Off UMETA(DisplayName = "Leave Out"),
	Suggest UMETA(DisplayName = "List for Review"),
	Keep UMETA(DisplayName = "Keep")
};

// Starting points for the detection settings.
UENUM()
enum class EAutoGrindPreset : uint8
{
	Skatepark UMETA(ToolTip = "The defaults: ledges, boxes, coping and rails of skateparks and plazas."),
	Street UMETA(ToolTip = "City maps: curbs kept, roofs and wall tops above 4 m left out."),
	Strict UMETA(ToolTip = "Fewer, surer lines: sharp edges and long lines only."),
	Loose UMETA(ToolTip = "Everything that might grind, for review: low ledges, ridges, rounded edges.")
};

// How the generated spline passes between its points.
UENUM()
enum class EAutoGrindPointType : uint8
{
	Linear UMETA(DisplayName = "Linear (exact)", ToolTip = "Straight between points, exactly on the edge. Best for ledges and box edges."),
	Curve UMETA(DisplayName = "Curve (smooth)", ToolTip = "A smooth curve through the points. Smoother on coping and curved rails.")
};

// The grind type of a line drawn by hand.
UENUM()
enum class EAutoGrindDrawKind : uint8
{
	Auto UMETA(ToolTip = "Rail or stone as the scanned lines your points snap to; stone elsewhere."),
	Rail,
	Stone
};

// What counts as grindable, what to leave out, and how lines are placed. Shown in the AutoGrind panel and
// remembered per user and project.
UCLASS(config = EditorPerProjectUserSettings)
class UAutoGrindSettings : public UObject
{
	GENERATED_BODY()

public:
	// ----- Ledges -----

	/** A face this flat or flatter is a top. Only the outer edge of a top can carry a grind line. */
	UPROPERTY(EditAnywhere, config, Category = "Ledges", meta = (ClampMin = "0", ClampMax = "89", Units = "Degrees"))
	double MaxTopSlope = 45;

	/** How far the surface must fall just past an edge for it to be a ledge. */
	UPROPERTY(EditAnywhere, config, Category = "Ledges", meta = (ClampMin = "0", Units = "Centimeters"))
	double MinDrop = 25;

	/** Edges that fall less than Min Drop onto a wide, level landing: curbs, manual pads and low steps. Stairs are never lined. */
	UPROPERTY(EditAnywhere, config, Category = "Ledges")
	EAutoGrindLowLedges LowLedges = EAutoGrindLowLedges::Suggest;

	/** How far a low ledge or curb must fall. */
	UPROPERTY(EditAnywhere, config, Category = "Ledges", meta = (ClampMin = "1", Units = "Centimeters", EditCondition = "LowLedges != EAutoGrindLowLedges::Off"))
	double LowLedgeMinDrop = 12;

	/** An edge that falls onto a slope this steep is a lip even when it falls less than Min Drop: the top of a bank or of a quarter pipe with no coping. 0 turns this off. */
	UPROPERTY(EditAnywhere, config, Category = "Ledges", meta = (ClampMin = "0", ClampMax = "89", Units = "Degrees"))
	double SteepLip = 40;

	/** Convexity: how sharply the surface must turn down over an edge, measured across a few centimetres. Keeps lines off domes, mounds and rounded shapes; bevelled and bullnosed ledges still count. 0 turns this off. */
	UPROPERTY(EditAnywhere, config, Category = "Ledges", meta = (ClampMin = "0", ClampMax = "89", Units = "Degrees"))
	double MinEdgeAngle = 30;

	/** A flat surface this close past an edge, level with it or a small step below, carries the top on: the edge is a seam, a step or the gap between the slats of a bench or table, not a lip. */
	UPROPERTY(EditAnywhere, config, Category = "Ledges", meta = (ClampMin = "0", ClampMax = "50", Units = "Centimeters"))
	double GapBridge = 6;

	/** Test the space over and beside each edge against the meshes around it, so an edge buried in a wall or pressed against one is not a line. Slower on big scans. */
	UPROPERTY(EditAnywhere, config, Category = "Ledges")
	bool bCheckWalls = true;

	// ----- Rails -----

	/** A top narrower than this with a fall on both sides gives one line along its crest: a rail when it is round or thin, otherwise stone (a thin wall or beam). Retail rails measure 3 to 8 cm. */
	UPROPERTY(EditAnywhere, config, Category = "Rails", meta = (ClampMin = "0", Units = "Centimeters"))
	double RailMaxWidth = 15;

	/** A flat narrow top on a body deeper than this is a wall or beam, placed as stone. 0 makes every narrow top a rail. */
	UPROPERTY(EditAnywhere, config, Category = "Rails", meta = (ClampMin = "0", Units = "Centimeters"))
	double RailMaxThickness = 20;

	/** How far the surface must fall past a rail's sides. Less than Min Drop, so a railing along a wall or a low flat bar is still a rail. */
	UPROPERTY(EditAnywhere, config, Category = "Rails", meta = (ClampMin = "0", Units = "Centimeters"))
	double RailMinDrop = 10;

	/** A round top up to this wide (a pipe) gives one line along its crest instead of two on its shoulders. */
	UPROPERTY(EditAnywhere, config, Category = "Rails", meta = (ClampMin = "0", Units = "Centimeters"))
	double RoundTopMaxWidth = 40;

	/** Also find sharp convex ridges with no top to stand on, such as an A-frame rail or a steep roof ridge. They are listed for review. */
	UPROPERTY(EditAnywhere, config, Category = "Rails")
	bool bDetectRidges = false;

	/** How sharply the two faces of a ridge must meet. */
	UPROPERTY(EditAnywhere, config, Category = "Rails", meta = (ClampMin = "10", ClampMax = "170", Units = "Degrees", EditCondition = "bDetectRidges"))
	double MinRidgeAngle = 70;

	// ----- Lines -----

	/** Lines shorter than this are dropped, such as the caps of rail posts. Measured after lines are joined across meshes. */
	UPROPERTY(EditAnywhere, config, Category = "Lines", meta = (ClampMin = "0", Units = "Centimeters"))
	double MinLength = 50;

	/** Edges steeper than this cannot be ridden and are skipped. */
	UPROPERTY(EditAnywhere, config, Category = "Lines", meta = (ClampMin = "0", ClampMax = "89", Units = "Degrees"))
	double MaxLineSlope = 45;

	/** A sharper turn ends one line and starts the next, as at the corner of a box. */
	UPROPERTY(EditAnywhere, config, Category = "Lines", meta = (ClampMin = "0", ClampMax = "180", Units = "Degrees"))
	double MaxCorner = 40;

	/** Join the lines of separate meshes placed next to each other, such as modular ledge or rail pieces, into one line. */
	UPROPERTY(EditAnywhere, config, Category = "Lines")
	bool bJoinAcrossMeshes = true;

	/** Pieces of one ledge line are joined across a gap this short, such as the seam between two blocks or modules. */
	UPROPERTY(EditAnywhere, config, Category = "Lines", meta = (ClampMin = "0", Units = "Centimeters"))
	double JoinGap = 10;

	/** A rail continues across a gap this short, where a post or bracket meets it or two rail pieces almost touch. */
	UPROPERTY(EditAnywhere, config, Category = "Lines", meta = (ClampMin = "0", Units = "Centimeters"))
	double RailJoinGap = 40;

	// ----- What to line -----

	/** Edges falling further than this are left out: roofs, cliffs and the tops of high walls. 0 means no limit. */
	UPROPERTY(EditAnywhere, config, Category = "Filtering", meta = (ClampMin = "0", Units = "Centimeters"))
	double MaxDrop = 0;

	/** Lines falling further than this are listed with a lower confidence: often the back of a deck. */
	UPROPERTY(EditAnywhere, config, Category = "Filtering", meta = (ClampMin = "0", Units = "Centimeters"))
	double HighDrop = 150;

	/** Lines at or above this confidence are ticked after a scan; the rest are listed unticked for review. */
	UPROPERTY(EditAnywhere, config, Category = "Filtering", meta = (ClampMin = "0", ClampMax = "1"))
	double KeepConfidence = 0.5;

	/** Turn round triangles wound against their neighbours and drop double-sided copies, as in ripped or hand-made meshes with flipped faces. */
	UPROPERTY(EditAnywhere, config, Category = "Filtering")
	bool bRepairWinding = true;

	/** Whole Level scans skip actors and meshes whose names contain one of these words (case does not matter, whole words: "Tree" does not match "Street"). */
	UPROPERTY(EditAnywhere, config, Category = "Filtering")
	TArray<FString> ExcludeNames = {TEXT("Foliage"), TEXT("Tree"), TEXT("Grass"), TEXT("Bush"), TEXT("Shrub"), TEXT("Leaf"), TEXT("Leaves"), TEXT("Flower"), TEXT("Sky"), TEXT("Cloud"), TEXT("Decal"), TEXT("Water"), TEXT("Particle"), TEXT("Imposter"), TEXT("Billboard"), TEXT("Cable"), TEXT("Wire")};

	/** Actors with one of these tags are never scanned, in any scope. */
	UPROPERTY(EditAnywhere, config, Category = "Filtering")
	TArray<FName> ExcludeTags = {TEXT("NoGrind"), TEXT("AutoGrindIgnore")};

	/** Whole Level scans skip meshes with collision turned off: a skater cannot stand on them. */
	UPROPERTY(EditAnywhere, config, Category = "Filtering")
	bool bRequireCollision = true;

	/** Whole Level scans skip actors hidden in the editor. */
	UPROPERTY(EditAnywhere, config, Category = "Filtering")
	bool bSkipHidden = true;

	/** Whole Level scans skip meshes smaller than this in every direction: cans, bottles, small props. */
	UPROPERTY(EditAnywhere, config, Category = "Filtering", meta = (ClampMin = "0", Units = "Centimeters"))
	double MinMeshSize = 25;

	// ----- Review -----

	/** Also draw the edges that were considered and turned down, in grey, to see why an edge has no line. */
	UPROPERTY(EditAnywhere, config, Category = "Review")
	bool bShowNearMisses = false;

	/** Draw arrows along every line, not only the selected ones. */
	UPROPERTY(EditAnywhere, config, Category = "Review")
	bool bShowDirections = false;

	/** Thickness of the lines drawn in the viewport. */
	UPROPERTY(EditAnywhere, config, Category = "Review", meta = (ClampMin = "1", ClampMax = "12"))
	float PreviewThickness = 3;

	// ----- Output -----

	/** The grind actor placed along each line: Rollout Inline's GrindActor. Change it only for a project that keeps it elsewhere. */
	UPROPERTY(EditAnywhere, config, Category = "Output", meta = (MetaClass = "/Script/Engine.Actor"))
	FSoftClassPath GrindActorClass = FSoftClassPath(TEXT("/Game/MainFolder/Blueprints/Grinding/GrindActor.GrindActor_C"));

	/** The grind actor's enum property that holds its grind type. */
	UPROPERTY(EditAnywhere, config, Category = "Output", AdvancedDisplay)
	FName GrindTypeProperty = TEXT("GrindType");

	/** The enumerator of the grind type that means a rail. */
	UPROPERTY(EditAnywhere, config, Category = "Output", AdvancedDisplay)
	FString RailTypeName = TEXT("NewEnumerator0");

	/** The enumerator of the grind type that means stone (a ledge). */
	UPROPERTY(EditAnywhere, config, Category = "Output", AdvancedDisplay)
	FString StoneTypeName = TEXT("NewEnumerator1");

	/** Outliner folder for generated grind actors. Lines drawn by hand go in a Drawn folder inside it. */
	UPROPERTY(EditAnywhere, config, Category = "Output")
	FName OutputFolder = TEXT("AutoGrind");

	/** Generated actors are named this, then the actor the line runs along. */
	UPROPERTY(EditAnywhere, config, Category = "Output")
	FString LabelPrefix = TEXT("AutoGrind_");

	/** Raises every generated spline by this much, for a grind actor whose collision should sit above the edge. */
	UPROPERTY(EditAnywhere, config, Category = "Output", meta = (Units = "Centimeters"))
	double HeightOffset = 0;

	/** How the generated spline passes between its points. */
	UPROPERTY(EditAnywhere, config, Category = "Output")
	EAutoGrindPointType PointType = EAutoGrindPointType::Linear;

	// ----- Advanced -----

	/** How far past an edge its fall is measured. */
	UPROPERTY(EditAnywhere, config, Category = "Advanced", AdvancedDisplay, meta = (ClampMin = "1", Units = "Centimeters"))
	double ProbeDistance = 15;

	/** Long edges are checked in spans this long. Smaller spans find smaller obstructions but take more time. */
	UPROPERTY(EditAnywhere, config, Category = "Advanced", AdvancedDisplay, meta = (ClampMin = "1", ClampMax = "500", Units = "Centimeters"))
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

	// ----- Kept by the panel, not shown in the settings -----

	UPROPERTY(config)
	EAutoGrindScanScope Scope = EAutoGrindScanScope::Selected;

	UPROPERTY(config)
	EAutoGrindDrawKind DrawKind = EAutoGrindDrawKind::Auto;

	UPROPERTY(config)
	bool bDrawFollowEdges = true;

	UPROPERTY(config)
	bool bDrawSnap = true;

	/** Sets the detection settings to a preset's values. Output, filtering lists and preview settings are left alone. */
	void ApplyPreset(EAutoGrindPreset Preset);

	/** Every setting back to its default, except where lines are placed. */
	void ResetDetection();

	AutoGrindCore::Settings ToCore() const;

	/** Whether a name contains one of ExcludeNames as a whole word (CamelCase and digits split words). */
	bool IsExcludedName(const FString& Name) const;
};
