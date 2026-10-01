#pragma once

#include "CoreMinimal.h"
#include "AutoGrindScan.h"

namespace AutoGrind
{
	// Stone lines are blue, rails red, unticked lines grey, the highlighted line yellow. Drawn on top of
	// the level so a line lying exactly on an edge is not hidden by it. Replaces the previous preview
	// and leaves any other debug lines in the world alone.
	void DrawPreview(const UWorld& World, const TArray<TSharedPtr<FAutoGrindLine>>& Lines, const TArray<FAutoGrindNearMiss>& NearMisses, const FAutoGrindLine* Highlighted);

	void ClearPreview(const UWorld& World);
}
