#pragma once

#include "CoreMinimal.h"
#include "AutoGrindScan.h"

class UAutoGrindSettings;

namespace AutoGrind
{
	// The default GrindActor Blueprint of Rollout Inline, as the project's hand-placed ones use it.
	extern const TCHAR* GrindActorPath;
	// Every actor AutoGrind places carries GeneratedTag; lines drawn by hand also carry DrawnTag and are never
	// replaced by a scan's Generate.
	extern const FName GeneratedTag;
	extern const FName DrawnTag;

	// How grind actors are placed, read from the settings.
	struct FPlaceOptions
	{
		UClass* GrindClass = nullptr;
		FName TypeProperty;
		FString RailTypeName;
		FString StoneTypeName;
		FName Folder;
		FName DrawnFolder;
		FString LabelPrefix;
		double HeightOffset = 0;
		bool bCurvePoints = false;
	};

	// Loads the grind actor class and copies the rest. False, with the reason, when the class cannot be loaded
	// or has no grind type property.
	bool MakePlaceOptions(const UAutoGrindSettings& Settings, FPlaceOptions& Out, FString& OutError);

	struct FGenerateResult
	{
		int32 Placed = 0;
		int32 Replaced = 0;
		TArray<AActor*> Actors;
		FString Error;
	};

	// One undoable step: places a grind actor along each line in the world's current level, made like the
	// hand-placed ones (points in the actor's own space from its first point, the actor turned along its
	// first segment). Earlier output from the same source actors in that level is replaced, but only once
	// every new actor has been checked: on any failure nothing changes.
	FGenerateResult Generate(UWorld& World, const TArray<const FAutoGrindLine*>& Lines, const FPlaceOptions& Options);

	// One undoable step: places one grind actor along points drawn by hand. Null, with the reason, on failure.
	AActor* PlaceDrawnLine(UWorld& World, const TArray<FVector>& Points, bool bRail, const FPlaceOptions& Options, FString& OutError);

	// One undoable step: removes the grind actors AutoGrind placed in the world, those from scans, those drawn
	// by hand, or both. Hand-placed grind actors are never touched. Returns how many.
	int32 RemoveGenerated(UWorld& World, bool bScanned, bool bDrawn);

	// The grind actors AutoGrind placed in the world.
	TArray<AActor*> FindGenerated(UWorld& World, bool bScanned, bool bDrawn);

	// Places one grind actor along the world-space points, without a transaction of its own.
	AActor* PlaceGrindActor(UWorld& World, const FPlaceOptions& Options, const TArray<FVector>& Points, bool bRail);

	// Sets the actor's grind type to rail or stone. False when it has no such property or enumerator.
	bool SetGrindType(AActor& Actor, const FPlaceOptions& Options, bool bRail);
}
