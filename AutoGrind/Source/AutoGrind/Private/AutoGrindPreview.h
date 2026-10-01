#pragma once

#include "CoreMinimal.h"
#include "AutoGrindScan.h"

namespace AutoGrind
{
	struct FPreviewStyle
	{
		float Thickness = 3;
		// Arrows along every line, not only the selected ones.
		bool bDirections = false;
	};

	// Kept stone lines are blue, kept rails red, unticked lines thin grey, selected lines yellow with arrows
	// showing their direction. Drawn on top of the level so a line lying exactly on an edge is not hidden by
	// it. Replaces the previous preview and leaves any other debug lines in the world alone.
	void DrawPreview(const UWorld& World, const TArray<TSharedPtr<FAutoGrindLine>>& Lines, const TArray<FAutoGrindNearMiss>& NearMisses, const TSet<const FAutoGrindLine*>& Selected, const FPreviewStyle& Style);

	void ClearPreview(const UWorld& World);

	// The colours lines are drawn in, also used by the panel.
	FLinearColor RailColour();
	FLinearColor StoneColour();
	FLinearColor ReviewColour();
	FLinearColor HighlightColour();
}
