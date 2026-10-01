#pragma once

#include "CoreMinimal.h"
#include "Core/AutoGrindCore.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class UStaticMesh;
class UWorld;

// A grind line found in the level, in world space.
struct FAutoGrindLine
{
	TArray<FVector> Points;
	bool bRail = false;
	bool bClosed = false;
	// Unticked lines stay in the list but are not generated.
	bool bKeep = true;
	double Length = 0;
	double Drop = 0;
	double TopWidth = 0;
	TWeakObjectPtr<AActor> Source;
	FString SourceLabel;
};

// An edge that was considered and turned down, with the reason.
struct FAutoGrindNearMiss
{
	FVector A = FVector::ZeroVector;
	FVector B = FVector::ZeroVector;
	FString Reason;
};

struct FAutoGrindScanResult
{
	TArray<FAutoGrindLine> Lines;
	TArray<FAutoGrindNearMiss> NearMisses;
	int32 MeshCount = 0;
	// Static meshes with no source geometry in this project, such as cooked stand-ins.
	TArray<FString> Skipped;
	double Seconds = 0;
};

namespace AutoGrind
{
	// The class name of Rollout Inline's GrindActor. Existing ones are never scanned or landed on.
	extern const TCHAR* GrindActorClassName;

	// Searches the static meshes on the actors (instanced ones included) for grind lines. Only these
	// actors carry lines, but the fall past each edge is traced against the whole world's collision.
	// With bCheckWalls, the space over and beside each edge is also tested against the triangles of
	// every collision-enabled static mesh nearby, which sees walls the traces cannot.
	FAutoGrindScanResult Scan(UWorld& World, const TArray<AActor*>& Actors, const AutoGrindCore::Settings& Settings, bool bCollectNearMisses, bool bCheckWalls = true);

	// LOD 0 of the mesh's source geometry in its own space, wound as AutoGrindCore::Mesh requires.
	// False when the mesh has no source geometry.
	bool ReadMesh(const UStaticMesh& Mesh, TArray<FVector3f>& OutPositions, TArray<uint32>& OutIndices);
}
