#include "AutoGrindScan.h"

#include "AutoGrindSettings.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "MeshDescription.h"
#include "Misc/ScopedSlowTask.h"
#include "StaticMeshAttributes.h"

#define LOCTEXT_NAMESPACE "AutoGrind"

AActor* FAutoGrindLine::MainSource() const
{
	return Sources.Num() > 0 ? Sources[0].Get() : nullptr;
}

FText FAutoGrindLine::Describe() const
{
	FString Text = bRail ? TEXT("Rail") : TEXT("Stone");
	switch (Shape)
	{
	case AutoGrindCore::LineShape::Crest:
		Text += FString::Printf(TEXT(", along the crest of a %.0f cm top"), TopWidth);
		if (Thickness > 0)
		{
			Text += FString::Printf(TEXT(" on a body %.0f cm deep"), Thickness);
		}
		break;
	case AutoGrindCore::LineShape::Ridge:
		Text += TEXT(", along a ridge");
		break;
	case AutoGrindCore::LineShape::Lip:
	default:
		Text += TEXT(", along an edge");
		break;
	}
	Text += FString::Printf(TEXT("\n%.2f m long, falls %.2f m past the edge"), Length / 100, Drop / 100);
	if (EdgeAngle > 0 && Shape != AutoGrindCore::LineShape::Ridge)
	{
		Text += FString::Printf(TEXT(", the surface turns %.0f degrees over it"), EdgeAngle);
	}
	if (Sources.Num() > 1)
	{
		TArray<FString> Labels;
		for (const TWeakObjectPtr<AActor>& Source : Sources)
		{
			if (const AActor* Actor = Source.Get())
			{
				Labels.Add(Actor->GetActorNameOrLabel());
			}
		}
		Text += FString::Printf(TEXT("\nRuns along %d actors: %s"), Sources.Num(), *FString::Join(Labels, TEXT(", ")));
	}
	TArray<FString> Why;
	for (uint32 Bit = 0; Bit < AutoGrindCore::Notes::Count; ++Bit)
	{
		const uint32 Note = 1u << Bit;
		if ((Notes & Note) && Note != AutoGrindCore::Notes::Joined)
		{
			Why.Add(UTF8_TO_TCHAR(AutoGrindCore::DescribeNote(Note)));
		}
	}
	Text += FString::Printf(TEXT("\nConfidence %.0f%%"), Confidence * 100);
	if (Why.Num() > 0)
	{
		Text += TEXT(": ") + FString::Join(Why, TEXT("; "));
	}
	return FText::FromString(Text);
}

namespace AutoGrind
{
	const TCHAR* GrindActorClassName = TEXT("GrindActor_C");

	namespace
	{
		AutoGrindCore::Vec3 ToCore(const FVector& V)
		{
			return {V.X, V.Y, V.Z};
		}

		FVector FromCore(const AutoGrindCore::Vec3& V)
		{
			return FVector(V.X, V.Y, V.Z);
		}

		FBox BoundsOf(const AutoGrindCore::Mesh& Mesh)
		{
			FBox Box(ForceInit);
			for (const AutoGrindCore::Vec3& V : Mesh.Vertices)
			{
				Box += FromCore(V);
			}
			return Box;
		}
	}

	bool IsGrindActor(const AActor& Actor)
	{
		for (const UClass* Class = Actor.GetClass(); Class; Class = Class->GetSuperClass())
		{
			if (Class->GetFName() == GrindActorClassName)
			{
				return true;
			}
		}
		return false;
	}

	bool IsScannable(const AActor& Actor, const UAutoGrindSettings& Settings)
	{
		if (IsGrindActor(Actor))
		{
			return false;
		}
		for (const FName& Tag : Settings.ExcludeTags)
		{
			if (!Tag.IsNone() && Actor.ActorHasTag(Tag))
			{
				return false;
			}
		}
		return true;
	}

	bool PassesLevelFilters(const UStaticMeshComponent& Component, const UAutoGrindSettings& Settings)
	{
		const UStaticMesh* StaticMesh = Component.GetStaticMesh();
		if (!StaticMesh)
		{
			return false;
		}
		if (Settings.bRequireCollision && !CollisionEnabledHasQuery(Component.GetCollisionEnabled()))
		{
			return false;
		}
		if (Settings.bSkipHidden && !Component.IsVisibleInEditor())
		{
			return false;
		}
		if (Component.Bounds.BoxExtent.GetMax() * 2 < Settings.MinMeshSize)
		{
			return false;
		}
		return !Settings.IsExcludedName(StaticMesh->GetName()) && !Settings.IsExcludedName(Component.GetName());
	}

	TArray<AActor*> CollectLevelActors(UWorld& World, const UAutoGrindSettings& Settings, int32* OutFilteredOut)
	{
		TArray<AActor*> Out;
		int32 FilteredOut = 0;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || !IsScannable(*Actor, Settings))
			{
				continue;
			}
			TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
			if (Components.IsEmpty())
			{
				continue;
			}
			if ((Settings.bSkipHidden && Actor->IsHiddenEd()) || Settings.IsExcludedName(Actor->GetActorNameOrLabel()) || Settings.IsExcludedName(Actor->GetClass()->GetName()))
			{
				++FilteredOut;
				continue;
			}
			bool bAny = false;
			for (const UStaticMeshComponent* Component : Components)
			{
				bAny |= Component && PassesLevelFilters(*Component, Settings);
			}
			if (bAny)
			{
				Out.Add(Actor);
			}
			else
			{
				++FilteredOut;
			}
		}
		if (OutFilteredOut)
		{
			*OutFilteredOut = FilteredOut;
		}
		return Out;
	}

	bool ReadMesh(const UStaticMesh& Mesh, TArray<FVector3f>& OutPositions, TArray<uint32>& OutIndices)
	{
		const FMeshDescription* Description = Mesh.GetMeshDescription(0);
		if (!Description || Description->Triangles().Num() == 0)
		{
			return false;
		}
		const FStaticMeshConstAttributes Attributes(*Description);
		const TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TArray<int32> Compact;
		Compact.Init(INDEX_NONE, Description->Vertices().GetArraySize());
		for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
		{
			Compact[Vertex.GetValue()] = OutPositions.Add(Positions[Vertex]);
		}
		for (const FTriangleID Triangle : Description->Triangles().GetElementIDs())
		{
			// UE winds a triangle so its outward normal is Cross(P2 - P0, P1 - P0): "a left-handed
			// coordinate system, but a counter-clockwise winding order" (FStaticMeshOperations). The
			// core wants Cross(B - A, C - A), so the corners go in as 0, 2, 1.
			const TArrayView<const FVertexInstanceID> Corners = Description->GetTriangleVertexInstances(Triangle);
			for (const int32 Corner : {0, 2, 1})
			{
				OutIndices.Add(uint32(Compact[Description->GetVertexInstanceVertex(Corners[Corner]).GetValue()]));
			}
		}
		return true;
	}

	const FMeshLibrary::FLocal* FMeshLibrary::Load(const UStaticMesh& Mesh)
	{
		if (const TSharedPtr<FLocal>* Found = Read.Find(&Mesh))
		{
			return Found->Get();
		}
		if (Unreadable.Contains(&Mesh))
		{
			return nullptr;
		}
		TSharedPtr<FLocal> Fresh = MakeShared<FLocal>();
		if (!ReadMesh(Mesh, Fresh->Positions, Fresh->Indices))
		{
			Unreadable.Add(&Mesh);
			return nullptr;
		}
		return Read.Add(&Mesh, Fresh).Get();
	}

	bool FMeshLibrary::Place(const UStaticMeshComponent& Component, std::vector<AutoGrindCore::Mesh>& Out)
	{
		const UStaticMesh* StaticMesh = Component.GetStaticMesh();
		const FLocal* Local = StaticMesh ? Load(*StaticMesh) : nullptr;
		if (!Local)
		{
			return false;
		}
		// A mirroring transform (an odd number of negative scales) turns every triangle inside out.
		auto Emit = [&Out, Local](TFunctionRef<FVector(const FVector3f&)> ToWorld, bool bMirrored)
		{
			AutoGrindCore::Mesh Placed;
			Placed.Vertices.reserve(size_t(Local->Positions.Num()));
			for (const FVector3f& Position : Local->Positions)
			{
				Placed.Vertices.push_back(ToCore(ToWorld(Position)));
			}
			Placed.Indices.reserve(size_t(Local->Indices.Num()));
			for (int32 I = 0; I + 2 < Local->Indices.Num(); I += 3)
			{
				Placed.Indices.push_back(Local->Indices[I]);
				Placed.Indices.push_back(Local->Indices[I + (bMirrored ? 2 : 1)]);
				Placed.Indices.push_back(Local->Indices[I + (bMirrored ? 1 : 2)]);
			}
			Out.push_back(MoveTemp(Placed));
		};
		if (const USplineMeshComponent* Spline = Cast<USplineMeshComponent>(&Component))
		{
			// As the spline mesh bends its collision: each point moves to the spline's frame at its distance
			// along the mesh's forward axis.
			const FTransform ToWorld = Spline->GetComponentTransform();
			const ESplineMeshAxis::Type Forward = Spline->GetForwardAxis();
			Emit([Spline, &ToWorld, Forward](const FVector3f& Position)
			{
				FVector Flat(Position);
				double& Along = Forward == ESplineMeshAxis::X ? Flat.X : (Forward == ESplineMeshAxis::Y ? Flat.Y : Flat.Z);
				const float Distance = float(Along);
				Along = 0;
				return ToWorld.TransformPosition(Spline->CalcSliceTransform(Distance).TransformPosition(Flat));
			}, ToWorld.GetDeterminant() < 0);
			return true;
		}
		if (const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(&Component))
		{
			for (int32 Instance = 0; Instance < Instanced->GetInstanceCount(); ++Instance)
			{
				FTransform Transform;
				if (Instanced->GetInstanceTransform(Instance, Transform, true))
				{
					Emit([&Transform](const FVector3f& Position) { return Transform.TransformPosition(FVector(Position)); }, Transform.GetDeterminant() < 0);
				}
			}
			return true;
		}
		const FTransform Transform = Component.GetComponentTransform();
		Emit([&Transform](const FVector3f& Position) { return Transform.TransformPosition(FVector(Position)); }, Transform.GetDeterminant() < 0);
		return true;
	}

	FAutoGrindScanResult Scan(UWorld& World, const FAutoGrindScanRequest& Request)
	{
		const double Started = FPlatformTime::Seconds();
		FAutoGrindScanResult Result;
		FMeshLibrary Library;

		std::vector<AutoGrindCore::Mesh> Meshes;
		TArray<AActor*> Owners;
		TSet<AActor*> Seen;
		for (AActor* Actor : Request.Actors)
		{
			if (!Actor || IsGrindActor(*Actor) || Seen.Contains(Actor))
			{
				continue;
			}
			Seen.Add(Actor);
			++Result.ActorCount;
			TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
			for (const UStaticMeshComponent* Component : Components)
			{
				if (!Component || !Component->GetStaticMesh() || (Request.ComponentFilter && !Request.ComponentFilter(*Component)))
				{
					continue;
				}
				const size_t Before = Meshes.size();
				if (!Library.Place(*Component, Meshes))
				{
					Result.Skipped.AddUnique(Component->GetStaticMesh()->GetPathName());
					continue;
				}
				const std::string Label = TCHAR_TO_UTF8(*Actor->GetActorNameOrLabel());
				for (size_t I = Before; I < Meshes.size(); ++I)
				{
					Meshes[I].Name = Label;
					Owners.Add(Actor);
					Result.TriangleCount += int64(Meshes[I].Indices.size() / 3);
				}
			}
		}
		Result.MeshCount = int32(Meshes.size());

		FScopedSlowTask Progress(float(Meshes.size() + 2), LOCTEXT("ScanPreparing", "AutoGrind: preparing the scan..."));
		if (Request.bShowProgress)
		{
			Progress.MakeDialog(true);
		}
		Progress.EnterProgressFrame(1);

		// The fall past an edge is traced against what a skater can land on, complex collision included,
		// passing through existing GrindActors, whose volumes are not ground.
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(AutoGrindBelow), true);
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			if (IsGrindActor(**It))
			{
				Query.AddIgnoredActor(*It);
			}
		}
		const AutoGrindCore::SurfaceBelow Below = [&World, &Objects, &Query](const AutoGrindCore::Vec3& From, double MaxDistance) -> std::optional<double>
		{
			FHitResult Hit;
			const FVector Start = FromCore(From);
			if (World.LineTraceSingleByObjectType(Hit, Start, Start - FVector(0, 0, MaxDistance), Objects, Query))
			{
				return Hit.ImpactPoint.Z;
			}
			return std::nullopt;
		};

		// The wall test sees the triangles of every collision-enabled static mesh near the scanned ones, from
		// either side. The traces above cull back faces, so they miss a wall that an edge is buried in or that
		// a trace starts inside. Only triangles within reach of a scanned mesh are kept.
		std::vector<AutoGrindCore::Mesh> Nearby;
		if (Request.bCheckWalls && !Meshes.empty())
		{
			constexpr double CellSize = 100;
			const double Sideways = Request.Core.ProbeDistance + Request.Core.RailMaxWidth + CellSize;
			// Nothing above is cut off: the top of a tall part an edge is buried in must be there to be met.
			TSet<FIntPoint> Cells;
			double LowZ = TNumericLimits<double>::Max();
			auto CellOf = [](double X, double Y) { return FIntPoint(FMath::FloorToInt32(X / CellSize), FMath::FloorToInt32(Y / CellSize)); };
			for (const AutoGrindCore::Mesh& Scanned : Meshes)
			{
				const FBox Box = BoundsOf(Scanned);
				const FIntPoint Low = CellOf(Box.Min.X - Sideways, Box.Min.Y - Sideways);
				const FIntPoint High = CellOf(Box.Max.X + Sideways, Box.Max.Y + Sideways);
				for (int32 X = Low.X; X <= High.X; ++X)
				{
					for (int32 Y = Low.Y; Y <= High.Y; ++Y)
					{
						Cells.Add(FIntPoint(X, Y));
					}
				}
				LowZ = FMath::Min(LowZ, Box.Min.Z - Sideways);
			}
			auto InReach = [&](const FVector& Min, const FVector& Max)
			{
				if (Max.Z < LowZ)
				{
					return false;
				}
				const FIntPoint Low = CellOf(Min.X, Min.Y);
				const FIntPoint High = CellOf(Max.X, Max.Y);
				// Something as big as a whole street is kept and left to its triangles to decide.
				if (int64(High.X - Low.X + 1) * int64(High.Y - Low.Y + 1) > 4096)
				{
					return true;
				}
				for (int32 X = Low.X; X <= High.X; ++X)
				{
					for (int32 Y = Low.Y; Y <= High.Y; ++Y)
					{
						if (Cells.Contains(FIntPoint(X, Y)))
						{
							return true;
						}
					}
				}
				return false;
			};
			std::vector<AutoGrindCore::Mesh> Placed;
			for (TActorIterator<AActor> It(&World); It; ++It)
			{
				if (IsGrindActor(**It))
				{
					continue;
				}
				TInlineComponentArray<UStaticMeshComponent*> Components(*It);
				for (const UStaticMeshComponent* Component : Components)
				{
					if (!Component || !Component->GetStaticMesh() || !CollisionEnabledHasQuery(Component->GetCollisionEnabled()))
					{
						continue;
					}
					const FBox Bounds = Component->Bounds.GetBox();
					if (!InReach(Bounds.Min, Bounds.Max))
					{
						continue;
					}
					Placed.clear();
					if (!Library.Place(*Component, Placed))
					{
						continue;
					}
					for (AutoGrindCore::Mesh& Mesh : Placed)
					{
						// Keep only the triangles within reach; the winding stays outward, as the wall test tells
						// which side of a face it met.
						AutoGrindCore::Mesh Kept;
						Kept.Vertices = MoveTemp(Mesh.Vertices);
						for (size_t I = 0; I + 2 < Mesh.Indices.size(); I += 3)
						{
							const AutoGrindCore::Vec3& A = Kept.Vertices[Mesh.Indices[I]];
							const AutoGrindCore::Vec3& B = Kept.Vertices[Mesh.Indices[I + 1]];
							const AutoGrindCore::Vec3& C = Kept.Vertices[Mesh.Indices[I + 2]];
							const FVector Min(FMath::Min3(A.X, B.X, C.X), FMath::Min3(A.Y, B.Y, C.Y), FMath::Min3(A.Z, B.Z, C.Z));
							const FVector Max(FMath::Max3(A.X, B.X, C.X), FMath::Max3(A.Y, B.Y, C.Y), FMath::Max3(A.Z, B.Z, C.Z));
							if (InReach(Min, Max))
							{
								Kept.Indices.insert(Kept.Indices.end(), {Mesh.Indices[I], Mesh.Indices[I + 1], Mesh.Indices[I + 2]});
							}
						}
						if (!Kept.Indices.empty())
						{
							Nearby.push_back(MoveTemp(Kept));
						}
					}
				}
			}
		}
		const AutoGrindCore::TriangleField Solid(Nearby);
		AutoGrindCore::SegmentTest FirstHit;
		if (!Nearby.empty())
		{
			FirstHit = [&Solid](const AutoGrindCore::Vec3& From, const AutoGrindCore::Vec3& To) { return Solid.FirstHit(From, To); };
		}

		std::vector<AutoGrindCore::Rejection> Rejections;
		bool bCancelled = false;
		AutoGrindCore::ScanOptions Options;
		Options.Rejections = Request.bCollectNearMisses ? &Rejections : nullptr;
		Options.bCancelled = &bCancelled;
		Options.Progress = [&Progress, &Owners](size_t Done, size_t Total)
		{
			const AActor* Owner = Done < size_t(Owners.Num()) ? Owners[int32(Done)] : nullptr;
			Progress.EnterProgressFrame(1, FText::Format(LOCTEXT("ScanningMesh", "AutoGrind: scanning {0} ({1} of {2})"),
				FText::FromString(Owner ? Owner->GetActorNameOrLabel() : FString()), int32(Done + 1), int32(Total)));
			return !Progress.ShouldCancel();
		};
		const std::vector<AutoGrindCore::Line> Lines = AutoGrindCore::FindGrindLines(Meshes, Below, FirstHit, Request.Core, Options);
		Result.bCancelled = bCancelled;

		for (const AutoGrindCore::Line& Found : Lines)
		{
			FAutoGrindLine& Line = Result.Lines.AddDefaulted_GetRef();
			for (const AutoGrindCore::Vec3& Point : Found.Points)
			{
				Line.Points.Add(FromCore(Point));
			}
			Line.bRail = Found.Kind == AutoGrindCore::LineKind::Rail;
			Line.bClosed = Found.bClosed;
			Line.Length = Found.Length;
			Line.Drop = Found.Drop;
			Line.TopWidth = Found.TopWidth;
			Line.Thickness = Found.Thickness;
			Line.EdgeAngle = Found.EdgeAngle;
			Line.Shape = Found.Shape;
			Line.Confidence = float(Found.Confidence);
			Line.Notes = Found.LineNotes;
			Line.bSuggested = Found.bSuggested;
			Line.bKeep = Found.bSuggested;
			for (const size_t MeshIndex : Found.MeshIndices)
			{
				AActor* Owner = Owners[int32(MeshIndex)];
				if (!Line.Sources.Contains(Owner))
				{
					Line.Sources.Add(Owner);
				}
			}
			const AActor* Main = Line.MainSource();
			Line.SourceLabel = Main ? Main->GetActorNameOrLabel() : FString();
			if (Line.Sources.Num() > 1)
			{
				Line.SourceLabel += FString::Printf(TEXT(" +%d"), Line.Sources.Num() - 1);
			}
		}
		Result.Lines.Sort([](const FAutoGrindLine& L, const FAutoGrindLine& R) { return L.Length > R.Length; });
		for (const AutoGrindCore::Rejection& Rejected : Rejections)
		{
			Result.NearMisses.Add({FromCore(Rejected.A), FromCore(Rejected.B), Rejected.Reason, UTF8_TO_TCHAR(AutoGrindCore::Describe(Rejected.Reason))});
		}
		Result.Seconds = FPlatformTime::Seconds() - Started;
		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
