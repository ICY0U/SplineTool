#include "AutoGrindGenerate.h"

#include "Components/SplineComponent.h"
#include "Editor/EditorEngine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "AutoGrind"

namespace AutoGrind
{
	const TCHAR* GrindActorPath = TEXT("/Game/MainFolder/Blueprints/Grinding/GrindActor.GrindActor_C");
	const FName GeneratedTag(TEXT("AutoGrind"));
	const FName GeneratedFolder(TEXT("AutoGrind"));

	namespace
	{
		const FString SourceTagPrefix(TEXT("AutoGrindFrom:"));

		FName SourceTag(const AActor& Source)
		{
			return FName(SourceTagPrefix + Source.GetName());
		}

		bool ValidPoints(const TArray<FVector>& Points)
		{
			if (Points.Num() < 2) return false;
			for (int32 I = 0; I < Points.Num(); ++I)
			{
				if (Points[I].ContainsNaN() || (I > 0 && Points[I].Equals(Points[I - 1], 0.001))) return false;
			}
			return true;
		}

		void RecordRemoval(AActor& Actor)
		{
			Actor.Modify();
			// Imported Blueprint components can lack reconstruction data. Capture their actual
			// state before deletion, including garbage flags, instead of relying solely on
			// UActorComponent::Modify redirecting construction-script components to the actor.
			TInlineComponentArray<UActorComponent*> Components(&Actor);
			for (UActorComponent* Component : Components)
			{
				Component->SetFlags(RF_Transactional);
				Component->UObject::Modify();
			}
		}

		// The GrindType enum is a user-defined enum: NewEnumerator0 is a rail, NewEnumerator1 stone.
		bool SetGrindType(AActor& Actor, bool bRail)
		{
			const FByteProperty* Type = FindFProperty<FByteProperty>(Actor.GetClass(), TEXT("GrindType"));
			if (!Type || !Type->Enum)
			{
				return false;
			}
			const FString Wanted = bRail ? TEXT("NewEnumerator0") : TEXT("NewEnumerator1");
			for (int32 I = 0; I < Type->Enum->NumEnums(); ++I)
			{
				if (Type->Enum->GetNameStringByIndex(I) == Wanted)
				{
					Type->SetPropertyValue_InContainer(&Actor, uint8(Type->Enum->GetValueByIndex(I)));
					return true;
				}
			}
			return false;
		}
	}

	AActor* PlaceGrindActor(UWorld& World, UClass& GrindClass, const TArray<FVector>& Points, bool bRail)
	{
		if (!ValidPoints(Points))
		{
			return nullptr;
		}
		const FVector Along = Points[1] - Points[0];
		const FRotator Facing(0, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0);

		// As UEditorEngine::AddActor does it, less its dialogs: transactional, into the current level.
		FActorSpawnParameters Spawn;
		Spawn.OverrideLevel = World.GetCurrentLevel();
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Spawn.ObjectFlags = RF_Transactional;
		AActor* Actor = World.SpawnActor(&GrindClass, &Points[0], &Facing, Spawn);
		if (!Actor)
		{
			return nullptr;
		}
		if (!SetGrindType(*Actor, bRail))
		{
			World.EditorDestroyActor(Actor, true);
			return nullptr;
		}

		// Hand-placed GrindActors keep their points because the spline is marked as edited; without it
		// every construction-script rerun would put the Blueprint's default rail back.
		USplineComponent* Spline = Actor->FindComponentByClass<USplineComponent>();
		if (!Spline)
		{
			World.EditorDestroyActor(Actor, true);
			return nullptr;
		}
		if (Spline)
		{
			TArray<FVector> Local;
			for (const FVector& Point : Points)
			{
				Local.Add(Actor->GetActorTransform().InverseTransformPosition(Point));
			}
			// The game's construction script builds N-1 segments. Closed lines use a repeated
			// endpoint, so keep the component open to avoid an extra zero-length closing segment.
			Spline->SetClosedLoop(false, false);
			Spline->SetSplinePoints(Local, ESplineCoordinateSpace::Local, false);
			for (int32 I = 0; I < Local.Num(); ++I)
			{
				Spline->SetSplinePointType(I, ESplinePointType::Linear, false);
			}
			Spline->UpdateSpline();
			Spline->bSplineHasBeenEdited = true;
			Spline->bInputSplinePointsToConstructionScript = true;
		}
		// The construction script builds the grind mesh from the spline, as it does when a spline is edited by hand.
		Actor->RerunConstructionScripts();
		Spline = Actor->FindComponentByClass<USplineComponent>();
		bool bMatches = Spline && Spline->GetNumberOfSplinePoints() == Points.Num();
		for (int32 I = 0; bMatches && I < Points.Num(); ++I)
		{
			bMatches = Spline->GetLocationAtSplinePoint(I, ESplineCoordinateSpace::World).Equals(Points[I], 0.1);
		}
		if (!bMatches)
		{
			World.EditorDestroyActor(Actor, true);
			return nullptr;
		}

		Actor->Tags.AddUnique(GeneratedTag);
		Actor->SetFolderPath(GeneratedFolder);
		Actor->InvalidateLightingCache();
		Actor->PostEditMove(true);
		Actor->MarkPackageDirty();
		return Actor;
	}

	FGenerateResult Generate(UWorld& World, const TArray<const FAutoGrindLine*>& Lines)
	{
		FGenerateResult Result;
		if (Lines.IsEmpty()) return Result;
		for (const FAutoGrindLine* Line : Lines)
		{
			if (!Line || !Line->Source.IsValid() || !ValidPoints(Line->Points))
			{
				Result.Error = TEXT("A source was deleted or a line is invalid. Scan again before generating. Previous output was preserved.");
				return Result;
			}
		}
		UClass* GrindClass = LoadClass<AActor>(nullptr, GrindActorPath);
		if (!GrindClass)
		{
			Result.Error = FString::Printf(TEXT("Cannot load %s: the project needs the GrindActor Blueprint at that path."), GrindActorPath);
			return Result;
		}

		FScopedTransaction Transaction(LOCTEXT("Generate", "AutoGrind: Generate Grind Lines"));
		TSet<FName> Sources;
		for (const FAutoGrindLine* Line : Lines)
		{
			if (const AActor* Source = Line->Source.Get())
			{
				Sources.Add(SourceTag(*Source));
			}
		}
		TArray<AActor*> Earlier;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			if (It->GetLevel() == World.GetCurrentLevel() && It->Tags.Contains(GeneratedTag) && It->Tags.ContainsByPredicate([&Sources](const FName& Tag) { return Sources.Contains(Tag); }))
			{
				Earlier.Add(*It);
			}
		}
		TArray<AActor*> Created;

		for (const FAutoGrindLine* Line : Lines)
		{
			AActor* Placed = PlaceGrindActor(World, *GrindClass, Line->Points, Line->bRail);
			if (!Placed)
			{
				for (AActor* Actor : Created) World.EditorDestroyActor(Actor, true);
				Transaction.Cancel();
				Result.Placed = 0;
				Result.Error = TEXT("Could not create a valid GrindActor spline and grind type. Previous output was preserved.");
				return Result;
			}
			Created.Add(Placed);
			if (const AActor* Source = Line->Source.Get())
			{
				Placed->Tags.Add(SourceTag(*Source));
				FActorLabelUtilities::SetActorLabelUnique(Placed, FString::Printf(TEXT("AutoGrind_%s"), *Line->SourceLabel));
			}
			++Result.Placed;
		}
		for (AActor* Actor : Earlier)
		{
			RecordRemoval(*Actor);
			World.EditorDestroyActor(Actor, true);
		}
		Result.Replaced = Earlier.Num();
		return Result;
	}

	int32 RemoveGenerated(UWorld& World)
	{
		FScopedTransaction Transaction(LOCTEXT("Remove", "AutoGrind: Remove Generated"));
		TArray<AActor*> Generated;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			if (It->Tags.Contains(GeneratedTag))
			{
				Generated.Add(*It);
			}
		}
		for (AActor* Actor : Generated)
		{
			RecordRemoval(*Actor);
			World.EditorDestroyActor(Actor, true);
		}
		if (Generated.IsEmpty())
		{
			Transaction.Cancel();
		}
		return Generated.Num();
	}
}

#undef LOCTEXT_NAMESPACE
