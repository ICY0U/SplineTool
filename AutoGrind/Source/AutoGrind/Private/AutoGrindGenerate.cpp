#include "AutoGrindGenerate.h"

#include "ActorEditorUtils.h"
#include "AutoGrindSettings.h"
#include "Components/SplineComponent.h"
#include "Editor/EditorEngine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "AutoGrind"

namespace AutoGrind
{
	const TCHAR* GrindActorPath = TEXT("/Game/MainFolder/Blueprints/Grinding/GrindActor.GrindActor_C");
	const FName GeneratedTag(TEXT("AutoGrind"));
	const FName DrawnTag(TEXT("AutoGrindDrawn"));

	namespace
	{
		const FString SourceTagPrefix(TEXT("AutoGrindFrom:"));

		FName SourceTag(const AActor& Source)
		{
			return FName(SourceTagPrefix + Source.GetName());
		}

		bool ValidPoints(const TArray<FVector>& Points)
		{
			if (Points.Num() < 2)
			{
				return false;
			}
			for (int32 I = 0; I < Points.Num(); ++I)
			{
				if (Points[I].ContainsNaN() || (I > 0 && Points[I].Equals(Points[I - 1], 0.001)))
				{
					return false;
				}
			}
			return true;
		}

		void RecordRemoval(AActor& Actor)
		{
			Actor.Modify();
			// Imported Blueprint components can lack reconstruction data. Capture their actual state before
			// deletion, including garbage flags, instead of relying solely on UActorComponent::Modify
			// redirecting construction-script components to the actor.
			TInlineComponentArray<UActorComponent*> Components(&Actor);
			for (UActorComponent* Component : Components)
			{
				Component->SetFlags(RF_Transactional);
				Component->UObject::Modify();
			}
		}

		// The index of the enumerator named Wanted, by its name ("NewEnumerator0") or its display name ("Rail").
		int32 FindEnumerator(const UEnum& Enum, const FString& Wanted)
		{
			for (int32 I = 0; I < Enum.NumEnums(); ++I)
			{
				if (Enum.GetNameStringByIndex(I).Equals(Wanted, ESearchCase::IgnoreCase) || Enum.GetDisplayNameTextByIndex(I).ToString().Equals(Wanted, ESearchCase::IgnoreCase))
				{
					return I;
				}
			}
			return INDEX_NONE;
		}

		// A Blueprint enum variable is a byte property with an enum; a C++ enum class one is an enum property.
		bool HasGrindType(const UClass& Class, const FPlaceOptions& Options)
		{
			if (const FByteProperty* Byte = FindFProperty<FByteProperty>(&Class, Options.TypeProperty))
			{
				return Byte->Enum && FindEnumerator(*Byte->Enum, Options.RailTypeName) != INDEX_NONE && FindEnumerator(*Byte->Enum, Options.StoneTypeName) != INDEX_NONE;
			}
			if (const FEnumProperty* Enumerated = FindFProperty<FEnumProperty>(&Class, Options.TypeProperty))
			{
				const UEnum* Enum = Enumerated->GetEnum();
				return Enum && FindEnumerator(*Enum, Options.RailTypeName) != INDEX_NONE && FindEnumerator(*Enum, Options.StoneTypeName) != INDEX_NONE;
			}
			return false;
		}

		TArray<AActor*> Collect(UWorld& World, bool bScanned, bool bDrawn, const ULevel* OnlyLevel)
		{
			TArray<AActor*> Out;
			for (TActorIterator<AActor> It(&World); It; ++It)
			{
				if (!It->Tags.Contains(GeneratedTag) || (OnlyLevel && It->GetLevel() != OnlyLevel))
				{
					continue;
				}
				const bool bIsDrawn = It->Tags.Contains(DrawnTag);
				if ((bIsDrawn && bDrawn) || (!bIsDrawn && bScanned))
				{
					Out.Add(*It);
				}
			}
			return Out;
		}

		void Label(AActor& Actor, const FPlaceOptions& Options, const FString& Name)
		{
			FActorLabelUtilities::SetActorLabelUnique(&Actor, Options.LabelPrefix + Name);
		}
	}

	bool MakePlaceOptions(const UAutoGrindSettings& Settings, FPlaceOptions& Out, FString& OutError)
	{
		const FString Path = Settings.GrindActorClass.IsValid() ? Settings.GrindActorClass.ToString() : FString(GrindActorPath);
		Out.GrindClass = LoadClass<AActor>(nullptr, *Path);
		Out.TypeProperty = Settings.GrindTypeProperty;
		Out.RailTypeName = Settings.RailTypeName;
		Out.StoneTypeName = Settings.StoneTypeName;
		Out.Folder = Settings.OutputFolder.IsNone() ? FName(TEXT("AutoGrind")) : Settings.OutputFolder;
		Out.DrawnFolder = FName(*(Out.Folder.ToString() + TEXT("/Drawn")));
		Out.LabelPrefix = Settings.LabelPrefix;
		Out.HeightOffset = Settings.HeightOffset;
		Out.bCurvePoints = Settings.PointType == EAutoGrindPointType::Curve;
		if (!Out.GrindClass)
		{
			OutError = FString::Printf(TEXT("Cannot load the grind actor class %s. Placing needs Rollout Inline's GrindActor Blueprint at that path, or set Grind Actor Class in the settings."), *Path);
			return false;
		}
		if (!HasGrindType(*Out.GrindClass, Out))
		{
			OutError = FString::Printf(TEXT("%s has no %s enum with the enumerators %s and %s. Check Grind Type Property, Rail Type Name and Stone Type Name in the settings."),
				*Out.GrindClass->GetName(), *Out.TypeProperty.ToString(), *Out.RailTypeName, *Out.StoneTypeName);
			return false;
		}
		return true;
	}

	bool SetGrindType(AActor& Actor, const FPlaceOptions& Options, bool bRail)
	{
		const FString& Wanted = bRail ? Options.RailTypeName : Options.StoneTypeName;
		if (const FByteProperty* Byte = FindFProperty<FByteProperty>(Actor.GetClass(), Options.TypeProperty))
		{
			const int32 Index = Byte->Enum ? FindEnumerator(*Byte->Enum, Wanted) : INDEX_NONE;
			if (Index == INDEX_NONE)
			{
				return false;
			}
			Byte->SetPropertyValue_InContainer(&Actor, uint8(Byte->Enum->GetValueByIndex(Index)));
			return true;
		}
		if (const FEnumProperty* Enumerated = FindFProperty<FEnumProperty>(Actor.GetClass(), Options.TypeProperty))
		{
			const UEnum* Enum = Enumerated->GetEnum();
			const int32 Index = Enum ? FindEnumerator(*Enum, Wanted) : INDEX_NONE;
			FNumericProperty* Underlying = Enumerated->GetUnderlyingProperty();
			if (Index == INDEX_NONE || !Underlying)
			{
				return false;
			}
			Underlying->SetIntPropertyValue(Enumerated->ContainerPtrToValuePtr<void>(&Actor), Enum->GetValueByIndex(Index));
			return true;
		}
		return false;
	}

	AActor* PlaceGrindActor(UWorld& World, const FPlaceOptions& Options, const TArray<FVector>& InPoints, bool bRail)
	{
		if (!Options.GrindClass || !ValidPoints(InPoints))
		{
			return nullptr;
		}
		TArray<FVector> Points = InPoints;
		for (FVector& Point : Points)
		{
			Point.Z += Options.HeightOffset;
		}
		const FVector Along = Points[1] - Points[0];
		const FRotator Facing(0, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0);

		// As UEditorEngine::AddActor does it, less its dialogs: transactional, into the current level.
		FActorSpawnParameters Spawn;
		Spawn.OverrideLevel = World.GetCurrentLevel();
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Spawn.ObjectFlags = RF_Transactional;
		AActor* Actor = World.SpawnActor(Options.GrindClass, &Points[0], &Facing, Spawn);
		if (!Actor)
		{
			return nullptr;
		}
		if (!SetGrindType(*Actor, Options, bRail))
		{
			World.EditorDestroyActor(Actor, true);
			return nullptr;
		}

		// Hand-placed GrindActors keep their points because the spline is marked as edited; without it every
		// construction-script rerun would put the Blueprint's default rail back.
		USplineComponent* Spline = Actor->FindComponentByClass<USplineComponent>();
		if (!Spline)
		{
			World.EditorDestroyActor(Actor, true);
			return nullptr;
		}
		TArray<FVector> Local;
		for (const FVector& Point : Points)
		{
			Local.Add(Actor->GetActorTransform().InverseTransformPosition(Point));
		}
		// The game's construction script builds N-1 segments. Closed lines use a repeated endpoint, so keep the
		// component open to avoid an extra zero-length closing segment.
		Spline->SetClosedLoop(false, false);
		Spline->SetSplinePoints(Local, ESplineCoordinateSpace::Local, false);
		for (int32 I = 0; I < Local.Num(); ++I)
		{
			Spline->SetSplinePointType(I, Options.bCurvePoints ? ESplinePointType::Curve : ESplinePointType::Linear, false);
		}
		Spline->UpdateSpline();
		Spline->bSplineHasBeenEdited = true;
		Spline->bInputSplinePointsToConstructionScript = true;
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
		Actor->SetFolderPath(Options.Folder);
		Actor->InvalidateLightingCache();
		Actor->PostEditMove(true);
		Actor->MarkPackageDirty();
		return Actor;
	}

	FGenerateResult Generate(UWorld& World, const TArray<const FAutoGrindLine*>& Lines, const FPlaceOptions& Options)
	{
		FGenerateResult Result;
		if (Lines.IsEmpty())
		{
			return Result;
		}
		for (const FAutoGrindLine* Line : Lines)
		{
			if (!Line || !Line->MainSource() || !ValidPoints(Line->Points))
			{
				Result.Error = TEXT("A source actor was deleted or a line is invalid. Scan again before generating. Earlier output was kept.");
				return Result;
			}
		}
		if (!Options.GrindClass)
		{
			Result.Error = TEXT("No grind actor class to place. Earlier output was kept.");
			return Result;
		}

		FScopedTransaction Transaction(LOCTEXT("Generate", "AutoGrind: Generate Grind Lines"));
		TSet<FName> Sources;
		for (const FAutoGrindLine* Line : Lines)
		{
			for (const TWeakObjectPtr<AActor>& Source : Line->Sources)
			{
				if (const AActor* Actor = Source.Get())
				{
					Sources.Add(SourceTag(*Actor));
				}
			}
		}
		TArray<AActor*> Earlier;
		for (AActor* Actor : Collect(World, true, false, World.GetCurrentLevel()))
		{
			if (Actor->Tags.ContainsByPredicate([&Sources](const FName& Tag) { return Sources.Contains(Tag); }))
			{
				Earlier.Add(Actor);
			}
		}

		for (const FAutoGrindLine* Line : Lines)
		{
			AActor* Placed = PlaceGrindActor(World, Options, Line->Points, Line->bRail);
			if (!Placed)
			{
				for (AActor* Actor : Result.Actors)
				{
					World.EditorDestroyActor(Actor, true);
				}
				Transaction.Cancel();
				Result.Placed = 0;
				Result.Actors.Reset();
				Result.Error = TEXT("Could not make a valid grind actor spline and grind type. Earlier output was kept.");
				return Result;
			}
			for (const TWeakObjectPtr<AActor>& Source : Line->Sources)
			{
				if (const AActor* Actor = Source.Get())
				{
					Placed->Tags.AddUnique(SourceTag(*Actor));
				}
			}
			Label(*Placed, Options, Line->SourceLabel.Replace(TEXT(" "), TEXT("")));
			Result.Actors.Add(Placed);
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

	AActor* PlaceDrawnLine(UWorld& World, const TArray<FVector>& Points, bool bRail, const FPlaceOptions& Options, FString& OutError)
	{
		if (!ValidPoints(Points))
		{
			OutError = TEXT("A drawn line needs at least two different points.");
			return nullptr;
		}
		FScopedTransaction Transaction(LOCTEXT("Draw", "AutoGrind: Draw Grind Line"));
		AActor* Placed = PlaceGrindActor(World, Options, Points, bRail);
		if (!Placed)
		{
			Transaction.Cancel();
			OutError = TEXT("Could not make a valid grind actor spline and grind type.");
			return nullptr;
		}
		Placed->Tags.AddUnique(DrawnTag);
		Placed->SetFolderPath(Options.DrawnFolder);
		Label(*Placed, Options, bRail ? TEXT("DrawnRail") : TEXT("DrawnStone"));
		return Placed;
	}

	TArray<AActor*> FindGenerated(UWorld& World, bool bScanned, bool bDrawn)
	{
		return Collect(World, bScanned, bDrawn, nullptr);
	}

	int32 RemoveGenerated(UWorld& World, bool bScanned, bool bDrawn)
	{
		FScopedTransaction Transaction(LOCTEXT("Remove", "AutoGrind: Remove Generated"));
		const TArray<AActor*> Generated = Collect(World, bScanned, bDrawn, nullptr);
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
