#include "AutoGrindPreview.h"

#include "Components/LineBatchComponent.h"
#include "Engine/World.h"

namespace AutoGrind
{
	namespace
	{
		// Every preview line carries this batch, so clearing removes AutoGrind's lines and nothing else.
		constexpr uint32 PreviewBatch = 0x41475244; // "AGRD"

		const FLinearColor StoneColour(FColor(40, 150, 255));
		const FLinearColor RailColour(FColor(255, 70, 60));
		const FLinearColor UntickedColour(FColor(90, 90, 90));
		const FLinearColor HighlightColour(FColor(255, 220, 0));
		const FLinearColor NearMissColour(FColor(150, 150, 150));

		// The persistent batcher, not the foreground one: the editor viewport flushes the foreground
		// batcher after every frame it draws (FEditorViewportClient::Draw). Each line still carries its
		// own depth priority, so these draw on top of the level. A lifetime of 0 never expires.
		ULineBatchComponent* Batcher(const UWorld& World)
		{
			return World.PersistentLineBatcher;
		}

		void DrawPolyline(ULineBatchComponent& Lines, const TArray<FVector>& Points, const FLinearColor& Colour, float Thickness)
		{
			for (int32 I = 0; I + 1 < Points.Num(); ++I)
			{
				Lines.DrawLine(Points[I], Points[I + 1], Colour, SDPG_Foreground, Thickness, 0, PreviewBatch);
			}
			if (Points.Num() > 0)
			{
				Lines.DrawPoint(Points[0], Colour, Thickness * 3, SDPG_Foreground, 0, PreviewBatch);
				Lines.DrawPoint(Points.Last(), Colour, Thickness * 3, SDPG_Foreground, 0, PreviewBatch);
			}
		}
	}

	void DrawPreview(const UWorld& World, const TArray<TSharedPtr<FAutoGrindLine>>& Lines, const TArray<FAutoGrindNearMiss>& NearMisses, const FAutoGrindLine* Highlighted)
	{
		ClearPreview(World);
		ULineBatchComponent* Target = Batcher(World);
		if (!Target)
		{
			return;
		}
		for (const FAutoGrindNearMiss& Miss : NearMisses)
		{
			Target->DrawLine(Miss.A, Miss.B, NearMissColour, SDPG_Foreground, 1, 0, PreviewBatch);
		}
		for (const TSharedPtr<FAutoGrindLine>& Line : Lines)
		{
			if (Line.Get() != Highlighted)
			{
				DrawPolyline(*Target, Line->Points, !Line->bKeep ? UntickedColour : Line->bRail ? RailColour : StoneColour, 3);
			}
		}
		if (Highlighted)
		{
			DrawPolyline(*Target, Highlighted->Points, HighlightColour, 6);
		}
	}

	void ClearPreview(const UWorld& World)
	{
		if (ULineBatchComponent* Target = Batcher(World))
		{
			Target->ClearBatch(PreviewBatch);
		}
	}
}
