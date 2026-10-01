// Stand-ins for the parts of Unreal Engine 5.4 the AutoGrind editor module uses, so it can be compiled and
// linked without the engine (CMake option AUTOGRIND_EDITOR_CHECK). This file: core types, containers,
// strings, maths and delegates. Declarations follow UE's names and signatures, and UE's strictness where
// it catches real mistakes: delegates bind only methods of the exact signature, and Printf, UE_LOG and
// checkf format strings are checked against their arguments. Bodies do nothing useful.
//
// Passing this check does not prove the module builds against the engine; Tests/run_unreal.ps1 does.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
typedef wchar_t TCHAR;
typedef wchar_t WIDECHAR;
typedef char ANSICHAR;
typedef char UTF8CHAR;

#define TEXT(x) L##x
#define INDEX_NONE (-1)
#define MAX_uint32 (0xffffffffu)
#define MAX_int32 (0x7fffffff)
#define KINDA_SMALL_NUMBER (1.e-4f)
#define SMALL_NUMBER (1.e-8f)
#define check(x) ((void)(x))
#define checkf(x, ...) ((void)(x), (void)FString::Printf(__VA_ARGS__))
#define verify(x) ((void)(x))
#define FORCEINLINE inline
#define UE_LOG(Category, Verbosity, Format, ...) ((void)FString::Printf(Format, ##__VA_ARGS__))
#define DEFINE_LOG_CATEGORY_STATIC(...)
#define LOCTEXT(Key, Text) FText::FromString(TEXT(Text))
#define NSLOCTEXT(Namespace, Key, Text) FText::FromString(TEXT(Text))
#define UCLASS(...)
#define USTRUCT(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UENUM(...)
#define UMETA(...)
#define GENERATED_BODY()
#define GENERATED_UCLASS_BODY()
#define IMPLEMENT_MODULE(Class, Name)
#define SCENE_QUERY_STAT(Name) FName(TEXT(#Name))
#define WITH_EDITOR 1
#define WITH_EDITORONLY_DATA 1

namespace ESearchCase { enum Type { CaseSensitive, IgnoreCase }; }
namespace ESearchDir { enum Type { FromStart, FromEnd }; }
enum EForceInit { ForceInit, ForceInitToZero };

template <typename T> inline void Swap(T& A, T& B) { std::swap(A, B); }
template <typename T> inline typename std::remove_reference<T>::type&& MoveTemp(T&& Value) { return static_cast<typename std::remove_reference<T>::type&&>(Value); }
template <typename T> struct TNumericLimits { static constexpr T Max() { return std::numeric_limits<T>::max(); } static constexpr T Lowest() { return std::numeric_limits<T>::lowest(); } static constexpr T Min() { return std::numeric_limits<T>::min(); } };

// ---------------------------------------------------------------------------------------------
// Containers
// ---------------------------------------------------------------------------------------------

template <typename T>
class TArray
{
public:
	using ElementType = T;
	std::vector<T> V;
	TArray() = default;
	TArray(std::initializer_list<T> Items) : V(Items) {}
	int32 Num() const { return int32(V.size()); }
	bool IsEmpty() const { return V.empty(); }
	int32 Add(const T& Item) { V.push_back(Item); return Num() - 1; }
	int32 Add(T&& Item) { V.push_back(std::move(Item)); return Num() - 1; }
	int32 AddUnique(const T& Item) { for (int32 I = 0; I < Num(); ++I) if (V[size_t(I)] == Item) return I; return Add(Item); }
	T& AddDefaulted_GetRef() { V.emplace_back(); return V.back(); }
	void Reset() { V.clear(); }
	void Empty(int32 Slack = 0) { (void)Slack; V.clear(); }
	void Reserve(int32 Count) { V.reserve(size_t(Count)); }
	void Init(const T& Value, int32 Count) { V.assign(size_t(Count), Value); }
	T& operator[](int32 I) { return V[size_t(I)]; }
	const T& operator[](int32 I) const { return V[size_t(I)]; }
	T& Last() { return V.back(); }
	const T& Last() const { return V.back(); }
	T Pop() { T Out = V.back(); V.pop_back(); return Out; }
	template <typename U> bool Contains(const U& Item) const { for (const T& E : V) if (E == Item) return true; return false; }
	template <typename P> bool ContainsByPredicate(P Pred) const { for (const T& E : V) if (Pred(E)) return true; return false; }
	template <typename P> T* FindByPredicate(P Pred) { for (T& E : V) if (Pred(E)) return &E; return nullptr; }
	template <typename P> const T* FindByPredicate(P Pred) const { for (const T& E : V) if (Pred(E)) return &E; return nullptr; }
	template <typename U> int32 Find(const U& Item) const { for (int32 I = 0; I < Num(); ++I) if (V[size_t(I)] == Item) return I; return INDEX_NONE; }
	template <typename P> int32 IndexOfByPredicate(P Pred) const { for (int32 I = 0; I < Num(); ++I) if (Pred(V[size_t(I)])) return I; return INDEX_NONE; }
	template <typename P> TArray FilterByPredicate(P Pred) const { TArray Out; for (const T& E : V) if (Pred(E)) Out.Add(E); return Out; }
	void Append(const TArray& Other) { V.insert(V.end(), Other.V.begin(), Other.V.end()); }
	void SetNum(int32 Count) { V.resize(size_t(Count)); }
	bool IsValidIndex(int32 I) const { return I >= 0 && I < Num(); }
	void Insert(const T& Item, int32 Index) { V.insert(V.begin() + Index, Item); }
	template <typename P> int32 RemoveAll(P Pred) { const size_t Before = V.size(); V.erase(std::remove_if(V.begin(), V.end(), Pred), V.end()); return int32(Before - V.size()); }
	int32 Remove(const T& Item) { return RemoveAll([&](const T& E) { return E == Item; }); }
	void RemoveAt(int32 Index) { V.erase(V.begin() + Index); }
	void Sort() { std::sort(V.begin(), V.end()); }
	template <typename P> void Sort(P Pred) { std::sort(V.begin(), V.end(), Pred); }
	template <typename P> void StableSort(P Pred) { std::stable_sort(V.begin(), V.end(), Pred); }
	T* GetData() { return V.data(); }
	const T* GetData() const { return V.data(); }
	auto begin() { return V.begin(); }
	auto end() { return V.end(); }
	auto begin() const { return V.begin(); }
	auto end() const { return V.end(); }
	bool operator==(const TArray& Other) const { return V == Other.V; }
};

template <typename T, int N = 24>
class TInlineAllocatorArray : public TArray<T> {};

template <typename T>
class TArrayView
{
public:
	const T* Data = nullptr;
	int32 Count = 0;
	int32 Num() const { return Count; }
	const T& operator[](int32 I) const { return Data[I]; }
};

template <typename K, typename V>
struct TPair
{
	K Key;
	V Value;
};

template <typename K, typename V>
class TMap
{
public:
	std::vector<TPair<K, V>> Items;
	V* Find(const K& Key) { for (auto& P : Items) if (P.Key == Key) return &P.Value; return nullptr; }
	const V* Find(const K& Key) const { for (auto& P : Items) if (P.Key == Key) return &P.Value; return nullptr; }
	V& Add(const K& Key, const V& Value) { if (V* Found = Find(Key)) { *Found = Value; return *Found; } Items.push_back({Key, Value}); return Items.back().Value; }
	V& Add(const K& Key) { return Add(Key, V()); }
	V& FindOrAdd(const K& Key) { if (V* Found = Find(Key)) return *Found; return Add(Key, V()); }
	int32 Remove(const K& Key) { const size_t Before = Items.size(); Items.erase(std::remove_if(Items.begin(), Items.end(), [&](const TPair<K, V>& P) { return P.Key == Key; }), Items.end()); return int32(Before - Items.size()); }
	bool Contains(const K& Key) const { return Find(Key) != nullptr; }
	int32 Num() const { return int32(Items.size()); }
	void Empty() { Items.clear(); }
	void Reset() { Items.clear(); }
	template <typename P> void ValueSort(P Pred) { std::sort(Items.begin(), Items.end(), [&](const TPair<K, V>& L, const TPair<K, V>& R) { return Pred(L.Value, R.Value); }); }
	auto begin() { return Items.begin(); }
	auto end() { return Items.end(); }
	auto begin() const { return Items.begin(); }
	auto end() const { return Items.end(); }
};

template <typename T>
class TSet
{
public:
	std::vector<T> Items;
	TSet() = default;
	TSet(std::initializer_list<T> List) : Items(List) {}
	void Add(const T& Item) { if (!Contains(Item)) Items.push_back(Item); }
	bool Contains(const T& Item) const { for (const T& E : Items) if (E == Item) return true; return false; }
	int32 Num() const { return int32(Items.size()); }
	void Empty() { Items.clear(); }
	void Reset() { Items.clear(); }
	auto begin() const { return Items.begin(); }
	auto end() const { return Items.end(); }
};

template <typename T>
class TOptional
{
public:
	std::optional<T> Value;
	TOptional() = default;
	TOptional(const T& In) : Value(In) {}
	TOptional& operator=(const T& In) { Value = In; return *this; }
	bool IsSet() const { return Value.has_value(); }
	explicit operator bool() const { return Value.has_value(); }
	T& GetValue() { return *Value; }
	const T& GetValue() const { return *Value; }
	T* operator->() { return &*Value; }
	const T* operator->() const { return &*Value; }
	T& operator*() { return *Value; }
	const T& operator*() const { return *Value; }
	void Reset() { Value.reset(); }
};

template <typename Signature> using TFunction = std::function<Signature>;
template <typename Signature> using TFunctionRef = std::function<Signature>;
template <typename T> using TUniquePtr = std::unique_ptr<T>;
template <typename T, typename... A> TUniquePtr<T> MakeUnique(A&&... Args) { return std::make_unique<T>(std::forward<A>(Args)...); }

template <typename T> class TSharedRef;

template <typename T>
class TSharedPtr
{
public:
	std::shared_ptr<T> P;
	TSharedPtr() = default;
	TSharedPtr(std::nullptr_t) {}
	TSharedPtr(std::shared_ptr<T> In) : P(std::move(In)) {}
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>> TSharedPtr(const TSharedPtr<U>& In) : P(In.P) {}
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>> TSharedPtr(const TSharedRef<U>& In) : P(In.P) {}
	T* Get() const { return P.get(); }
	bool IsValid() const { return P != nullptr; }
	explicit operator bool() const { return P != nullptr; }
	T* operator->() const { return P.get(); }
	T& operator*() const { return *P; }
	void Reset() { P.reset(); }
	TSharedRef<T> ToSharedRef() const;
	bool operator==(const TSharedPtr& Other) const { return P == Other.P; }
	bool operator!=(const TSharedPtr& Other) const { return P != Other.P; }
};

template <typename T>
class TSharedRef
{
public:
	std::shared_ptr<T> P;
	TSharedRef(std::shared_ptr<T> In) : P(std::move(In)) {}
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>> TSharedRef(const TSharedRef<U>& In) : P(In.P) {}
	T* operator->() const { return P.get(); }
	T& Get() const { return *P; }
	T& operator*() const { return *P; }
	bool operator==(const TSharedRef& Other) const { return P == Other.P; }
};

template <typename T> TSharedRef<T> TSharedPtr<T>::ToSharedRef() const { return TSharedRef<T>(P); }
template <typename T, typename... A> TSharedRef<T> MakeShared(A&&... Args) { return TSharedRef<T>(std::make_shared<T>(std::forward<A>(Args)...)); }
template <typename T> TSharedRef<T> MakeShareable(T* Raw) { return TSharedRef<T>(std::shared_ptr<T>(Raw)); }

template <typename T>
class TSharedFromThis : public std::enable_shared_from_this<T>
{
public:
	TSharedRef<T> AsShared() { return TSharedRef<T>(this->shared_from_this()); }
};

template <typename T>
class TGuardValue
{
public:
	TGuardValue(T& Ref, const T& Value) : Target(Ref), Old(Ref) { Ref = Value; }
	~TGuardValue() { Target = Old; }

private:
	T& Target;
	T Old;
};


// ---------------------------------------------------------------------------------------------
// Format string checks: each specifier must match its argument's type, as UE's format string
// sanitizer demands. Integers wider than 32 bits need 'll'; %s needs a TCHAR pointer.
// ---------------------------------------------------------------------------------------------

namespace UEStubFormat
{
	enum class EKind { Int32, Int64, Float, String, Pointer, Other };
	template <typename T> constexpr EKind KindOf()
	{
		using D = std::decay_t<T>;
		if constexpr (std::is_same_v<D, const wchar_t*> || std::is_same_v<D, wchar_t*>) return EKind::String;
		else if constexpr (std::is_floating_point_v<D>) return EKind::Float;
		else if constexpr (std::is_integral_v<D> || std::is_enum_v<D>) return sizeof(D) == 8 ? EKind::Int64 : EKind::Int32;
		else if constexpr (std::is_pointer_v<D>) return EKind::Pointer;
		else return EKind::Other;
	}
	inline void Fail(const char*) {}
	template <size_t N, typename... A> consteval bool Check(const wchar_t (&Fmt)[N])
	{
		constexpr EKind Kinds[] = {KindOf<A>()..., EKind::Other};
		constexpr size_t Count = sizeof...(A);
		size_t Next = 0;
		for (size_t I = 0; I + 1 < N && Fmt[I]; ++I)
		{
			if (Fmt[I] != L'%')
			{
				continue;
			}
			++I;
			if (Fmt[I] == L'%')
			{
				continue;
			}
			while (Fmt[I] == L'-' || Fmt[I] == L'+' || Fmt[I] == L' ' || Fmt[I] == L'#' || Fmt[I] == L'0')
			{
				++I;
			}
			if (Fmt[I] == L'*')
			{
				if (Next >= Count || Kinds[Next] != EKind::Int32) throw "a '*' width needs an int32 argument";
				++Next;
				++I;
			}
			while (Fmt[I] >= L'0' && Fmt[I] <= L'9') ++I;
			if (Fmt[I] == L'.')
			{
				++I;
				if (Fmt[I] == L'*')
				{
					if (Next >= Count || Kinds[Next] != EKind::Int32) throw "a '*' precision needs an int32 argument";
					++Next;
					++I;
				}
				while (Fmt[I] >= L'0' && Fmt[I] <= L'9') ++I;
			}
			bool bLong = false;
			if (Fmt[I] == L'l' && Fmt[I + 1] == L'l') { bLong = true; I += 2; }
			else if (Fmt[I] == L'I' && Fmt[I + 1] == L'6' && Fmt[I + 2] == L'4') { bLong = true; I += 3; }
			else if (Fmt[I] == L'l' || Fmt[I] == L'h' || Fmt[I] == L'z') { throw "unsupported length modifier"; }
			if (Next >= Count) throw "more format specifiers than arguments";
			const EKind Kind = Kinds[Next++];
			switch (Fmt[I])
			{
			case L'd': case L'i': case L'u': case L'x': case L'X': case L'c':
				if (Kind != (bLong ? EKind::Int64 : EKind::Int32)) throw "integer specifier does not match the argument (int64 needs %lld)";
				break;
			case L'f': case L'g': case L'e': case L'G': case L'E':
				if (Kind != EKind::Float) throw "%f needs a float or double";
				break;
			case L's':
				if (Kind != EKind::String) throw "%s needs a TCHAR pointer (dereference FStrings with *)";
				break;
			case L'p':
				if (Kind != EKind::Pointer) throw "%p needs a pointer";
				break;
			default:
				throw "unknown format specifier";
			}
		}
		if (Next != Count) throw "more arguments than format specifiers";
		return true;
	}
	template <typename... A>
	struct TChecked
	{
		const wchar_t* Fmt;
		template <size_t N> consteval TChecked(const wchar_t (&In)[N]) : Fmt(In) { Check<N, A...>(In); }
	};
}

// ---------------------------------------------------------------------------------------------
// Strings
// ---------------------------------------------------------------------------------------------

class FString
{
public:
	std::wstring S;
	FString() = default;
	FString(const TCHAR* In) : S(In ? In : L"") {}
	FString(const ANSICHAR* In) { if (In) { while (*In) S.push_back(TCHAR(*In++)); } }
	explicit FString(const std::wstring& In) : S(In) {}
	int32 Len() const { return int32(S.size()); }
	bool IsEmpty() const { return S.empty(); }
	void Reset() { S.clear(); }
	void Empty() { S.clear(); }
	const TCHAR* operator*() const { return S.c_str(); }
	TCHAR operator[](int32 I) const { return S[size_t(I)]; }
	TCHAR& operator[](int32 I) { return S[size_t(I)]; }
	void AppendChar(TCHAR C) { S.push_back(C); }
	FString& operator+=(const FString& Other) { S += Other.S; return *this; }
	FString& operator+=(const TCHAR* Other) { S += Other; return *this; }
	friend FString operator+(const FString& A, const FString& B) { return FString(A.S + B.S); }
	friend FString operator+(const FString& A, const TCHAR* B) { return FString(A.S + B); }
	friend FString operator+(const TCHAR* A, const FString& B) { return FString(A + B.S); }
	bool Equals(const FString& Other, ESearchCase::Type Case = ESearchCase::CaseSensitive) const { (void)Case; return S == Other.S; }
	bool Contains(const FString& Sub, ESearchCase::Type Case = ESearchCase::IgnoreCase, ESearchDir::Type Dir = ESearchDir::FromStart) const { (void)Case; (void)Dir; return S.find(Sub.S) != std::wstring::npos; }
	FString Replace(const TCHAR* From, const TCHAR* To, ESearchCase::Type Case = ESearchCase::IgnoreCase) const { (void)From; (void)To; (void)Case; return *this; }
	template <typename... A> static FString Printf(UEStubFormat::TChecked<std::type_identity_t<A>...> Format, A... Args) { (void)sizeof...(Args); return FString(Format.Fmt); }
	template <typename R> static FString Join(const R& Range, const TCHAR* Separator) { FString Out; bool bFirst = true; for (const auto& E : Range) { if (!bFirst) Out += Separator; Out += E; bFirst = false; } return Out; }
	template <typename R, typename P> static FString JoinBy(const R& Range, const TCHAR* Separator, P Projection) { FString Out; for (const auto& E : Range) { Out += Projection(E); Out += Separator; } return Out; }
	bool operator==(const FString& Other) const { return S == Other.S; }
	bool operator!=(const FString& Other) const { return S != Other.S; }
	bool operator<(const FString& Other) const { return S < Other.S; }
};
inline FString operator/(const FString& A, const TCHAR* B) { return A + TEXT("/") + B; }

class FName
{
public:
	std::wstring S;
	FName() = default;
	FName(const TCHAR* In) : S(In ? In : L"") {}
	FName(const ANSICHAR* In) { if (In) { while (*In) S.push_back(TCHAR(*In++)); } }
	explicit FName(const FString& In) : S(In.S) {}
	bool IsNone() const { return S.empty(); }
	FString ToString() const { return FString(S); }
	bool operator==(const FName& Other) const { return S == Other.S; }
	bool operator!=(const FName& Other) const { return S != Other.S; }
	bool operator==(const TCHAR* Other) const { return S == Other; }
	bool operator!=(const TCHAR* Other) const { return S != Other; }
	bool operator<(const FName& Other) const { return S < Other.S; }
};
#define NAME_None FName()

struct FNumberFormattingOptions
{
	FNumberFormattingOptions& SetMinimumFractionalDigits(int32) { return *this; }
	FNumberFormattingOptions& SetMaximumFractionalDigits(int32) { return *this; }
};

class FText;
struct FFormatArgumentValue
{
	FFormatArgumentValue(int32) {}
	FFormatArgumentValue(int64) {}
	FFormatArgumentValue(uint32) {}
	FFormatArgumentValue(double) {}
	FFormatArgumentValue(float) {}
	FFormatArgumentValue(const FText&) {}
};
struct FFormatNamedArguments
{
	void Add(const FString&, const FFormatArgumentValue&) {}
};

class FText
{
public:
	FString S;
	static FText FromString(const FString& In) { FText T; T.S = In; return T; }
	static const FText& GetEmpty() { static FText Empty; return Empty; }
	bool IsEmpty() const { return S.IsEmpty(); }
	FString ToString() const { return S; }
	static FText AsNumber(double, const FNumberFormattingOptions* = nullptr) { return FText(); }
	static FText AsNumber(float, const FNumberFormattingOptions* = nullptr) { return FText(); }
	static FText AsNumber(int32, const FNumberFormattingOptions* = nullptr) { return FText(); }
	static FText AsNumber(int64, const FNumberFormattingOptions* = nullptr) { return FText(); }
	static FText Format(const FText& Pattern, const FFormatNamedArguments&) { return Pattern; }
	template <typename... A> static FText Format(const FText& Pattern, const A&... Args) { (void)std::initializer_list<int>{(FFormatArgumentValue(Args), 0)...}; return Pattern; }
};

struct FUtf8ToTchar
{
	std::wstring S;
	explicit FUtf8ToTchar(const char* In) { if (In) { while (*In) S.push_back(TCHAR(*In++)); } }
	const TCHAR* Get() const { return S.c_str(); }
};
struct FTcharToUtf8
{
	std::string S;
	explicit FTcharToUtf8(const TCHAR* In) { if (In) { while (*In) S.push_back(char(*In++)); } }
	const char* Get() const { return S.c_str(); }
};
#define UTF8_TO_TCHAR(x) (FUtf8ToTchar(x).Get())
#define TCHAR_TO_UTF8(x) (FTcharToUtf8(x).Get())

struct FChar
{
	static bool IsAlnum(TCHAR C) { return std::iswalnum(wint_t(C)) != 0; }
	static bool IsLower(TCHAR C) { return std::iswlower(wint_t(C)) != 0; }
	static bool IsUpper(TCHAR C) { return std::iswupper(wint_t(C)) != 0; }
	static bool IsDigit(TCHAR C) { return std::iswdigit(wint_t(C)) != 0; }
};

struct FParse
{
	static bool Value(const TCHAR*, const TCHAR*, FString&) { return false; }
};

struct FPlatformTime
{
	static double Seconds() { return 0; }
};

struct FPlatformProcess
{
	static void LaunchURL(const TCHAR*, const TCHAR*, FString*) {}
};

// ---------------------------------------------------------------------------------------------
// Maths
// ---------------------------------------------------------------------------------------------

struct FMath
{
	template <typename T> static T Min(T A, T B) { return A < B ? A : B; }
	template <typename T> static T Max(T A, T B) { return A > B ? A : B; }
	template <typename T> static T Min3(T A, T B, T C) { return Min(Min(A, B), C); }
	template <typename T> static T Max3(T A, T B, T C) { return Max(Max(A, B), C); }
	template <typename T> static T Abs(T A) { return A < 0 ? -A : A; }
	template <typename T> static T Clamp(T X, T Low, T High) { return X < Low ? Low : X > High ? High : X; }
	static int32 FloorToInt32(double X) { return int32(std::floor(X)); }
	static int32 RoundToInt(double X) { return int32(std::lround(X)); }
	static int32 RoundToInt(float X) { return int32(std::lround(X)); }
	static bool IsNearlyEqual(double A, double B, double Tolerance = 1e-8) { return std::abs(A - B) <= Tolerance; }
	static bool IsNearlyZero(double A, double Tolerance = 1e-8) { return std::abs(A) <= Tolerance; }
	static double RadiansToDegrees(double R) { return R * 57.29577951308232; }
	static double Atan2(double Y, double X) { return std::atan2(Y, X); }
};

struct FVector3f;

struct FVector
{
	double X = 0, Y = 0, Z = 0;
	static const FVector ZeroVector;
	static const FVector UpVector;
	static const FVector RightVector;
	FVector() = default;
	FVector(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
	explicit FVector(const FVector3f& V);
	FVector operator+(const FVector& O) const { return {X + O.X, Y + O.Y, Z + O.Z}; }
	FVector operator-(const FVector& O) const { return {X - O.X, Y - O.Y, Z - O.Z}; }
	FVector operator*(double S) const { return {X * S, Y * S, Z * S}; }
	FVector operator/(double S) const { return {X / S, Y / S, Z / S}; }
	FVector operator-() const { return {-X, -Y, -Z}; }
	FVector& operator+=(const FVector& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
	FVector& operator-=(const FVector& O) { X -= O.X; Y -= O.Y; Z -= O.Z; return *this; }
	double& operator[](int32 I) { return I == 0 ? X : I == 1 ? Y : Z; }
	double operator[](int32 I) const { return I == 0 ? X : I == 1 ? Y : Z; }
	double Size() const { return std::sqrt(X * X + Y * Y + Z * Z); }
	double GetMax() const { return std::max({X, Y, Z}); }
	FVector GetSafeNormal() const { const double S = Size(); return S > 0 ? *this / S : FVector(); }
	bool IsNearlyZero(double Tolerance = 1e-4) const { return Size() <= Tolerance; }
	bool Equals(const FVector& O, double Tolerance = 1e-4) const { return std::abs(X - O.X) <= Tolerance && std::abs(Y - O.Y) <= Tolerance && std::abs(Z - O.Z) <= Tolerance; }
	bool ContainsNaN() const { return std::isnan(X) || std::isnan(Y) || std::isnan(Z); }
	FString ToString() const { return FString(); }
	static double Dist(const FVector& A, const FVector& B) { return (A - B).Size(); }
	static FVector CrossProduct(const FVector& A, const FVector& B) { return {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X}; }
	static double DotProduct(const FVector& A, const FVector& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
};
inline const FVector FVector::ZeroVector(0, 0, 0);
inline const FVector FVector::UpVector(0, 0, 1);
inline const FVector FVector::RightVector(0, 1, 0);

struct FVector3f
{
	float X = 0, Y = 0, Z = 0;
	FVector3f() = default;
	FVector3f(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}
	explicit FVector3f(const FVector& V) : X(float(V.X)), Y(float(V.Y)), Z(float(V.Z)) {}
	FVector3f operator+(const FVector3f& O) const { return {X + O.X, Y + O.Y, Z + O.Z}; }
	FVector3f operator-(const FVector3f& O) const { return {X - O.X, Y - O.Y, Z - O.Z}; }
	FVector3f operator/(float S) const { return {X / S, Y / S, Z / S}; }
	float Size() const { return std::sqrt(X * X + Y * Y + Z * Z); }
	FVector3f GetSafeNormal() const { return *this; }
	static FVector3f CrossProduct(const FVector3f& A, const FVector3f& B) { return {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X}; }
	static float DotProduct(const FVector3f& A, const FVector3f& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
};
inline FVector::FVector(const FVector3f& V) : X(V.X), Y(V.Y), Z(V.Z) {}

struct FVector2D
{
	double X = 0, Y = 0;
	FVector2D() = default;
	FVector2D(double InX, double InY) : X(InX), Y(InY) {}
};

struct FIntPoint
{
	int32 X = 0, Y = 0;
	FIntPoint() = default;
	FIntPoint(int32 InX, int32 InY) : X(InX), Y(InY) {}
	bool operator==(const FIntPoint& O) const { return X == O.X && Y == O.Y; }
};

struct FRotator
{
	double Pitch = 0, Yaw = 0, Roll = 0;
	static const FRotator ZeroRotator;
	FRotator() = default;
	FRotator(double InPitch, double InYaw, double InRoll) : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}
	FString ToString() const { return FString(); }
};
inline const FRotator FRotator::ZeroRotator;

struct FTransform
{
	FTransform() = default;
	FTransform(const FRotator&, const FVector&, const FVector& = FVector(1, 1, 1)) {}
	FVector TransformPosition(const FVector& P) const { return P; }
	FVector InverseTransformPosition(const FVector& P) const { return P; }
	double GetDeterminant() const { return 1; }
	bool Equals(const FTransform&, double Tolerance = 1e-4) const { (void)Tolerance; return true; }
};

struct FBox
{
	FVector Min, Max;
	uint8 IsValid = 0;
	FBox() = default;
	explicit FBox(EForceInit) {}
	FBox(const FVector& InMin, const FVector& InMax) : Min(InMin), Max(InMax), IsValid(1) {}
	explicit FBox(const TArray<FVector>& Points) { for (const FVector& P : Points) *this += P; }
	FBox& operator+=(const FVector& P) { if (!IsValid) { Min = Max = P; IsValid = 1; } else { Min = {std::min(Min.X, P.X), std::min(Min.Y, P.Y), std::min(Min.Z, P.Z)}; Max = {std::max(Max.X, P.X), std::max(Max.Y, P.Y), std::max(Max.Z, P.Z)}; } return *this; }
	FBox ExpandBy(double W) const { return FBox(Min - FVector(W, W, W), Max + FVector(W, W, W)); }
	bool IsInside(const FVector& P) const { return P.X > Min.X && P.X < Max.X && P.Y > Min.Y && P.Y < Max.Y && P.Z > Min.Z && P.Z < Max.Z; }
	bool IsInsideOrOn(const FVector& P) const { return P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y && P.Z >= Min.Z && P.Z <= Max.Z; }
	bool Intersect(const FBox& O) const { return !(O.Min.X > Max.X || O.Max.X < Min.X || O.Min.Y > Max.Y || O.Max.Y < Min.Y || O.Min.Z > Max.Z || O.Max.Z < Min.Z); }
	FBox TransformBy(const FTransform&) const { return *this; }
	FVector GetCenter() const { return (Min + Max) * 0.5; }
};

struct FBoxSphereBounds
{
	FVector Origin;
	FVector BoxExtent;
	double SphereRadius = 0;
	FBox GetBox() const { return FBox(Origin - BoxExtent, Origin + BoxExtent); }
};

struct FColor
{
	uint8 R = 0, G = 0, B = 0, A = 255;
	FColor() = default;
	FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255) : R(InR), G(InG), B(InB), A(InA) {}
};

struct FLinearColor
{
	float R = 0, G = 0, B = 0, A = 1;
	static const FLinearColor White;
	static const FLinearColor Black;
	FLinearColor() = default;
	FLinearColor(float InR, float InG, float InB, float InA = 1) : R(InR), G(InG), B(InB), A(InA) {}
	explicit FLinearColor(const FColor& C) : R(C.R / 255.f), G(C.G / 255.f), B(C.B / 255.f), A(C.A / 255.f) {}
};
inline const FLinearColor FLinearColor::White(1, 1, 1);
inline const FLinearColor FLinearColor::Black(0, 0, 0);

// ---------------------------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------------------------

struct FDelegateHandle
{
	int32 Id = 0;
};

template <bool bConst, typename U, typename Sig> struct TMemFunPtrType;
template <typename U, typename Rt, typename... Ar> struct TMemFunPtrType<false, U, Rt(Ar...)> { using Type = Rt (U::*)(Ar...); };
template <typename U, typename Rt, typename... Ar> struct TMemFunPtrType<true, U, Rt(Ar...)> { using Type = Rt (U::*)(Ar...) const; };
template <typename T> struct TIdentity { using Type = T; };

template <typename Signature> class TDelegate;

template <typename R, typename... A>
class TDelegate<R(A...)>
{
public:
	std::function<R(A...)> F;
	template <typename U, typename... P> using TMethodPtr = typename TMemFunPtrType<false, U, R(A..., P...)>::Type;
	template <typename U, typename... P> using TConstMethodPtr = typename TMemFunPtrType<true, U, R(A..., P...)>::Type;
	template <typename L, typename... P> static TDelegate CreateLambda(L&& Fn, P... Payload)
	{
		static_assert(std::is_invocable_v<L, A..., P...>, "the lambda cannot be called with the delegate's arguments");
		if constexpr (!std::is_void_v<R>)
		{
			static_assert(std::is_convertible_v<std::invoke_result_t<L, A..., P...>, R>, "the lambda returns the wrong type");
		}
		TDelegate D;
		D.F = [Fn = std::forward<L>(Fn), Payload...](A... Args) mutable -> R { return R(Fn(Args..., Payload...)); };
		return D;
	}
	template <typename... P> static TDelegate CreateStatic(typename TIdentity<R (*)(A..., std::decay_t<P>...)>::Type Function, P&&... Payload)
	{
		TDelegate D;
		D.F = [Function, Payload...](A... Args) -> R { return Function(Args..., Payload...); };
		return D;
	}
	template <typename U, typename... P> static TDelegate CreateSP(U* Object, typename TMemFunPtrType<false, U, R(A..., std::decay_t<P>...)>::Type Method, P&&... Payload)
	{
		TDelegate D;
		D.F = [Object, Method, Payload...](A... Args) -> R { return (Object->*Method)(Args..., Payload...); };
		return D;
	}
	template <typename U, typename... P> static TDelegate CreateSP(const U* Object, typename TMemFunPtrType<true, U, R(A..., std::decay_t<P>...)>::Type Method, P&&... Payload)
	{
		TDelegate D;
		D.F = [Object, Method, Payload...](A... Args) -> R { return (Object->*Method)(Args..., Payload...); };
		return D;
	}
	template <typename U, typename... P> static TDelegate CreateRaw(U* Object, typename TMemFunPtrType<false, U, R(A..., std::decay_t<P>...)>::Type Method, P&&... Payload) { return CreateSP<U>(Object, Method, std::forward<P>(Payload)...); }
	template <typename U, typename... P> static TDelegate CreateRaw(const U* Object, typename TMemFunPtrType<true, U, R(A..., std::decay_t<P>...)>::Type Method, P&&... Payload) { return CreateSP<U>(Object, Method, std::forward<P>(Payload)...); }
	template <typename U, typename... P> static TDelegate CreateUObject(U* Object, typename TMemFunPtrType<false, U, R(A..., std::decay_t<P>...)>::Type Method, P&&... Payload) { return CreateSP<U>(Object, Method, std::forward<P>(Payload)...); }
	bool IsBound() const { return bool(F); }
	R Execute(A... Args) const { return F(Args...); }
	void ExecuteIfBound(A... Args) const { if (F) F(Args...); }
	void Unbind() { F = nullptr; }
};

template <typename... A>
class TMulticastDelegate
{
public:
	using FDelegate = TDelegate<void(A...)>;
	template <typename U, typename... P> FDelegateHandle AddSP(U* Object, typename TMemFunPtrType<false, U, void(A..., std::decay_t<P>...)>::Type Method, P&&... Payload) { (void)Object; (void)Method; return FDelegateHandle(); }
	template <typename U, typename... P> FDelegateHandle AddSP(const U* Object, typename TMemFunPtrType<true, U, void(A..., std::decay_t<P>...)>::Type Method, P&&... Payload) { (void)Object; (void)Method; return FDelegateHandle(); }
	template <typename U, typename... P> FDelegateHandle AddRaw(U* Object, typename TMemFunPtrType<false, U, void(A..., std::decay_t<P>...)>::Type Method, P&&... Payload) { (void)Object; (void)Method; return FDelegateHandle(); }
	template <typename U, typename... P> FDelegateHandle AddUObject(U* Object, typename TMemFunPtrType<false, U, void(A..., std::decay_t<P>...)>::Type Method, P&&... Payload) { (void)Object; (void)Method; return FDelegateHandle(); }
	template <typename L> FDelegateHandle AddLambda(L&& Fn) { static_assert(std::is_invocable_v<L, A...>, "AddLambda does not match"); return FDelegateHandle(); }
	FDelegateHandle Add(const FDelegate&) { return FDelegateHandle(); }
	void Remove(FDelegateHandle) {}
	void RemoveAll(const void*) {}
	void Broadcast(A... Args) const { (void)sizeof...(Args); }
};

#define DECLARE_DELEGATE(Name) using Name = TDelegate<void()>;
#define DECLARE_DELEGATE_RetVal(R, Name) using Name = TDelegate<R()>;
#define DECLARE_DELEGATE_OneParam(Name, A1) using Name = TDelegate<void(A1)>;
#define DECLARE_DELEGATE_TwoParams(Name, A1, A2) using Name = TDelegate<void(A1, A2)>;
#define DECLARE_DELEGATE_ThreeParams(Name, A1, A2, A3) using Name = TDelegate<void(A1, A2, A3)>;
#define DECLARE_DELEGATE_RetVal_OneParam(R, Name, A1) using Name = TDelegate<R(A1)>;
#define DECLARE_DELEGATE_RetVal_TwoParams(R, Name, A1, A2) using Name = TDelegate<R(A1, A2)>;
#define DECLARE_MULTICAST_DELEGATE(Name) using Name = TMulticastDelegate<>;
#define DECLARE_MULTICAST_DELEGATE_OneParam(Name, A1) using Name = TMulticastDelegate<A1>;
#define DECLARE_MULTICAST_DELEGATE_TwoParams(Name, A1, A2) using Name = TMulticastDelegate<A1, A2>;

using FSimpleDelegate = TDelegate<void()>;
using FSimpleMulticastDelegate = TMulticastDelegate<>;

template <typename T>
class TAttribute
{
public:
	using FGetter = TDelegate<T()>;
	TAttribute() = default;
	TAttribute(const T&) {}
	template <typename U, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, TAttribute> && std::is_constructible_v<T, const U&>>> TAttribute(const U&) {}
	template <typename L> static TAttribute CreateLambda(L&& Fn) { static_assert(std::is_convertible_v<std::invoke_result_t<L>, T>, "attribute lambda returns the wrong type"); return TAttribute(); }
	static TAttribute Create(const FGetter&) { return TAttribute(); }
	template <typename U> static TAttribute CreateSP(U* Object, typename TMemFunPtrType<true, U, T()>::Type Getter) { (void)Object; (void)Getter; return TAttribute(); }
	T Get() const { return T(); }
	bool IsSet() const { return true; }
};
