#pragma once

#include "CoreMinimal.h"
#include "AutoGrindScan.h"

namespace AutoGrind
{
	// Rollout Inline's GrindActor, as the project's hand-placed ones use it.
	extern const TCHAR* GrindActorPath;
	// Every actor Generate places carries this tag and sits in this outliner folder.
	extern const FName GeneratedTag;
	extern const FName GeneratedFolder;

	struct FGenerateResult
	{
		int32 Placed = 0;
		int32 Replaced = 0;
		FString Error;
	};

	// One undoable step. Validates new actors before replacing earlier output for the same sources in the current level.
	// Places
	// GrindActors along the lines in the world's current level, made like the hand-placed ones: points
	// in the actor's own space from its first point, the actor turned along its first segment.
	FGenerateResult Generate(UWorld& World, const TArray<const FAutoGrindLine*>& Lines);

	// One undoable step: removes every GrindActor Generate placed in the world. Returns how many.
	int32 RemoveGenerated(UWorld& World);

	// Places one GrindActor along the world-space points, without a transaction of its own.
	AActor* PlaceGrindActor(UWorld& World, UClass& GrindClass, const TArray<FVector>& Points, bool bRail);
}
