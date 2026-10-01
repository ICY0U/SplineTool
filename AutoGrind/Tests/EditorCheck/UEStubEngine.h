// Compile-check stubs: UObject, properties, actors, components, worlds, meshes, collision, editor.
#pragma once

#include "UEStubCore.h"

class UClass;
class UWorld;
class ULevel;
class AActor;
class UPackage;
class UFont;

enum EObjectFlags { RF_NoFlags = 0, RF_Transactional = 1 };

class UObject
{
public:
	virtual ~UObject() = default;
	static UClass* StaticClass() { return nullptr; }
	FString GetName() const { return FString(); }
	FName GetFName() const { return FName(); }
	FString GetPathName() const { return FString(); }
	UClass* GetClass() const { return nullptr; }
	bool Modify(bool bAlwaysMarkDirty = true) { (void)bAlwaysMarkDirty; return true; }
	void SetFlags(EObjectFlags) {}
	void SaveConfig() {}
	bool MarkPackageDirty() const { return true; }
	template <typename T> bool IsA() const { return false; }
	UPackage* GetOutermost() const { return nullptr; }
};

class UField : public UObject {};
class UStruct : public UField {};

class UFunction : public UStruct
{
public:
	TArray<uint8> Script;
	UClass* GetOwnerClass() const { return nullptr; }
};

class UClass : public UStruct
{
public:
	UClass* GetSuperClass() const { return nullptr; }
	bool IsNative() const { return true; }
	bool IsChildOf(const UClass*) const { return false; }
	UFunction* FindFunctionByName(FName) const { return nullptr; }
};

class UEnum : public UField
{
public:
	int32 NumEnums() const { return 0; }
	FString GetNameStringByIndex(int32) const { return FString(); }
	FString GetNameStringByValue(int64) const { return FString(); }
	FText GetDisplayNameTextByIndex(int32) const { return FText(); }
	FText GetDisplayNameTextByValue(int64) const { return FText(); }
	FText GetToolTipTextByIndex(int32) const { return FText(); }
	int64 GetValueByIndex(int32) const { return 0; }
};
template <typename T> UEnum* StaticEnum() { return nullptr; }

class UPackage : public UObject {};

class FField
{
public:
	virtual ~FField() = default;
	FString GetName() const { return FString(); }
	template <typename T> bool IsA() const { return false; }
};
enum EPropertyPortFlags { PPF_None = 0 };
class FProperty : public FField
{
public:
	uint64 PropertyFlags = 0;
	UClass* GetOwnerClass() const { return nullptr; }
	FString GetCPPType() const { return FString(); }
	const FString& GetMetaData(const TCHAR*) const { static FString Empty; return Empty; }
	template <typename T> T* ContainerPtrToValuePtr(void* Container, int32 ArrayIndex = 0) const { (void)ArrayIndex; return static_cast<T*>(Container); }
	template <typename T> const T* ContainerPtrToValuePtr(const void* Container, int32 ArrayIndex = 0) const { (void)ArrayIndex; return static_cast<const T*>(Container); }
	void ExportTextItem_Direct(FString&, const void*, const void*, UObject*, int32, UObject* = nullptr) const {}
};
class FNumericProperty : public FProperty
{
public:
	void SetIntPropertyValue(void*, int64) const {}
	void SetIntPropertyValue(void*, uint64) const {}
};
class FByteProperty : public FNumericProperty
{
public:
	UEnum* Enum = nullptr;
	void SetPropertyValue_InContainer(void*, const uint8&, int32 ArrayIndex = 0) const { (void)ArrayIndex; }
	uint8 GetPropertyValue_InContainer(const void*, int32 ArrayIndex = 0) const { (void)ArrayIndex; return 0; }
};
class FEnumProperty : public FProperty
{
public:
	UEnum* GetEnum() const { return nullptr; }
	FNumericProperty* GetUnderlyingProperty() const { return nullptr; }
};
class FObjectPropertyBase : public FProperty {};
template <typename T> T* FindFProperty(const UStruct*, FName) { return nullptr; }

namespace EFieldIteratorFlags { enum SuperClassFlags { ExcludeSuper, IncludeSuper }; }
template <typename T>
class TFieldIterator
{
public:
	TFieldIterator(const UStruct*, EFieldIteratorFlags::SuperClassFlags = EFieldIteratorFlags::IncludeSuper) {}
	explicit operator bool() const { return false; }
	void operator++() {}
	T* operator*() const { return nullptr; }
	T* operator->() const { return nullptr; }
};

template <typename T> const T* GetDefault() { static T Instance; return &Instance; }
template <typename T> T* GetMutableDefault() { static T Instance; return &Instance; }
template <typename T> T* NewObject(UObject* Outer = nullptr, FName Name = FName()) { (void)Outer; (void)Name; return new T(); }
template <typename T> T* LoadObject(UObject*, const TCHAR*) { return nullptr; }
template <typename T> UClass* LoadClass(UObject*, const TCHAR*) { return nullptr; }
template <typename T, typename U> T* Cast(U* Object) { return dynamic_cast<T*>(Object); }
template <typename T, typename U> const T* Cast(const U* Object) { return dynamic_cast<const T*>(Object); }
inline bool IsValid(const UObject* Object) { return Object != nullptr; }
inline UPackage* LoadPackage(UPackage*, const TCHAR*, uint32) { return nullptr; }
enum ELoadFlags { LOAD_None = 0 };

template <typename T>
class TWeakObjectPtr
{
public:
	T* Ptr = nullptr;
	TWeakObjectPtr() = default;
	TWeakObjectPtr(T* In) : Ptr(In) {}
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>> TWeakObjectPtr(U* In) : Ptr(In) {}
	T* Get() const { return Ptr; }
	bool IsValid() const { return Ptr != nullptr; }
	void Reset() { Ptr = nullptr; }
	T* operator->() const { return Ptr; }
	bool operator==(const TWeakObjectPtr& Other) const { return Ptr == Other.Ptr; }
	template <typename U> bool operator==(const U* Other) const { return Ptr == Other; }
};

class FSoftObjectPath
{
public:
	FSoftObjectPath() = default;
	explicit FSoftObjectPath(const TCHAR*) {}
	bool IsValid() const { return true; }
	FString ToString() const { return FString(); }
};
class FSoftClassPath : public FSoftObjectPath
{
public:
	FSoftClassPath() = default;
	explicit FSoftClassPath(const TCHAR* In) : FSoftObjectPath(In) {}
};

class FReferenceCollector {};
class FGCObject
{
public:
	virtual ~FGCObject() = default;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) = 0;
	virtual FString GetReferencerName() const = 0;
};

// ----- Components and actors -----

namespace EComponentMobility { enum Type { Static, Stationary, Movable }; }
namespace ECollisionEnabled { enum Type { NoCollision, QueryOnly, PhysicsOnly, QueryAndPhysics }; }
inline bool CollisionEnabledHasQuery(ECollisionEnabled::Type Type) { return Type == ECollisionEnabled::QueryOnly || Type == ECollisionEnabled::QueryAndPhysics; }
enum ECollisionChannel { ECC_WorldStatic, ECC_WorldDynamic, ECC_Visibility, ECC_GameTraceChannel2 };
enum ECollisionResponse { ECR_Ignore, ECR_Overlap, ECR_Block };
enum ESceneDepthPriorityGroup { SDPG_World, SDPG_Foreground };
template <typename T> struct TEnumAsByte { T Value; TEnumAsByte(T In = T()) : Value(In) {} T GetValue() const { return Value; } operator T() const { return Value; } };

class UActorComponent : public UObject
{
public:
	uint8 CreationMethod = 0;
	TArray<FName> ComponentTags;
	void RegisterComponent() {}
	AActor* GetOwner() const { return nullptr; }
};

class USceneComponent : public UActorComponent
{
public:
	TEnumAsByte<EComponentMobility::Type> Mobility;
	FBoxSphereBounds Bounds;
	const FTransform& GetComponentTransform() const { static FTransform T; return T; }
	FVector GetRelativeLocation() const { return FVector(); }
	bool IsVisibleInEditor() const { return true; }
	void SetMobility(EComponentMobility::Type) {}
};

class UPrimitiveComponent : public USceneComponent
{
public:
	bool bHiddenInGame = false;
	ECollisionEnabled::Type GetCollisionEnabled() const { return ECollisionEnabled::QueryAndPhysics; }
	ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel) const { return ECR_Block; }
	FName GetCollisionProfileName() const { return FName(); }
	void SetCollisionProfileName(FName, bool bUpdateOverlaps = true) { (void)bUpdateOverlaps; }
};

class UStaticMesh;
class UMeshComponent : public UPrimitiveComponent {};
class UStaticMeshComponent : public UMeshComponent
{
public:
	UStaticMesh* GetStaticMesh() const { return nullptr; }
	bool SetStaticMesh(UStaticMesh*) { return true; }
};

class UInstancedStaticMeshComponent : public UStaticMeshComponent
{
public:
	int32 GetInstanceCount() const { return 0; }
	bool GetInstanceTransform(int32, FTransform&, bool bWorldSpace = false) const { (void)bWorldSpace; return false; }
};

namespace ESplineMeshAxis { enum Type { X, Y, Z }; }
class USplineMeshComponent : public UStaticMeshComponent
{
public:
	ESplineMeshAxis::Type GetForwardAxis() const { return ESplineMeshAxis::X; }
	void SetForwardAxis(ESplineMeshAxis::Type, bool bUpdateMesh = true) { (void)bUpdateMesh; }
	FTransform CalcSliceTransform(const double DistanceAlong) const { (void)DistanceAlong; return FTransform(); }
	void SetStartAndEnd(FVector, FVector, FVector, FVector, bool bUpdateMesh = true) { (void)bUpdateMesh; }
	FVector GetStartPosition() const { return FVector(); }
	FVector GetEndPosition() const { return FVector(); }
};

namespace ESplineCoordinateSpace { enum Type { Local, World }; }
namespace ESplinePointType { enum Type { Linear, Curve, Constant, CurveClamped, CurveCustomTangent }; }
class USplineComponent : public UPrimitiveComponent
{
public:
	bool bSplineHasBeenEdited = false;
	bool bInputSplinePointsToConstructionScript = false;
	void SetClosedLoop(bool, bool bUpdateSpline = true) { (void)bUpdateSpline; }
	bool IsClosedLoop() const { return false; }
	void SetSplinePoints(const TArray<FVector>&, ESplineCoordinateSpace::Type, bool bUpdateSpline = true) { (void)bUpdateSpline; }
	void SetSplinePointType(int32, ESplinePointType::Type, bool bUpdateSpline = true) { (void)bUpdateSpline; }
	ESplinePointType::Type GetSplinePointType(int32) const { return ESplinePointType::Linear; }
	void UpdateSpline() {}
	int32 GetNumberOfSplinePoints() const { return 0; }
	FVector GetLocationAtSplinePoint(int32, ESplineCoordinateSpace::Type) const { return FVector(); }
};

struct FBatchedLine {};
struct FBatchedPoint {};
class ULineBatchComponent : public UPrimitiveComponent
{
public:
	static constexpr uint32 INVALID_ID = 0;
	TArray<FBatchedLine> BatchedLines;
	TArray<FBatchedPoint> BatchedPoints;
	void DrawLine(const FVector&, const FVector&, const FLinearColor&, uint8, float Thickness = 0, float LifeTime = 0, uint32 BatchID = INVALID_ID) { (void)Thickness; (void)LifeTime; (void)BatchID; }
	void DrawPoint(const FVector&, const FLinearColor&, float, uint8, float LifeTime = 0, uint32 BatchID = INVALID_ID) { (void)LifeTime; (void)BatchID; }
	void ClearBatch(uint32) {}
};

class AActor : public UObject
{
public:
	TArray<FName> Tags;
	FString GetActorNameOrLabel() const { return FString(); }
	bool ActorHasTag(FName) const { return false; }
	bool IsHiddenEd() const { return false; }
	FTransform GetActorTransform() const { return FTransform(); }
	FVector GetActorLocation() const { return FVector(); }
	FRotator GetActorRotation() const { return FRotator(); }
	FVector GetActorScale3D() const { return FVector(); }
	ULevel* GetLevel() const { return nullptr; }
	UWorld* GetWorld() const { return nullptr; }
	bool SetRootComponent(USceneComponent*) { return true; }
	bool SetActorTransform(const FTransform&) { return true; }
	bool SetActorLocation(const FVector&) { return true; }
	template <typename T> T* FindComponentByClass() const { return nullptr; }
	void RerunConstructionScripts() {}
	void InvalidateLightingCache() {}
	void PostEditMove(bool) {}
	void SetFolderPath(const FName&) {}
	FName GetFolderPath() const { return FName(); }
	FBox GetComponentsBoundingBox(bool bNonColliding = false) const { (void)bNonColliding; return FBox(); }
};

template <typename T, int N = 24>
class TInlineComponentArray : public TArray<T>
{
public:
	explicit TInlineComponentArray(const AActor*) {}
};

class ULevel : public UObject
{
public:
	TArray<AActor*> Actors;
};

// ----- Worlds -----

struct FHitResult
{
	FVector ImpactPoint;
	AActor* GetActor() const { return nullptr; }
};

struct FCollisionObjectQueryParams
{
	void AddObjectTypesToQuery(ECollisionChannel) {}
};

struct FCollisionQueryParams
{
	FCollisionQueryParams() = default;
	FCollisionQueryParams(FName, bool bInTraceComplex = false) { (void)bInTraceComplex; }
	void AddIgnoredActor(const AActor*) {}
	void AddIgnoredActors(const TArray<AActor*>&) {}
	bool bTraceComplex = false;
};

namespace ESpawnActorCollisionHandlingMethod { enum Type { AlwaysSpawn }; }
struct FActorSpawnParameters
{
	ULevel* OverrideLevel = nullptr;
	ESpawnActorCollisionHandlingMethod::Type SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	EObjectFlags ObjectFlags = RF_NoFlags;
};

namespace EWorldType { enum Type { None, Game, Editor }; }
enum ELevelTick { LEVELTICK_All };
struct FURL {};

class UWorld : public UObject
{
public:
	ULineBatchComponent* PersistentLineBatcher = nullptr;
	ULevel* PersistentLevel = nullptr;
	ULevel* GetCurrentLevel() const { return nullptr; }
	AActor* SpawnActor(UClass*, const FVector* Location = nullptr, const FRotator* Rotation = nullptr, const FActorSpawnParameters& Parameters = FActorSpawnParameters()) { (void)Location; (void)Rotation; (void)Parameters; return nullptr; }
	template <typename T> T* SpawnActor() { return nullptr; }
	bool EditorDestroyActor(AActor*, bool) { return true; }
	bool LineTraceSingleByObjectType(FHitResult&, const FVector&, const FVector&, const FCollisionObjectQueryParams&, const FCollisionQueryParams& = FCollisionQueryParams()) const { return false; }
	bool LineTraceSingleByChannel(FHitResult&, const FVector&, const FVector&, ECollisionChannel, const FCollisionQueryParams& = FCollisionQueryParams()) const { return false; }
	void UpdateWorldComponents(bool, bool) {}
	void InitializeActorsForPlay(const FURL&) {}
	void BeginPlay() {}
	void Tick(ELevelTick, float) {}
	void DestroyWorld(bool) {}
	static UWorld* CreateWorld(EWorldType::Type, bool, FName = FName()) { return nullptr; }
	static UWorld* FindWorldInPackage(UPackage*) { return nullptr; }
};

struct FWorldContext
{
	UWorld* World() const { return nullptr; }
	void SetCurrentWorld(UWorld*) {}
};

template <typename T>
class TActorIterator
{
public:
	explicit TActorIterator(const UWorld*) {}
	explicit operator bool() const { return false; }
	void operator++() {}
	T* operator*() const { return nullptr; }
	T* operator->() const { return nullptr; }
};

// ----- Meshes -----

struct FElementID
{
	int32 GetValue() const { return 0; }
};
struct FVertexID : FElementID {};
struct FTriangleID : FElementID {};
struct FVertexInstanceID : FElementID {};
template <typename IdType>
struct TElementIds
{
	const IdType* begin() const { return nullptr; }
	const IdType* end() const { return nullptr; }
};
template <typename IdType>
struct TElementArray
{
	int32 Num() const { return 0; }
	int32 GetArraySize() const { return 0; }
	TElementIds<IdType> GetElementIDs() const { return {}; }
};
class FMeshDescription
{
public:
	const TElementArray<FTriangleID>& Triangles() const { static TElementArray<FTriangleID> A; return A; }
	const TElementArray<FVertexID>& Vertices() const { static TElementArray<FVertexID> A; return A; }
	TArrayView<const FVertexInstanceID> GetTriangleVertexInstances(FTriangleID) const { return {}; }
	FVertexID GetVertexInstanceVertex(FVertexInstanceID) const { return {}; }
};
template <typename T>
struct TVertexAttributesConstRef
{
	T operator[](FVertexID) const { return T(); }
};
class FStaticMeshConstAttributes
{
public:
	explicit FStaticMeshConstAttributes(const FMeshDescription&) {}
	TVertexAttributesConstRef<FVector3f> GetVertexPositions() const { return {}; }
};

class UStaticMesh : public UObject
{
public:
	const FMeshDescription* GetMeshDescription(int32) const { return nullptr; }
	FBoxSphereBounds GetBounds() const { return {}; }
	FBox GetBoundingBox() const { return FBox(); }
};

// ----- Rendering -----

class FSceneInterface;
class FViewport
{
public:
	bool KeyState(const struct FKey&) const { return false; }
};
class FSceneView {};
class FSceneViewFamily
{
public:
	struct ConstructionValues
	{
		ConstructionValues(FViewport*, FSceneInterface*, const struct FEngineShowFlags&) {}
		ConstructionValues& SetRealtimeUpdate(bool) { return *this; }
	};
};
class FSceneViewFamilyContext : public FSceneViewFamily
{
public:
	explicit FSceneViewFamilyContext(const ConstructionValues&) {}
};
struct FEngineShowFlags {};

class FPrimitiveDrawInterface
{
public:
	virtual ~FPrimitiveDrawInterface() = default;
	virtual void DrawLine(const FVector& Start, const FVector& End, const FLinearColor& Color, uint8 DepthPriorityGroup, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false) = 0;
	virtual void DrawPoint(const FVector& Position, const FLinearColor& Color, float PointSize, uint8 DepthPriorityGroup) = 0;
};

class UFont : public UObject {};
class FCanvas
{
public:
	void DrawShadowedString(float, float, const TCHAR*, const UFont*, const FLinearColor&, const FLinearColor& ShadowColor = FLinearColor::Black) { (void)ShadowColor; }
};

class UEngine : public UObject
{
public:
	static UFont* GetSmallFont() { return nullptr; }
	FWorldContext& CreateNewWorldContext(EWorldType::Type) { static FWorldContext C; return C; }
	void DestroyWorldContext(UWorld*) {}
};
extern UEngine* GEngine;

// ----- Input -----

struct FKey
{
	FName Name;
	FKey() = default;
	explicit FKey(const TCHAR* In) : Name(In) {}
	bool operator==(const FKey& Other) const { return Name == Other.Name; }
	bool operator!=(const FKey& Other) const { return !(Name == Other.Name); }
};
struct EKeys
{
	static inline const FKey LeftMouseButton{TEXT("LeftMouseButton")};
	static inline const FKey RightMouseButton{TEXT("RightMouseButton")};
	static inline const FKey Enter{TEXT("Enter")};
	static inline const FKey BackSpace{TEXT("BackSpace")};
	static inline const FKey Escape{TEXT("Escape")};
	static inline const FKey SpaceBar{TEXT("SpaceBar")};
	static inline const FKey Delete{TEXT("Delete")};
	static inline const FKey LeftShift{TEXT("LeftShift")};
	static inline const FKey RightShift{TEXT("RightShift")};
	static inline const FKey F{TEXT("F")};
	static inline const FKey R{TEXT("R")};
	static inline const FKey S{TEXT("S")};
	static inline const FKey T{TEXT("T")};
};
enum EInputEvent { IE_Pressed, IE_Released, IE_Repeat, IE_DoubleClick, IE_Axis };
namespace EMouseCursor { enum Type { None, Default, Crosshairs }; }

// ----- Editor -----

class HHitProxy {};
using FEditorModeID = FName;
class FEditorViewportClient;

class FViewportCursorLocation
{
public:
	FViewportCursorLocation(const FSceneView*, FEditorViewportClient*, int32, int32) {}
	const FVector& GetOrigin() const { static FVector V; return V; }
	const FVector& GetDirection() const { static FVector V; return V; }
};
class FViewportClick : public FViewportCursorLocation
{
public:
	FViewportClick() : FViewportCursorLocation(nullptr, nullptr, 0, 0) {}
	const FKey& GetKey() const { static FKey K; return K; }
	EInputEvent GetEvent() const { return IE_Pressed; }
	bool IsControlDown() const { return false; }
	bool IsShiftDown() const { return false; }
	bool IsAltDown() const { return false; }
};

class FEditorViewportClient
{
public:
	FViewport* Viewport = nullptr;
	FEngineShowFlags EngineShowFlags;
	FSceneInterface* GetScene() const { return nullptr; }
	bool IsRealtime() const { return true; }
	bool IsOrtho() const { return false; }
	virtual FSceneView* CalcSceneView(FSceneViewFamily*, const int32 StereoViewIndex = -1) { (void)StereoViewIndex; return nullptr; }
	virtual ~FEditorViewportClient() = default;
};

class FEditorModeTools;
class FEdMode : public TSharedFromThis<FEdMode>, public FGCObject
{
public:
	virtual ~FEdMode() = default;
	virtual void Enter() {}
	virtual void Exit() {}
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) { (void)ViewportClient; (void)Viewport; (void)x; (void)y; return false; }
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) { (void)InViewportClient; (void)HitProxy; (void)Click; return false; }
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) { (void)ViewportClient; (void)Viewport; (void)Key; (void)Event; return false; }
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) { (void)View; (void)Viewport; (void)PDI; }
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) { (void)ViewportClient; (void)Viewport; (void)View; (void)Canvas; }
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const { (void)OtherModeID; return false; }
	virtual bool UsesToolkits() const { return false; }
	virtual bool ShouldDrawWidget() const { return true; }
	virtual bool GetCursor(EMouseCursor::Type& OutCursor) const { (void)OutCursor; return false; }
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { (void)Collector; }
	virtual FString GetReferencerName() const override { return TEXT("FEdMode"); }
};

class FSlateIcon;
class FEditorModeRegistry
{
public:
	static FEditorModeRegistry& Get() { static FEditorModeRegistry R; return R; }
	template <typename T> void RegisterMode(FEditorModeID, FText = FText(), const FSlateIcon* = nullptr, bool = false, int32 = MAX_int32) { static_assert(std::is_base_of_v<FEdMode, T>, "not a mode"); T Probe; (void)Probe; }
	template <typename T> void RegisterMode(FEditorModeID Id, FText Name, const FSlateIcon& Icon, bool bVisible = false, int32 Priority = MAX_int32) { RegisterMode<T>(Id, Name, &Icon, bVisible, Priority); }
	void UnregisterMode(FEditorModeID) {}
};

class FEditorModeTools
{
public:
	bool IsModeActive(FEditorModeID) const { return false; }
	void ActivateMode(FEditorModeID, bool bToggle = false) { (void)bToggle; }
	void DeactivateMode(FEditorModeID) {}
};
FEditorModeTools& GLevelEditorModeTools();

class FTimerManager
{
public:
	template <typename L> int SetTimerForNextTick(L&& Callback) { static_assert(std::is_invocable_v<L>, "timer callback"); return 0; }
};

class USelection;
class FSelectionIterator
{
public:
	explicit FSelectionIterator(USelection&) {}
	explicit operator bool() const { return false; }
	void operator++() {}
	UObject* operator*() const { return nullptr; }
};

class UTransactor : public UObject {};
class UEditorEngine : public UEngine
{
public:
	UTransactor* Trans = nullptr;
	FWorldContext& GetEditorWorldContext(bool bEnsureIsGWorld = false) { (void)bEnsureIsGWorld; static FWorldContext C; return C; }
	USelection& GetSelectedActorIterator() { return *reinterpret_cast<USelection*>(this); }
	int32 GetSelectedActorCount() const { return 0; }
	void SelectNone(bool, bool, bool WarnAboutManyActors = true) { (void)WarnAboutManyActors; }
	void SelectActor(AActor*, bool, bool, bool bSelectEvenIfHidden = false, bool bForceRefresh = false) { (void)bSelectEvenIfHidden; (void)bForceRefresh; }
	void NoteSelectionChange(bool bNotify = true) { (void)bNotify; }
	void MoveViewportCamerasToBox(const FBox&, bool) const {}
	TSharedRef<FTimerManager> GetTimerManager() { return MakeShared<FTimerManager>(); }
	bool UndoTransaction(bool bCanRedo = true) { (void)bCanRedo; return true; }
	bool RedoTransaction() { return true; }
	UTransactor* CreateTrans() { return nullptr; }
};
extern UEditorEngine* GEditor;
extern bool GAllowActorScriptExecutionInEditor;

class FScopedTransaction
{
public:
	explicit FScopedTransaction(const FText&) {}
	void Cancel() {}
};

class FScopedSlowTask
{
public:
	FScopedSlowTask(float, const FText& = FText(), bool bEnabled = true) { (void)bEnabled; }
	void MakeDialog(bool bShowCancelButton = false, bool bAllowInPIE = false) { (void)bShowCancelButton; (void)bAllowInPIE; }
	void EnterProgressFrame(float ExpectedWorkThisFrame = 1.f, const FText& Text = FText()) { (void)ExpectedWorkThisFrame; (void)Text; }
	bool ShouldCancel() const { return false; }
};

struct FActorLabelUtilities
{
	static void SetActorLabelUnique(AActor*, const FString&, const void* InExistingActorLabels = nullptr) { (void)InExistingActorLabels; }
};

class UCommandlet : public UObject
{
public:
	virtual int32 Main(const FString& Params) { (void)Params; return 0; }
};

class UBlueprint : public UObject {};
class USCS_Node : public UObject
{
public:
	UClass* ComponentClass = nullptr;
	FName GetVariableName() const { return FName(); }
};
class USimpleConstructionScript : public UObject
{
public:
	const TArray<USCS_Node*>& GetAllNodes() const { static TArray<USCS_Node*> A; return A; }
};
class UBlueprintGeneratedClass : public UClass
{
public:
	USimpleConstructionScript* SimpleConstructionScript = nullptr;
};

// ----- Modules and plugins -----

class IModuleInterface
{
public:
	virtual ~IModuleInterface() = default;
	virtual void StartupModule() {}
	virtual void ShutdownModule() {}
};
class FModuleManager
{
public:
	template <typename T> static T& LoadModuleChecked(const FName) { static T Module; return Module; }
};
struct FPluginDescriptor
{
	FString VersionName;
};
class IPlugin
{
public:
	virtual ~IPlugin() = default;
	virtual FString GetBaseDir() const = 0;
	virtual const FPluginDescriptor& GetDescriptor() const = 0;
};
class IPluginManager
{
public:
	static IPluginManager& Get() { static IPluginManager M; return M; }
	TSharedPtr<IPlugin> FindPlugin(const FString&) { return nullptr; }
};
