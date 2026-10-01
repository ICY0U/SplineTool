#include "AutoGrindInspectCommandlet.h"

#include "AutoGrindScan.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Level.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogAutoGrindInspect, Display, All);

namespace AutoGrindInspect
{
	bool IsGrindActor(const AActor& Actor)
	{
		for (const UClass* Class = Actor.GetClass(); Class; Class = Class->GetSuperClass())
		{
			if (Class->GetFName() == AutoGrind::GrindActorClassName)
			{
				return true;
			}
		}
		return false;
	}

	void DumpClass(const UClass& Class)
	{
		UE_LOG(LogAutoGrindInspect, Display, TEXT("CLASS %s"), *Class.GetPathName());
		if (const UFunction* Construction = Class.FindFunctionByName(TEXT("UserConstructionScript")))
		{
			UE_LOG(LogAutoGrindInspect, Display, TEXT("  UserConstructionScript: %d bytes of script, owned by %s"), Construction->Script.Num(), *Construction->GetOwnerClass()->GetName());
		}
		for (TFieldIterator<UFunction> It(&Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			UE_LOG(LogAutoGrindInspect, Display, TEXT("  function %s: %d bytes"), *It->GetName(), It->Script.Num());
		}
		for (TFieldIterator<FProperty> It(&Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			UE_LOG(LogAutoGrindInspect, Display, TEXT("  property %s: %s flags=%llx"), *It->GetName(), *It->GetCPPType(), uint64(It->PropertyFlags));
		}
		if (const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(&Class))
		{
			if (Generated->SimpleConstructionScript)
			{
				for (const USCS_Node* Node : Generated->SimpleConstructionScript->GetAllNodes())
				{
					UE_LOG(LogAutoGrindInspect, Display, TEXT("  SCS %s: %s"), *Node->GetVariableName().ToString(), Node->ComponentClass ? *Node->ComponentClass->GetName() : TEXT("?"));
				}
			}
		}
	}

	void DumpActor(const AActor& Actor)
	{
		UE_LOG(LogAutoGrindInspect, Display, TEXT("ACTOR %s (%s) at %s rot %s scale %s"), *Actor.GetActorNameOrLabel(), *Actor.GetName(),
			*Actor.GetActorLocation().ToString(), *Actor.GetActorRotation().ToString(), *Actor.GetActorScale3D().ToString());
		for (TFieldIterator<FProperty> It(Actor.GetClass()); It; ++It)
		{
			if (!It->GetOwnerClass() || It->GetOwnerClass()->IsNative() || It->IsA<FObjectPropertyBase>())
			{
				continue;
			}
			FString Value;
			It->ExportTextItem_Direct(Value, It->ContainerPtrToValuePtr<void>(&Actor), nullptr, nullptr, PPF_None);
			UE_LOG(LogAutoGrindInspect, Display, TEXT("  %s = %s"), *It->GetName(), *Value);
		}
		UE_LOG(LogAutoGrindInspect, Display, TEXT("  tags: %s, folder: %s"), *FString::JoinBy(Actor.Tags, TEXT(","), [](const FName& Tag) { return Tag.ToString(); }), *Actor.GetFolderPath().ToString());
		TInlineComponentArray<UActorComponent*> Components(&Actor);
		for (const UActorComponent* Component : Components)
		{
			FString Detail;
			if (const USceneComponent* Scene = Cast<USceneComponent>(Component))
			{
				Detail += FString::Printf(TEXT(" mobility=%d rel=%s"), int32(Scene->Mobility.GetValue()), *Scene->GetRelativeLocation().ToString());
			}
			if (const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			{
				Detail += FString::Printf(TEXT(" profile=%s enabled=%d grindchannel=%d hiddenInGame=%d tags=%s"), *Primitive->GetCollisionProfileName().ToString(), int32(Primitive->GetCollisionEnabled()),
					int32(Primitive->GetCollisionResponseToChannel(ECC_GameTraceChannel2)), Primitive->bHiddenInGame ? 1 : 0,
					*FString::JoinBy(Primitive->ComponentTags, TEXT(","), [](const FName& Tag) { return Tag.ToString(); }));
			}
			if (const USplineComponent* Spline = Cast<USplineComponent>(Component))
			{
				Detail += FString::Printf(TEXT(" points=%d edited=%d inputToCS=%d closed=%d"), Spline->GetNumberOfSplinePoints(), Spline->bSplineHasBeenEdited ? 1 : 0,
					Spline->bInputSplinePointsToConstructionScript ? 1 : 0, Spline->IsClosedLoop() ? 1 : 0);
				for (int32 I = 0; I < Spline->GetNumberOfSplinePoints(); ++I)
				{
					Detail += FString::Printf(TEXT(" [%s type=%d]"), *Spline->GetLocationAtSplinePoint(I, ESplineCoordinateSpace::Local).ToString(), int32(Spline->GetSplinePointType(I)));
				}
			}
			if (const USplineMeshComponent* SplineMesh = Cast<USplineMeshComponent>(Component))
			{
				Detail += FString::Printf(TEXT(" mesh=%s start=%s end=%s"), SplineMesh->GetStaticMesh() ? *SplineMesh->GetStaticMesh()->GetName() : TEXT("none"),
					*SplineMesh->GetStartPosition().ToString(), *SplineMesh->GetEndPosition().ToString());
			}
			UE_LOG(LogAutoGrindInspect, Display, TEXT("  component %s: %s created=%d%s"), *Component->GetName(), *Component->GetClass()->GetName(), int32(Component->CreationMethod), *Detail);
		}
	}
}

int32 UAutoGrindInspectCommandlet::Main(const FString& Params)
{
	FString MapPath;
	if (!FParse::Value(*Params, TEXT("map="), MapPath))
	{
		UE_LOG(LogAutoGrindInspect, Error, TEXT("Pass -map=/Game/Path/To/Map"));
		return 1;
	}
	UPackage* Package = LoadPackage(nullptr, *MapPath, LOAD_None);
	UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
	if (!World || !World->PersistentLevel)
	{
		UE_LOG(LogAutoGrindInspect, Error, TEXT("Cannot load a world from %s"), *MapPath);
		return 1;
	}
	int32 Count = 0;
	for (const AActor* Actor : World->PersistentLevel->Actors)
	{
		if (Actor && AutoGrindInspect::IsGrindActor(*Actor))
		{
			if (Count++ == 0)
			{
				AutoGrindInspect::DumpClass(*Actor->GetClass());
			}
			AutoGrindInspect::DumpActor(*Actor);
		}
	}
	UE_LOG(LogAutoGrindInspect, Display, TEXT("INSPECT_DONE: %d GrindActors in %s"), Count, *MapPath);
	return 0;
}
