#include "AutoGrindScan.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

namespace AutoGrind
{
	const TCHAR* GrindActorClassName = TEXT("GrindActor_C");

	namespace
	{
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

		AutoGrindCore::Vec3 ToCore(const FVector& V)
		{
			return {V.X, V.Y, V.Z};
		}

		FVector FromCore(const AutoGrindCore::Vec3& V)
		{
			return FVector(V.X, V.Y, V.Z);
		}

		struct FLocalMesh
		{
			TArray<FVector3f> Positions;
			TArray<uint32> Indices;
		};
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

	FAutoGrindScanResult Scan(UWorld& World, const TArray<AActor*>& Actors, const AutoGrindCore::Settings& Settings, bool bCollectNearMisses, bool bCheckWalls)
	{
		const double Started = FPlatformTime::Seconds();
		FAutoGrindScanResult Result;

		std::vector<AutoGrindCore::Mesh> Meshes;
		TArray<AActor*> Owners;
		TMap<const UStaticMesh*, FLocalMesh> Read;
		TSet<const UStaticMesh*> Unreadable;
		auto Load = [&](const UStaticMesh& StaticMesh) -> const FLocalMesh*
		{
			if (const FLocalMesh* Local = Read.Find(&StaticMesh))
			{
				return Local;
			}
			if (Unreadable.Contains(&StaticMesh))
			{
				return nullptr;
			}
			FLocalMesh Fresh;
			if (!ReadMesh(StaticMesh, Fresh.Positions, Fresh.Indices))
			{
				Unreadable.Add(&StaticMesh);
				return nullptr;
			}
			return &Read.Add(&StaticMesh, MoveTemp(Fresh));
		};
		auto AddPlaced = [&](const UStaticMesh& StaticMesh, const FTransform& Transform, AActor& Owner)
		{
			const FLocalMesh* Local = Load(StaticMesh);
			if (!Local)
			{
				Result.Skipped.AddUnique(StaticMesh.GetPathName());
				return;
			}
			AutoGrindCore::Mesh Placed;
			Placed.Name = TCHAR_TO_UTF8(*Owner.GetActorNameOrLabel());
			Placed.Vertices.reserve(Local->Positions.Num());
			for (const FVector3f& Position : Local->Positions)
			{
				Placed.Vertices.push_back(ToCore(Transform.TransformPosition(FVector(Position))));
			}
			// A mirroring transform (an odd number of negative scales) turns every triangle inside out.
			const bool bMirrored = Transform.GetDeterminant() < 0;
			Placed.Indices.reserve(Local->Indices.Num());
			for (int32 I = 0; I + 2 < Local->Indices.Num(); I += 3)
			{
				Placed.Indices.push_back(Local->Indices[I]);
				Placed.Indices.push_back(Local->Indices[I + (bMirrored ? 2 : 1)]);
				Placed.Indices.push_back(Local->Indices[I + (bMirrored ? 1 : 2)]);
			}
			Meshes.push_back(std::move(Placed));
			Owners.Add(&Owner);
		};

		for (AActor* Actor : Actors)
		{
			if (!Actor || IsGrindActor(*Actor))
			{
				continue;
			}
			TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
			for (const UStaticMeshComponent* Component : Components)
			{
				const UStaticMesh* StaticMesh = Component->GetStaticMesh();
				// A spline mesh bends its mesh along a spline, which the transform alone does not describe.
				if (StaticMesh && Component->IsA<USplineMeshComponent>())
				{
					Result.Skipped.Add(Component->GetPathName() + TEXT(" (spline-deformed geometry is not supported yet)"));
					continue;
				}
				if (!StaticMesh)
				{
					continue;
				}
				if (const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Component))
				{
					for (int32 Instance = 0; Instance < Instanced->GetInstanceCount(); ++Instance)
					{
						FTransform Transform;
						if (Instanced->GetInstanceTransform(Instance, Transform, true))
						{
							AddPlaced(*StaticMesh, Transform, *Actor);
						}
					}
				}
				else
				{
					AddPlaced(*StaticMesh, Component->GetComponentTransform(), *Actor);
				}
			}
		}
		Result.MeshCount = int32(Meshes.size());

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
		if (bCheckWalls && !Meshes.empty())
		{
			constexpr double CellSize = 100;
			const double Sideways = Settings.ProbeDistance + Settings.RailMaxWidth + CellSize;
			// Nothing above is cut off: the top of a tall part an edge is buried in must be there to be met.
			TSet<FIntPoint> Cells;
			double LowZ = TNumericLimits<double>::Max();
			auto CellOf = [](double X, double Y) { return FIntPoint(FMath::FloorToInt32(X / CellSize), FMath::FloorToInt32(Y / CellSize)); };
			for (const AutoGrindCore::Mesh& Scanned : Meshes)
			{
				FBox Box(ForceInit);
				for (const AutoGrindCore::Vec3& V : Scanned.Vertices)
				{
					Box += FromCore(V);
				}
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
			auto AddNearby = [&](const FLocalMesh& Local, const FTransform& Transform)
			{
				AutoGrindCore::Mesh Placed;
				Placed.Vertices.reserve(Local.Positions.Num());
				for (const FVector3f& Position : Local.Positions)
				{
					Placed.Vertices.push_back(ToCore(Transform.TransformPosition(FVector(Position))));
				}
				// The wall test hits faces from either side, but tells which side it met: keep the winding
				// outward through a mirroring transform too.
				const bool bMirrored = Transform.GetDeterminant() < 0;
				for (int32 I = 0; I + 2 < Local.Indices.Num(); I += 3)
				{
					const AutoGrindCore::Vec3& A = Placed.Vertices[Local.Indices[I]];
					const AutoGrindCore::Vec3& B = Placed.Vertices[Local.Indices[I + 1]];
					const AutoGrindCore::Vec3& C = Placed.Vertices[Local.Indices[I + 2]];
					const FVector Min(FMath::Min3(A.X, B.X, C.X), FMath::Min3(A.Y, B.Y, C.Y), FMath::Min3(A.Z, B.Z, C.Z));
					const FVector Max(FMath::Max3(A.X, B.X, C.X), FMath::Max3(A.Y, B.Y, C.Y), FMath::Max3(A.Z, B.Z, C.Z));
					if (InReach(Min, Max))
					{
						Placed.Indices.insert(Placed.Indices.end(), {Local.Indices[I], Local.Indices[I + (bMirrored ? 2 : 1)], Local.Indices[I + (bMirrored ? 1 : 2)]});
					}
				}
				if (!Placed.Indices.empty())
				{
					Nearby.push_back(std::move(Placed));
				}
			};
			for (TActorIterator<AActor> It(&World); It; ++It)
			{
				if (IsGrindActor(**It))
				{
					continue;
				}
				TInlineComponentArray<UStaticMeshComponent*> Components(*It);
				for (const UStaticMeshComponent* Component : Components)
				{
					const UStaticMesh* StaticMesh = Component->GetStaticMesh();
					if (!StaticMesh || Component->IsA<USplineMeshComponent>() || !CollisionEnabledHasQuery(Component->GetCollisionEnabled()))
					{
						continue;
					}
					const FBox Bounds = Component->Bounds.GetBox();
					if (!InReach(Bounds.Min, Bounds.Max))
					{
						continue;
					}
					const FLocalMesh* Local = Load(*StaticMesh);
					if (!Local)
					{
						continue;
					}
					if (const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Component))
					{
						const FBox MeshBox = StaticMesh->GetBoundingBox();
						for (int32 Instance = 0; Instance < Instanced->GetInstanceCount(); ++Instance)
						{
							FTransform Transform;
							if (Instanced->GetInstanceTransform(Instance, Transform, true))
							{
								const FBox InstanceBox = MeshBox.TransformBy(Transform);
								if (InReach(InstanceBox.Min, InstanceBox.Max))
								{
									AddNearby(*Local, Transform);
								}
							}
						}
					}
					else
					{
						AddNearby(*Local, Component->GetComponentTransform());
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
		const std::vector<AutoGrindCore::Line> Lines = AutoGrindCore::FindGrindLines(Meshes, Below, FirstHit, Settings, bCollectNearMisses ? &Rejections : nullptr);
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
			AActor* Owner = Owners[int32(Found.MeshIndex)];
			Line.Source = Owner;
			Line.SourceLabel = Owner->GetActorNameOrLabel();
		}
		Result.Lines.Sort([](const FAutoGrindLine& L, const FAutoGrindLine& R) { return L.Length > R.Length; });
		for (const AutoGrindCore::Rejection& Rejected : Rejections)
		{
			Result.NearMisses.Add({FromCore(Rejected.A), FromCore(Rejected.B), UTF8_TO_TCHAR(AutoGrindCore::Describe(Rejected.Reason))});
		}
		Result.Seconds = FPlatformTime::Seconds() - Started;
		return Result;
	}
}
