#pragma once

#include "CoreMinimal.h"
#include "Core/AutoGrindCore.h"
#include "Templates/Function.h"
#include "UObject/WeakObjectPtr.h"

#include <vector>

class AActor;
class UAutoGrindSettings;
class UStaticMesh;
class UStaticMeshComponent;
class UWorld;

// A grind line in the level, in world space: found by a scan or drawn by hand.
struct FAutoGrindLine
{
	TArray<FVector> Points;
	bool bRail = false;
	// A closed line repeats its first point at the end.
	bool bClosed = false;
	// Unticked lines stay in the list but are not generated.
	bool bKeep = true;
	// What the scan suggested, and how sure it was.
	bool bSuggested = true;
	float Confidence = 1;
	uint32 Notes = 0;
	AutoGrindCore::LineShape Shape = AutoGrindCore::LineShape::Lip;
	double Length = 0;
	double Drop = 0;
	double TopWidth = 0;
	double Thickness = 0;
	double EdgeAngle = 0;
	// Every actor the line runs along, the main one first.
	TArray<TWeakObjectPtr<AActor>> Sources;
	// The main actor's label, with how many more there are.
	FString SourceLabel;

	AActor* MainSource() const;
	// A few lines on what the line is and why it has the confidence it has, for tooltips.
	FText Describe() const;
};

// An edge that was considered and turned down, with the reason.
struct FAutoGrindNearMiss
{
	FVector A = FVector::ZeroVector;
	FVector B = FVector::ZeroVector;
	AutoGrindCore::Reject Reason = AutoGrindCore::Reject::SmallFall;
	FString Text;
};

struct FAutoGrindScanRequest
{
	// The actors whose static meshes are searched for lines.
	TArray<AActor*> Actors;
	AutoGrindCore::Settings Core;
	bool bCollectNearMisses = false;
	// Test the space over and beside each edge against the triangles of the meshes around it.
	bool bCheckWalls = true;
	// Show a progress dialog with a Cancel button.
	bool bShowProgress = true;
	// When set, only components it accepts are searched.
	TFunction<bool(const UStaticMeshComponent&)> ComponentFilter;
};

struct FAutoGrindScanResult
{
	TArray<FAutoGrindLine> Lines;
	TArray<FAutoGrindNearMiss> NearMisses;
	int32 ActorCount = 0;
	int32 MeshCount = 0;
	int64 TriangleCount = 0;
	// Static meshes with no source geometry in this project, such as cooked stand-ins.
	TArray<FString> Skipped;
	double Seconds = 0;
	bool bCancelled = false;
};

namespace AutoGrind
{
	// The class name of Rollout Inline's GrindActor. Existing ones are never scanned or landed on.
	extern const TCHAR* GrindActorClassName;

	bool IsGrindActor(const AActor& Actor);

	// Not a grind actor and not tagged to be left alone.
	bool IsScannable(const AActor& Actor, const UAutoGrindSettings& Settings);

	// Whether a component passes the Whole Level filters: name, collision, visibility and size.
	bool PassesLevelFilters(const UStaticMeshComponent& Component, const UAutoGrindSettings& Settings);

	// The actors a Whole Level scan searches. OutFilteredOut counts those the filters left out.
	TArray<AActor*> CollectLevelActors(UWorld& World, const UAutoGrindSettings& Settings, int32* OutFilteredOut = nullptr);

	// Searches the static meshes of the request's actors for grind lines: plain, instanced and spline-bent.
	// Only these actors carry lines, but the fall past each edge is traced against the whole world's collision.
	// With bCheckWalls the space over and beside each edge is also tested against the triangles of every
	// collision-enabled static mesh nearby, which sees walls the traces cannot.
	FAutoGrindScanResult Scan(UWorld& World, const FAutoGrindScanRequest& Request);

	// LOD 0 of the mesh's source geometry in its own space, wound as AutoGrindCore::Mesh requires.
	// False when the mesh has no source geometry.
	bool ReadMesh(const UStaticMesh& Mesh, TArray<FVector3f>& OutPositions, TArray<uint32>& OutIndices);

	// Reads each static mesh once and places it in the world as the core reads it.
	class FMeshLibrary
	{
	public:
		// Appends the component's world-space meshes: one per instance of an instanced component, bent along
		// the spline for a spline mesh. False when its mesh has no source geometry.
		bool Place(const UStaticMeshComponent& Component, std::vector<AutoGrindCore::Mesh>& Out);

	private:
		struct FLocal
		{
			TArray<FVector3f> Positions;
			TArray<uint32> Indices;
		};
		TMap<const UStaticMesh*, TSharedPtr<FLocal>> Read;
		TSet<const UStaticMesh*> Unreadable;
		const FLocal* Load(const UStaticMesh& Mesh);
	};
}
