#include "AutoGrindTestCommandlet.h"

#include "AutoGrindGenerate.h"
#include "AutoGrindPreview.h"
#include "AutoGrindScan.h"
#include "AutoGrindSettings.h"
#include "Components/LineBatchComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogAutoGrindTest, Display, All);

namespace AutoGrindTest
{
	AActor* SpawnMesh(UWorld& World, UStaticMesh* Mesh, const FTransform& Transform, const TCHAR* Name)
	{
		AActor* Actor = World.SpawnActor<AActor>();
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Actor, Name);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionProfileName(TEXT("BlockAll"));
		Actor->SetRootComponent(Component);
		Component->RegisterComponent();
		Actor->SetActorTransform(Transform);
		return Actor;
	}

	TArray<const FAutoGrindLine*> LinesOf(const FAutoGrindScanResult& Result, const AActor* Actor)
	{
		TArray<const FAutoGrindLine*> Out;
		for (const FAutoGrindLine& Line : Result.Lines)
		{
			if (Line.MainSource() == Actor)
			{
				Out.Add(&Line);
			}
		}
		return Out;
	}

	FAutoGrindScanResult ScanActors(UWorld& World, const TArray<AActor*>& Actors)
	{
		FAutoGrindScanRequest Request;
		Request.Actors = Actors;
		Request.Core = AutoGrindCore::Settings();
		Request.bCollectNearMisses = true;
		Request.bShowProgress = false;
		return AutoGrind::Scan(World, Request);
	}
}

int32 UAutoGrindTestCommandlet::Main(const FString& Params)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	checkf(Cube && Cylinder, TEXT("Engine basic shapes are missing"));

	// Winding: every face of the engine cube must face away from its centre, and the faces pointing
	// up must cover exactly its 100 x 100 top.
	{
		TArray<FVector3f> Positions;
		TArray<uint32> Indices;
		checkf(AutoGrind::ReadMesh(*Cube, Positions, Indices), TEXT("The engine cube has no mesh description"));
		const FVector3f Centre(Cube->GetBounds().Origin);
		double UpArea = 0;
		for (int32 I = 0; I + 2 < Indices.Num(); I += 3)
		{
			const FVector3f A = Positions[Indices[I]];
			const FVector3f B = Positions[Indices[I + 1]];
			const FVector3f C = Positions[Indices[I + 2]];
			const FVector3f Cross = FVector3f::CrossProduct(B - A, C - A);
			const FVector3f Normal = Cross.GetSafeNormal();
			checkf(FVector3f::DotProduct(Normal, (A + B + C) / 3 - Centre) > 0, TEXT("Cube triangle %d faces inward: ReadMesh winds triangles the wrong way"), I / 3);
			UpArea += Normal.Z > 0.99f ? Cross.Size() / 2 : 0;
		}
		checkf(FMath::IsNearlyEqual(UpArea, 10000.0, 1.0), TEXT("The cube's upward faces cover %.1f, expected 10000"), UpArea);
		UE_LOG(LogAutoGrindTest, Display, TEXT("WINDING_PASS: %d cube triangles all face outward"), Indices.Num() / 3);
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("AutoGrindTest")));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);

	// A floor with its top at 0; a 3 m x 1.2 m ledge box 45 cm high; the same box mirrored and turned;
	// a 10 cm thick, 4 m long rail floating with its top at 105.
	AutoGrindTest::SpawnMesh(*World, Cube, FTransform(FRotator::ZeroRotator, FVector(0, 0, -50), FVector(40, 40, 1)), TEXT("Floor"));
	AActor* Box = AutoGrindTest::SpawnMesh(*World, Cube, FTransform(FRotator::ZeroRotator, FVector(0, 0, 22.5), FVector(3, 1.2, 0.45)), TEXT("Box"));
	AActor* Mirrored = AutoGrindTest::SpawnMesh(*World, Cube, FTransform(FRotator(0, 30, 0), FVector(0, 600, 22.5), FVector(-3, 1.2, 0.45)), TEXT("Mirrored"));
	AActor* Rail = AutoGrindTest::SpawnMesh(*World, Cylinder, FTransform(FRotator(0, 0, 90), FVector(0, -600, 100), FVector(0.1, 0.1, 4)), TEXT("Rail"));
	// Two 1.5 m ledge modules end to end, as modular pieces are placed: their long sides are one line each.
	AActor* ModuleA = AutoGrindTest::SpawnMesh(*World, Cube, FTransform(FRotator::ZeroRotator, FVector(-75, 1200, 25), FVector(1.5, 0.6, 0.5)), TEXT("ModuleA"));
	AActor* ModuleB = AutoGrindTest::SpawnMesh(*World, Cube, FTransform(FRotator::ZeroRotator, FVector(75, 1200, 25), FVector(1.5, 0.6, 0.5)), TEXT("ModuleB"));
	// The cube bent along a straight 3 m spline: a 3 m x 1 m x 1 m ledge.
	AActor* Bent = World->SpawnActor<AActor>();
	{
		USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(Bent, TEXT("BentLedge"));
		SplineMesh->SetStaticMesh(Cube);
		SplineMesh->SetCollisionProfileName(TEXT("BlockAll"));
		SplineMesh->SetForwardAxis(ESplineMeshAxis::X, false);
		SplineMesh->SetStartAndEnd(FVector::ZeroVector, FVector(300, 0, 0), FVector(300, 0, 0), FVector(300, 0, 0), true);
		Bent->SetRootComponent(SplineMesh);
		SplineMesh->RegisterComponent();
		Bent->SetActorLocation(FVector(-150, 1800, 50));
	}
	World->UpdateWorldComponents(true, false);
	// Chaos adds new bodies to its query structure when the world ticks; the editor world ticks on its own.
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	World->Tick(LEVELTICK_All, 0.016f);

	FHitResult FloorHit;
	checkf(World->LineTraceSingleByChannel(FloorHit, FVector(1000, 1000, 500), FVector(1000, 1000, -500), ECC_Visibility, FCollisionQueryParams(NAME_None, true)) && FMath::IsNearlyEqual(FloorHit.ImpactPoint.Z, 0.0, 0.5),
		TEXT("The test floor has no collision, so no drop can be measured"));

	const FAutoGrindScanResult Result = AutoGrindTest::ScanActors(*World, {Box, Mirrored, Rail});
	for (const FAutoGrindLine& Line : Result.Lines)
	{
		UE_LOG(LogAutoGrindTest, Display, TEXT("line on %s: %s, %d points, length %.1f, drop %.1f, from %s to %s"), *Line.SourceLabel, Line.bRail ? TEXT("rail") : TEXT("stone"),
			Line.Points.Num(), Line.Length, Line.Drop, *Line.Points[0].ToString(), *Line.Points.Last().ToString());
	}
	checkf(Result.MeshCount == 3 && Result.Skipped.IsEmpty(), TEXT("Scanned %d meshes with %d skipped, expected 3 and none"), Result.MeshCount, Result.Skipped.Num());

	// Each box: its four top edges, as stone, lying on the top outline in the box's own space.
	for (const AActor* Each : {Box, Mirrored})
	{
		const TArray<const FAutoGrindLine*> Lines = AutoGrindTest::LinesOf(Result, Each);
		if (Lines.Num() != 4)
		{
			// Say why the top outline was cut before failing.
			const FBox Around = Each->GetComponentsBoundingBox().ExpandBy(5);
			for (const FAutoGrindNearMiss& Miss : Result.NearMisses)
			{
				if (Around.IsInside(Miss.A) && FMath::IsNearlyEqual(Miss.A.Z, 45.0, 1.0))
				{
					UE_LOG(LogAutoGrindTest, Display, TEXT("near miss on %s: %s, from %s to %s"), *Each->GetName(), *Miss.Text, *Miss.A.ToString(), *Miss.B.ToString());
				}
			}
		}
		checkf(Lines.Num() == 4, TEXT("%s: %d lines, expected its 4 top edges"), *Each->GetName(), Lines.Num());
		TArray<double> Lengths;
		for (const FAutoGrindLine* Line : Lines)
		{
			checkf(!Line->bRail, TEXT("%s: a box edge came out as a rail"), *Each->GetName());
			for (const FVector& Point : Line->Points)
			{
				const FVector Local = Each->GetActorTransform().InverseTransformPosition(Point);
				const bool bOnOutline = FMath::IsNearlyEqual(FMath::Abs(Local.X), 50.0, 0.5) || FMath::IsNearlyEqual(FMath::Abs(Local.Y), 50.0, 0.5);
				checkf(FMath::IsNearlyEqual(Point.Z, 45.0, 0.5) && bOnOutline, TEXT("%s: point %s is not on the top outline"), *Each->GetName(), *Point.ToString());
			}
			Lengths.Add(Line->Length);
		}
		Lengths.Sort();
		checkf(FMath::IsNearlyEqual(Lengths[0], 120.0, 1.0) && FMath::IsNearlyEqual(Lengths[1], 120.0, 1.0) && FMath::IsNearlyEqual(Lengths[2], 300.0, 1.0) && FMath::IsNearlyEqual(Lengths[3], 300.0, 1.0),
			TEXT("%s: edge lengths %.1f %.1f %.1f %.1f, expected 120 120 300 300"), *Each->GetName(), Lengths[0], Lengths[1], Lengths[2], Lengths[3]);
	}
	UE_LOG(LogAutoGrindTest, Display, TEXT("BOX_PASS: both boxes, the mirrored one included, give their four top edges as stone"));

	// The rail: one rail line along its crest.
	{
		const TArray<const FAutoGrindLine*> Lines = AutoGrindTest::LinesOf(Result, Rail);
		checkf(Lines.Num() == 1 && Lines[0]->bRail, TEXT("Rail: %d lines, expected one rail line"), Lines.Num());
		checkf(FMath::IsNearlyEqual(Lines[0]->Length, 400.0, 10.0), TEXT("Rail line is %.1f long, expected 400"), Lines[0]->Length);
		for (const FVector& Point : Lines[0]->Points)
		{
			checkf(FMath::IsNearlyEqual(Point.Z, 105.0, 1.0) && FMath::IsNearlyEqual(Point.X, 0.0, 1.0), TEXT("Rail point %s is not on the crest (x 0, z 105)"), *Point.ToString());
		}
	}
	UE_LOG(LogAutoGrindTest, Display, TEXT("RAIL_PASS: the rail gives one rail line on its crest"));

	// Modular pieces: one line along both modules' long sides, naming both as its sources.
	{
		const FAutoGrindScanResult Modular = AutoGrindTest::ScanActors(*World, {ModuleA, ModuleB});
		int32 Joined = 0;
		for (const FAutoGrindLine& Line : Modular.Lines)
		{
			UE_LOG(LogAutoGrindTest, Display, TEXT("modular line: %s, %.1f long, %d source(s)"), Line.bRail ? TEXT("rail") : TEXT("stone"), Line.Length, Line.Sources.Num());
			Joined += FMath::IsNearlyEqual(Line.Length, 300.0, 1.0) && Line.Sources.Num() == 2 && Line.Sources.Contains(ModuleA) && Line.Sources.Contains(ModuleB) ? 1 : 0;
		}
		checkf(Modular.Lines.Num() == 4 && Joined == 2, TEXT("Two touching modules gave %d lines, %d joined across both; expected 4 and 2"), Modular.Lines.Num(), Joined);
	}
	UE_LOG(LogAutoGrindTest, Display, TEXT("MODULAR_PASS: modules placed end to end give one line along both"));

	// A spline mesh is scanned as it is bent, not as its straight mesh.
	{
		const FAutoGrindScanResult Spline = AutoGrindTest::ScanActors(*World, {Bent});
		TArray<double> Lengths;
		for (const FAutoGrindLine& Line : Spline.Lines)
		{
			UE_LOG(LogAutoGrindTest, Display, TEXT("spline-mesh line: %.1f long from %s to %s"), Line.Length, *Line.Points[0].ToString(), *Line.Points.Last().ToString());
			for (const FVector& Point : Line.Points)
			{
				checkf(FMath::IsNearlyEqual(Point.Z, 100.0, 0.5), TEXT("Spline-mesh line point %s is not on the bent ledge's top"), *Point.ToString());
			}
			Lengths.Add(Line.Length);
		}
		Lengths.Sort();
		checkf(Lengths.Num() == 4 && FMath::IsNearlyEqual(Lengths[0], 100.0, 1.0) && FMath::IsNearlyEqual(Lengths[3], 300.0, 1.0), TEXT("The bent ledge gave %d lines, expected its 4 top edges"), Lengths.Num());
	}
	UE_LOG(LogAutoGrindTest, Display, TEXT("SPLINE_MESH_PASS: a spline mesh is scanned as it is bent"));

	// The preview: drawn into the persistent batcher and cleared again without touching other lines.
	{
		ULineBatchComponent* Lines = World->PersistentLineBatcher;
		checkf(Lines, TEXT("The world has no persistent line batcher"));
		Lines->DrawLine(FVector::ZeroVector, FVector(0, 0, 100), FLinearColor::White, SDPG_World, 1, 0);
		TArray<TSharedPtr<FAutoGrindLine>> Shared;
		int32 Segments = 0;
		for (const FAutoGrindLine& Line : Result.Lines)
		{
			Shared.Add(MakeShared<FAutoGrindLine>(Line));
			Segments += Line.Points.Num() - 1;
		}
		// Without the arrows a selected line gets, each line segment is one batched line.
		AutoGrind::FPreviewStyle Style;
		AutoGrind::DrawPreview(*World, Shared, {}, {}, Style);
		checkf(Lines->BatchedLines.Num() == Segments + 1, TEXT("Preview drew %d lines, expected %d plus the existing one"), Lines->BatchedLines.Num() - 1, Segments);
		AutoGrind::DrawPreview(*World, Shared, {}, {Shared[0].Get()}, Style);
		checkf(Lines->BatchedLines.Num() >= Segments + 1, TEXT("Highlighting a line lost preview lines: %d"), Lines->BatchedLines.Num());
		AutoGrind::DrawPreview(*World, Shared, {}, {}, Style);
		checkf(Lines->BatchedLines.Num() == Segments + 1, TEXT("Redrawing the preview did not replace it: %d lines"), Lines->BatchedLines.Num());
		AutoGrind::ClearPreview(*World);
		checkf(Lines->BatchedLines.Num() == 1 && Lines->BatchedPoints.Num() == 0, TEXT("Clearing left %d lines and %d points; only the unrelated line should remain"), Lines->BatchedLines.Num(), Lines->BatchedPoints.Num());
	}
	UE_LOG(LogAutoGrindTest, Display, TEXT("PREVIEW_PASS: the preview draws, redraws in place and clears only its own lines"));

	// Generate: one GrindActor per line, made like the hand-placed ones, riding the grind channel.
	{
		TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
		// Commandlets start without the editor's undo buffer; make it as editor startup does.
		checkf(GEditor, TEXT("No editor engine"));
		if (!GEditor->Trans)
		{
			GEditor->Trans = GEditor->CreateTrans();
		}
		TArray<const FAutoGrindLine*> All;
		for (const FAutoGrindLine& Line : Result.Lines)
		{
			All.Add(&Line);
		}
		auto Generated = [World]
		{
			TArray<AActor*> Out;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->Tags.Contains(AutoGrind::GeneratedTag))
				{
					Out.Add(*It);
				}
			}
			return Out;
		};

		AutoGrind::FPlaceOptions Options;
		FString OptionsError;
		checkf(AutoGrind::MakePlaceOptions(*GetDefault<UAutoGrindSettings>(), Options, OptionsError), TEXT("%s"), *OptionsError);
		const AutoGrind::FGenerateResult First = AutoGrind::Generate(*World, All, Options);
		checkf(First.Error.IsEmpty() && First.Placed == All.Num() && First.Replaced == 0, TEXT("Generate placed %d of %d lines, replaced %d: %s"), First.Placed, All.Num(), First.Replaced, *First.Error);
		World->Tick(LEVELTICK_All, 0.016f);
		const TArray<AActor*> Placed = Generated();
		checkf(Placed.Num() == All.Num(), TEXT("%d generated actors in the world, expected %d"), Placed.Num(), All.Num());
		const FByteProperty* Type = FindFProperty<FByteProperty>(Placed[0]->GetClass(), TEXT("GrindType"));
		checkf(Type && Type->Enum, TEXT("The GrindActor has no GrindType enum"));
		for (const FAutoGrindLine* Line : All)
		{
			// The actor for this line is the one whose first spline point is the line's first point.
			AActor* const* Found = Placed.FindByPredicate([Line](const AActor* Actor) { return Actor->GetActorLocation().Equals(Line->Points[0], 0.1); });
			checkf(Found, TEXT("No generated actor starts at %s"), *Line->Points[0].ToString());
			const AActor& Actor = **Found;
			const FString TypeName = Type->Enum->GetNameStringByValue(Type->GetPropertyValue_InContainer(&Actor));
			checkf(TypeName == (Line->bRail ? TEXT("NewEnumerator0") : TEXT("NewEnumerator1")), TEXT("%s: GrindType %s for a %s"), *Actor.GetActorNameOrLabel(), *TypeName, Line->bRail ? TEXT("rail") : TEXT("stone"));
			checkf(FMath::IsNearlyZero(Actor.GetActorRotation().Pitch) && FMath::IsNearlyZero(Actor.GetActorRotation().Roll), TEXT("%s is tilted"), *Actor.GetActorNameOrLabel());
			checkf(Actor.GetFolderPath() == Options.Folder && Actor.Tags.Contains(FName(TEXT("Grind"))), TEXT("%s: folder %s, tags missing"), *Actor.GetActorNameOrLabel(), *Actor.GetFolderPath().ToString());
			const USplineComponent* Spline = Actor.FindComponentByClass<USplineComponent>();
			checkf(Spline && Spline->bSplineHasBeenEdited && Spline->GetNumberOfSplinePoints() == Line->Points.Num(), TEXT("%s: spline missing, unedited or the wrong length"), *Actor.GetActorNameOrLabel());
			for (int32 I = 0; I < Line->Points.Num(); ++I)
			{
				checkf(Spline->GetLocationAtSplinePoint(I, ESplineCoordinateSpace::World).Equals(Line->Points[I], 0.1), TEXT("%s: spline point %d is off the line"), *Actor.GetActorNameOrLabel(), I);
			}
			TInlineComponentArray<USplineMeshComponent*> Meshes(&Actor);
			checkf(Meshes.Num() == Line->Points.Num() - 1, TEXT("%s: %d grind meshes for %d segments"), *Actor.GetActorNameOrLabel(), Meshes.Num(), Line->Points.Num() - 1);
			for (const USplineMeshComponent* Mesh : Meshes)
			{
				checkf(Mesh->Mobility == EComponentMobility::Movable && Mesh->GetCollisionEnabled() == ECollisionEnabled::QueryOnly && Mesh->GetCollisionResponseToChannel(ECC_GameTraceChannel2) == ECR_Block,
					TEXT("%s: grind mesh %s is not a movable, query-only grind-channel blocker"), *Actor.GetActorNameOrLabel(), *Mesh->GetName());
			}
			// The game finds a rail by tracing down the grind channel: every segment must answer. The test
			// shapes are BlockAll, which here also blocks the grind channel (this project lacks the retail
			// GrindCollision declaration), so the trace passes through them as it would through level geometry.
			FCollisionQueryParams GrindQuery;
			GrindQuery.AddIgnoredActors(TArray<AActor*>{Box, Mirrored, Rail});
			for (int32 I = 0; I + 1 < Line->Points.Num(); ++I)
			{
				const FVector Middle = (Line->Points[I] + Line->Points[I + 1]) / 2;
				FHitResult Hit;
				checkf(World->LineTraceSingleByChannel(Hit, Middle + FVector(0, 0, 20), Middle - FVector(0, 0, 40), ECC_GameTraceChannel2, GrindQuery) && Hit.GetActor() == &Actor,
					TEXT("%s: a grind-channel trace at %s missed it"), *Actor.GetActorNameOrLabel(), *Middle.ToString());
			}
		}
		UE_LOG(LogAutoGrindTest, Display, TEXT("GENERATE_PASS: %d GrindActors made like the hand-placed ones, every segment answering a grind trace"), Placed.Num());
	}

	// Undo, in an editor world like the level editor's: only there does destroying an actor record it
	// for undo (UWorld::DestroyActor calls Modify only outside game worlds). No traces are needed here.
	{
		TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
		UWorld* EditorWorld = UWorld::CreateWorld(EWorldType::Editor, false, FName(TEXT("AutoGrindUndoTest")));
		FWorldContext& EditorContext = GEngine->CreateNewWorldContext(EWorldType::Editor);
		EditorContext.SetCurrentWorld(EditorWorld);
		TArray<const FAutoGrindLine*> All;
		for (const FAutoGrindLine& Line : Result.Lines)
		{
			All.Add(&Line);
		}
		auto Generated = [EditorWorld]
		{
			TArray<AActor*> Out;
			for (TActorIterator<AActor> It(EditorWorld); It; ++It)
			{
				if (It->Tags.Contains(AutoGrind::GeneratedTag))
				{
					Out.Add(*It);
				}
			}
			return Out;
		};
		AutoGrind::FPlaceOptions Options;
		FString OptionsError;
		checkf(AutoGrind::MakePlaceOptions(*GetDefault<UAutoGrindSettings>(), Options, OptionsError), TEXT("%s"), *OptionsError);
		const AutoGrind::FGenerateResult First = AutoGrind::Generate(*EditorWorld, All, Options);
		const TArray<AActor*> Placed = Generated();
		checkf(First.Placed == All.Num() && Placed.Num() == All.Num(), TEXT("Editor-world Generate placed %d, found %d"), First.Placed, Placed.Num());
		FAutoGrindLine Invalid = *All[0];
		Invalid.Points[1] = Invalid.Points[0];
		const AutoGrind::FGenerateResult Failed = AutoGrind::Generate(*EditorWorld, {&Invalid}, Options);
		checkf(!Failed.Error.IsEmpty() && Failed.Placed == 0 && Failed.Replaced == 0 && Generated().Contains(Placed[0]) && Generated().Num() == All.Num(), TEXT("Invalid replacement changed previous output"));
		Invalid = *All[0];
		Invalid.Sources.Reset();
		checkf(!AutoGrind::Generate(*EditorWorld, {&Invalid}, Options).Error.IsEmpty() && Generated().Num() == All.Num(), TEXT("Deleted source must fail without changing output"));
		AutoGrind::FPlaceOptions Incompatible = Options;
		Incompatible.GrindClass = AActor::StaticClass();
		checkf(!AutoGrind::PlaceGrindActor(*EditorWorld, Incompatible, All[0]->Points, true), TEXT("An incompatible actor class must not count as a successful grind"));
		UE_LOG(LogAutoGrindTest, Display, TEXT("FAILURE_PRESERVATION_PASS: invalid geometry, deleted sources and incompatible actors are rejected"));
		const AutoGrind::FGenerateResult Second = AutoGrind::Generate(*EditorWorld, All, Options);
		checkf(Second.Placed == All.Num() && Second.Replaced == All.Num() && Generated().Num() == All.Num(), TEXT("Generating again left %d actors (placed %d, replaced %d)"), Generated().Num(), Second.Placed, Second.Replaced);
		checkf(GEditor->UndoTransaction(), TEXT("Undo of the second Generate failed"));
		const TArray<AActor*> Restored = Generated();
		checkf(Restored.Num() == All.Num() && Restored.Contains(Placed[0]), TEXT("Undo left %d actors, not the first Generate's %d"), Restored.Num(), All.Num());
		auto CheckRestoredSplines = [&All, &Generated]()
		{
			for (AActor* Actor : Generated())
			{
				const USplineComponent* Spline = Actor->FindComponentByClass<USplineComponent>();
				checkf(IsValid(Spline) && Spline->GetNumberOfSplinePoints() >= 2, TEXT("Undo/redo restored an actor without a usable spline"));
				bool bMatches = false;
				for (const FAutoGrindLine* Line : All)
				{
					bool bSame = Spline->GetNumberOfSplinePoints() == Line->Points.Num();
					for (int32 I = 0; bSame && I < Line->Points.Num(); ++I)
						bSame = Spline->GetLocationAtSplinePoint(I, ESplineCoordinateSpace::World).Equals(Line->Points[I], 0.1);
					bMatches |= bSame;
				}
				checkf(bMatches, TEXT("Undo/redo changed the generated spline geometry"));
				TInlineComponentArray<USplineMeshComponent*> Meshes(Actor);
				checkf(Meshes.Num() == Spline->GetNumberOfSplinePoints() - 1, TEXT("Undo/redo lost generated grind meshes"));
			}
		};
		CheckRestoredSplines();
		checkf(GEditor->RedoTransaction() && Generated().Num() == All.Num(), TEXT("Redo of replacement failed"));
		CheckRestoredSplines();
		checkf(GEditor->UndoTransaction() && Generated().Contains(Placed[0]), TEXT("Second undo of replacement failed"));
		CheckRestoredSplines();
		// A line drawn by hand survives a scan's Generate and Remove Scanned Lines.
		FString DrawError;
		AActor* Drawn = AutoGrind::PlaceDrawnLine(*EditorWorld, All[0]->Points, false, Options, DrawError);
		checkf(Drawn && Drawn->Tags.Contains(AutoGrind::DrawnTag), TEXT("Drawing a line failed: %s"), *DrawError);
		checkf(AutoGrind::Generate(*EditorWorld, All, Options).Error.IsEmpty() && IsValid(Drawn) && Generated().Contains(Drawn), TEXT("Generate replaced a drawn line"));
		checkf(AutoGrind::RemoveGenerated(*EditorWorld, true, false) == All.Num() && Generated().Num() == 1 && Generated()[0] == Drawn, TEXT("Remove Scanned Lines must keep the drawn line"));
		checkf(GEditor->UndoTransaction() && Generated().Num() == All.Num() + 1, TEXT("Undo of Remove Scanned Lines failed"));
		checkf(GEditor->UndoTransaction() && GEditor->UndoTransaction() && Generated().Num() == All.Num(), TEXT("Undo of the drawn line failed"));
		UE_LOG(LogAutoGrindTest, Display, TEXT("DRAWN_PASS: lines drawn by hand are kept apart from scanned ones"));
		checkf(AutoGrind::RemoveGenerated(*EditorWorld, true, true) == All.Num() && Generated().Num() == 0, TEXT("Remove Generated left %d actors"), Generated().Num());
		checkf(GEditor->UndoTransaction() && Generated().Num() == All.Num(), TEXT("Undo of Remove Generated left %d actors"), Generated().Num());
		CheckRestoredSplines();
		GEngine->DestroyWorldContext(EditorWorld);
		EditorWorld->DestroyWorld(false);
		UE_LOG(LogAutoGrindTest, Display, TEXT("UNDO_PASS: generating again replaces only its own actors, and Ctrl+Z steps back through Generate and Remove"));
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	UE_LOG(LogAutoGrindTest, Display, TEXT("AUTOGRIND_TEST_PASS"));
	return 0;
}
