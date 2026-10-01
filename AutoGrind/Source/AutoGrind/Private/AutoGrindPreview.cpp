#include "AutoGrindPreview.h"

#include "Components/LineBatchComponent.h"
#include "Engine/World.h"

namespace AutoGrind
{
	namespace
	{
		// Every preview line carries this batch, so clearing removes AutoGrind's lines and nothing else.
		constexpr uint32 PreviewBatch = 0x41475244; // "AGRD"
		// Arrows along a line, this far apart and this big.
		constexpr double ArrowSpacing = 100;
		constexpr double ArrowSize = 6;

		const FLinearColor NearMissColour(FColor(150, 150, 150));

		// The persistent batcher, not the foreground one: the editor viewport flushes the foreground
		// batcher after every frame it draws (FEditorViewportClient::Draw). Each line still carries its
		// own depth priority, so these draw on top of the level. A lifetime of 0 never expires.
		ULineBatchComponent* Batcher(const UWorld& World)
		{
			return World.PersistentLineBatcher;
		}

		void DrawArrows(ULineBatchComponent& Batch, const TArray<FVector>& Points, const FLinearColor& Colour, float Thickness)
		{
			double Walked = 0;
			double Next = ArrowSpacing / 2;
			for (int32 I = 0; I + 1 < Points.Num(); ++I)
			{
				const FVector Along = Points[I + 1] - Points[I];
				const double Length = Along.Size();
				if (Length <= KINDA_SMALL_NUMBER)
				{
					continue;
				}
				const FVector Direction = Along / Length;
				FVector Side = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
				if (Side.IsNearlyZero())
				{
					Side = FVector::RightVector;
				}
				while (Next <= Walked + Length)
				{
					const FVector Tip = Points[I] + Direction * (Next - Walked) + FVector(0, 0, 1);
					const FVector Back = Tip - Direction * ArrowSize;
					Batch.DrawLine(Tip, Back + Side * ArrowSize * 0.6, Colour, SDPG_Foreground, Thickness, 0, PreviewBatch);
					Batch.DrawLine(Tip, Back - Side * ArrowSize * 0.6, Colour, SDPG_Foreground, Thickness, 0, PreviewBatch);
					Next += ArrowSpacing;
				}
				Walked += Length;
			}
		}

		void DrawPolyline(ULineBatchComponent& Batch, const FAutoGrindLine& Line, const FLinearColor& Colour, float Thickness, bool bArrows)
		{
			for (int32 I = 0; I + 1 < Line.Points.Num(); ++I)
			{
				Batch.DrawLine(Line.Points[I], Line.Points[I + 1], Colour, SDPG_Foreground, Thickness, 0, PreviewBatch);
			}
			if (Line.Points.Num() > 0 && !Line.bClosed)
			{
				Batch.DrawPoint(Line.Points[0], Colour, Thickness * 3, SDPG_Foreground, 0, PreviewBatch);
				Batch.DrawPoint(Line.Points.Last(), Colour, Thickness * 3, SDPG_Foreground, 0, PreviewBatch);
			}
			if (bArrows)
			{
				DrawArrows(Batch, Line.Points, Colour, FMath::Max(1.0f, Thickness * 0.6f));
			}
		}
	}

	FLinearColor RailColour() { return FLinearColor(FColor(255, 70, 60)); }
	FLinearColor StoneColour() { return FLinearColor(FColor(40, 150, 255)); }
	FLinearColor ReviewColour() { return FLinearColor(FColor(110, 110, 110)); }
	FLinearColor HighlightColour() { return FLinearColor(FColor(255, 220, 0)); }

	void DrawPreview(const UWorld& World, const TArray<TSharedPtr<FAutoGrindLine>>& Lines, const TArray<FAutoGrindNearMiss>& NearMisses, const TSet<const FAutoGrindLine*>& Selected, const FPreviewStyle& Style)
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
		// Unticked lines first, so kept lines draw over them where they meet.
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (const TSharedPtr<FAutoGrindLine>& Line : Lines)
			{
				if (!Line || Selected.Contains(Line.Get()) || Line->bKeep != (Pass == 1))
				{
					continue;
				}
				const FLinearColor Colour = !Line->bKeep ? ReviewColour() : Line->bRail ? RailColour() : StoneColour();
				DrawPolyline(*Target, *Line, Colour, Line->bKeep ? Style.Thickness : FMath::Max(1.0f, Style.Thickness * 0.6f), Style.bDirections && Line->bKeep);
			}
		}
		for (const TSharedPtr<FAutoGrindLine>& Line : Lines)
		{
			if (Line && Selected.Contains(Line.Get()))
			{
				DrawPolyline(*Target, *Line, HighlightColour(), Style.Thickness * 2, true);
			}
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
