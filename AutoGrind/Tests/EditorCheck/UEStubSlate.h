// Compile-check stubs: Slate's declarative syntax and the widgets, styles, menus, tabs, notifications and
// details views the AutoGrind panel uses. Arguments, events and slots follow UE 5.4's names and types.
#pragma once

#include "UEStubEngine.h"

// ---------------------------------------------------------------------------------------------
// Basic Slate types
// ---------------------------------------------------------------------------------------------

enum EHorizontalAlignment { HAlign_Fill, HAlign_Left, HAlign_Center, HAlign_Right };
enum EVerticalAlignment { VAlign_Fill, VAlign_Top, VAlign_Center, VAlign_Bottom };
enum EOrientation { Orient_Horizontal, Orient_Vertical };
enum class ECheckBoxState : uint8 { Unchecked, Checked, Undetermined };
namespace ESelectionMode { enum Type { None, Single, SingleToggle, Multi }; }
namespace ESelectInfo { enum Type { OnKeyPress, OnNavigation, OnMouseClick, Direct }; }
namespace EColumnSortMode { enum Type { None, Ascending, Descending }; }
namespace EColumnSortPriority { enum Type { None, Primary, Secondary, Max }; }
enum ETabRole : uint8 { MajorTab, PanelTab, NomadTab, DocumentTab, NumRoles };
namespace ETabSpawnerMenuType { enum Type { Enabled, Disabled, Hidden }; }
enum class EUserInterfaceActionType : uint8 { None, Button, ToggleButton, RadioButton, Check, CollapsedButton };

struct EVisibility
{
	static const EVisibility Visible;
	static const EVisibility Collapsed;
	static const EVisibility Hidden;
	static const EVisibility HitTestInvisible;
	static const EVisibility SelfHitTestInvisible;
	uint8 Value = 0;
	bool operator==(const EVisibility& Other) const { return Value == Other.Value; }
};
inline const EVisibility EVisibility::Visible{0};
inline const EVisibility EVisibility::Collapsed{1};
inline const EVisibility EVisibility::Hidden{2};
inline const EVisibility EVisibility::HitTestInvisible{3};
inline const EVisibility EVisibility::SelfHitTestInvisible{4};

struct FMargin
{
	float Left = 0, Top = 0, Right = 0, Bottom = 0;
	FMargin() = default;
	FMargin(float Uniform) : Left(Uniform), Top(Uniform), Right(Uniform), Bottom(Uniform) {}
	FMargin(float Horizontal, float Vertical) : Left(Horizontal), Top(Vertical), Right(Horizontal), Bottom(Vertical) {}
	FMargin(float InLeft, float InTop, float InRight, float InBottom) : Left(InLeft), Top(InTop), Right(InRight), Bottom(InBottom) {}
};

struct FSlateColor
{
	FSlateColor() = default;
	FSlateColor(const FLinearColor&) {}
	static FSlateColor UseForeground() { return FSlateColor(); }
	static FSlateColor UseSubduedForeground() { return FSlateColor(); }
};

struct FOptionalSize
{
	FOptionalSize() = default;
	FOptionalSize(float) {}
};

struct FSlateFontInfo {};
struct FSlateBrush
{
	virtual ~FSlateBrush() = default;
};
struct FSlateImageBrush : FSlateBrush
{
	FSlateImageBrush(const FString&, const FVector2D&, const FLinearColor& Tint = FLinearColor(1, 1, 1, 1)) { (void)Tint; }
	FSlateImageBrush(const FName&, const FVector2D&, const FLinearColor& Tint = FLinearColor(1, 1, 1, 1)) { (void)Tint; }
};
struct FSlateWidgetStyle {};
struct FButtonStyle : FSlateWidgetStyle {};
struct FCheckBoxStyle : FSlateWidgetStyle {};
struct FTableRowStyle : FSlateWidgetStyle {};
struct FTextBlockStyle : FSlateWidgetStyle {};

class FGeometry {};
class FInputEvent
{
public:
	bool IsControlDown() const { return false; }
	bool IsShiftDown() const { return false; }
	bool IsAltDown() const { return false; }
};
class FKeyEvent : public FInputEvent
{
public:
	FKey GetKey() const { return FKey(); }
};
class FPointerEvent : public FInputEvent {};

class FReply
{
public:
	static FReply Handled() { return FReply(); }
	static FReply Unhandled() { return FReply(); }
};

// ---------------------------------------------------------------------------------------------
// Styles and icons
// ---------------------------------------------------------------------------------------------

class ISlateStyle
{
public:
	virtual ~ISlateStyle() = default;
	const FSlateBrush* GetBrush(const FName, const ANSICHAR* Specifier = nullptr) const { (void)Specifier; return nullptr; }
	template <typename T> const T& GetWidgetStyle(const FName, const ANSICHAR* Specifier = nullptr) const
	{
		static_assert(std::is_base_of_v<FSlateWidgetStyle, T>, "not a widget style");
		(void)Specifier;
		static T Style;
		return Style;
	}
	FSlateColor GetSlateColor(const FName) const { return FSlateColor(); }
	FSlateFontInfo GetFontStyle(const FName) const { return FSlateFontInfo(); }
	const FName& GetStyleSetName() const { static FName Name; return Name; }
};

class FAppStyle
{
public:
	static const ISlateStyle& Get() { static ISlateStyle Style; return Style; }
	static const FName GetAppStyleSetName() { return FName(); }
};

class FCoreStyle
{
public:
	static const ISlateStyle& Get() { static ISlateStyle Style; return Style; }
	static FSlateFontInfo GetDefaultFontStyle(const FName, const int32 Size) { (void)Size; return FSlateFontInfo(); }
};

class FSlateStyleSet : public ISlateStyle
{
public:
	explicit FSlateStyleSet(const FName&) {}
	void SetContentRoot(const FString&) {}
	FString RootToContentDir(const ANSICHAR*, const TCHAR*) { return FString(); }
	FString RootToContentDir(const WIDECHAR*, const TCHAR*) { return FString(); }
	FString RootToContentDir(const FString&, const TCHAR*) { return FString(); }
	void Set(const FName, FSlateBrush*) {}
	void Set(const FName, const FLinearColor&) {}
};

class FSlateStyleRegistry
{
public:
	static bool RegisterSlateStyle(const ISlateStyle&) { return true; }
	static bool UnRegisterSlateStyle(const ISlateStyle&) { return true; }
	static bool UnRegisterSlateStyle(const FName) { return true; }
};

class FSlateIcon
{
public:
	FSlateIcon() = default;
	FSlateIcon(const FName& StyleSetName, const FName& StyleName) { (void)StyleSetName; (void)StyleName; }
	FSlateIcon(const FName& StyleSetName, const FName& StyleName, const FName& SmallStyleName) { (void)StyleSetName; (void)StyleName; (void)SmallStyleName; }
	const FSlateBrush* GetIcon() const { return nullptr; }
	const FSlateBrush* GetSmallIcon() const { return nullptr; }
	bool IsSet() const { return true; }
};

// ---------------------------------------------------------------------------------------------
// Declarative syntax
// ---------------------------------------------------------------------------------------------

class SWidget;

// The arguments every widget takes.
template <typename ArgsType>
struct TSlateBaseNamedArgs
{
	using WidgetArgsType = ArgsType;
	ArgsType& Me() { return static_cast<ArgsType&>(*this); }

	TAttribute<FText> _ToolTipText;
	ArgsType& ToolTipText(const TAttribute<FText>& In) { _ToolTipText = In; return Me(); }
	template <typename L> ArgsType& ToolTipText_Lambda(L&& Fn) { _ToolTipText = TAttribute<FText>::CreateLambda(std::forward<L>(Fn)); return Me(); }

	TAttribute<bool> _IsEnabled;
	ArgsType& IsEnabled(const TAttribute<bool>& In) { _IsEnabled = In; return Me(); }
	template <typename L> ArgsType& IsEnabled_Lambda(L&& Fn) { _IsEnabled = TAttribute<bool>::CreateLambda(std::forward<L>(Fn)); return Me(); }

	TAttribute<EVisibility> _Visibility;
	ArgsType& Visibility(const TAttribute<EVisibility>& In) { _Visibility = In; return Me(); }
	template <typename L> ArgsType& Visibility_Lambda(L&& Fn) { _Visibility = TAttribute<EVisibility>::CreateLambda(std::forward<L>(Fn)); return Me(); }
};

#define SLATE_BEGIN_ARGS(InWidgetType) \
	public: \
	struct FArguments : public TSlateBaseNamedArgs<FArguments> \
	{ \
		using WidgetArgsType = FArguments; \
		FArguments()

#define SLATE_END_ARGS() \
	};

#define SLATE_ARGUMENT(ArgType, ArgName) \
	ArgType _##ArgName{}; \
	WidgetArgsType& ArgName(ArgType InArg) { _##ArgName = InArg; return this->Me(); }

#define SLATE_ATTRIBUTE(AttrType, AttrName) \
	TAttribute<AttrType> _##AttrName; \
	WidgetArgsType& AttrName(const TAttribute<AttrType>& In) { _##AttrName = In; return this->Me(); } \
	template <typename L> WidgetArgsType& AttrName##_Lambda(L&& Fn) { _##AttrName = TAttribute<AttrType>::CreateLambda(std::forward<L>(Fn)); return this->Me(); } \
	template <typename U> WidgetArgsType& AttrName(U* Object, typename TMemFunPtrType<true, U, AttrType()>::Type Getter) { (void)Object; (void)Getter; return this->Me(); }

#define SLATE_STYLE_ARGUMENT(ArgType, ArgName) \
	const ArgType* _##ArgName = nullptr; \
	WidgetArgsType& ArgName(const ArgType* InArg) { _##ArgName = InArg; return this->Me(); }

#define SLATE_EVENT(DelegateName, EventName) \
	DelegateName _##EventName; \
	WidgetArgsType& EventName(const DelegateName& InDelegate) { _##EventName = InDelegate; return this->Me(); } \
	template <typename L, typename... P> WidgetArgsType& EventName##_Lambda(L&& Fn, P... Payload) { _##EventName = DelegateName::CreateLambda(std::forward<L>(Fn), Payload...); return this->Me(); } \
	template <typename F, typename... P> WidgetArgsType& EventName##_Static(F Fn, P&&... Payload) { _##EventName = DelegateName::CreateStatic(Fn, std::forward<P>(Payload)...); return this->Me(); } \
	template <typename U, typename M, typename... P> WidgetArgsType& EventName(U* Object, M Method, P&&... Payload) { _##EventName = DelegateName::template CreateSP<U>(Object, Method, std::forward<P>(Payload)...); return this->Me(); } \
	template <typename U, typename M, typename... P> WidgetArgsType& EventName##_Raw(U* Object, M Method, P&&... Payload) { _##EventName = DelegateName::template CreateRaw<U>(Object, Method, std::forward<P>(Payload)...); return this->Me(); }

template <typename ArgsType>
struct TNamedSlotProperty
{
	ArgsType& Owner;
	TSharedPtr<SWidget>& Slot;
	ArgsType& operator[](const TSharedRef<SWidget>& Child) { Slot = Child; return Owner; }
};

#define SLATE_NAMED_SLOT(DeclarationType, SlotName) \
	TSharedPtr<SWidget> _##SlotName; \
	TNamedSlotProperty<WidgetArgsType> SlotName() { return TNamedSlotProperty<WidgetArgsType>{this->Me(), _##SlotName}; } \
	WidgetArgsType& SlotName(const TSharedRef<SWidget>& InChild) { _##SlotName = InChild; return this->Me(); }

#define SLATE_DEFAULT_SLOT(DeclarationType, SlotName) \
	SLATE_NAMED_SLOT(DeclarationType, SlotName) \
	WidgetArgsType& operator[](const TSharedRef<SWidget>& InChild) { _##SlotName = InChild; return this->Me(); }

#define SLATE_SLOT_ARGUMENT(SlotArgsType, SlotName) \
	std::vector<SlotArgsType> _##SlotName; \
	WidgetArgsType& operator+(const SlotArgsType& Slot) { _##SlotName.push_back(Slot); return this->Me(); }

// Holds what SNew and SAssignNew were given until the arguments arrive.
template <typename WidgetType, typename... RequiredTypes>
struct TSlateDecl
{
	std::tuple<RequiredTypes...> Required;
	TSharedPtr<WidgetType>* Exposed = nullptr;

	template <typename ExposeType> TSlateDecl&& Expose(TSharedPtr<ExposeType>& Into) &&
	{
		static_assert(std::is_convertible_v<WidgetType*, ExposeType*>, "SAssignNew target cannot hold this widget");
		Exposer = [&Into](const TSharedRef<WidgetType>& Widget) { Into = TSharedPtr<ExposeType>(TSharedRef<ExposeType>(Widget)); };
		return std::move(*this);
	}
	std::function<void(const TSharedRef<WidgetType>&)> Exposer;

	TSharedRef<WidgetType> operator<<=(const typename WidgetType::FArguments& Args) &&
	{
		TSharedRef<WidgetType> Widget(std::make_shared<WidgetType>());
		std::apply([&](auto&... Values) { Widget->Construct(Args, Values...); }, Required);
		if (Exposer)
		{
			Exposer(Widget);
		}
		return Widget;
	}
};

template <typename WidgetType, typename... RequiredTypes>
TSlateDecl<WidgetType, std::decay_t<RequiredTypes>...> MakeTDecl(RequiredTypes&&... Values)
{
	return TSlateDecl<WidgetType, std::decay_t<RequiredTypes>...>{std::make_tuple(std::forward<RequiredTypes>(Values)...)};
}

#define SNew(WidgetType, ...) MakeTDecl<WidgetType>(__VA_ARGS__) <<= WidgetType::FArguments()
#define SAssignNew(ExposeAs, WidgetType, ...) MakeTDecl<WidgetType>(__VA_ARGS__).Expose(ExposeAs) <<= WidgetType::FArguments()

// ---------------------------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------------------------

class SWidget : public TSharedFromThis<SWidget>
{
public:
	virtual ~SWidget() = default;
	virtual bool SupportsKeyboardFocus() const { return false; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) { (void)MyGeometry; (void)InKeyEvent; return FReply::Unhandled(); }
	bool HasKeyboardFocus() const { return false; }
	bool HasFocusedDescendants() const { return false; }
	void SetToolTipText(const TAttribute<FText>&) {}
	void SetEnabled(const TAttribute<bool>&) {}
	void SetVisibility(TAttribute<EVisibility>) {}
	void Invalidate(int32 = 0) {}
};

class SNullWidget
{
public:
	static inline TSharedRef<SWidget> NullWidget{std::make_shared<SWidget>()};
};

struct FCompoundWidgetOneChildSlot
{
	FCompoundWidgetOneChildSlot& operator[](const TSharedRef<SWidget>&) { return *this; }
};

class SCompoundWidget : public SWidget
{
protected:
	FCompoundWidgetOneChildSlot ChildSlot;
};

class SLeafWidget : public SWidget {};
class SPanel : public SWidget {};

// Slot arguments shared by the boxes.
template <typename Derived>
struct TBoxSlotArgs
{
	TSharedPtr<SWidget> Widget;
	Derived& Me() { return static_cast<Derived&>(*this); }
	Derived& HAlign(EHorizontalAlignment) { return Me(); }
	Derived& VAlign(EVerticalAlignment) { return Me(); }
	Derived& Padding(const TAttribute<FMargin>&) { return Me(); }
	Derived& Padding(float) { return Me(); }
	Derived& Padding(float, float) { return Me(); }
	Derived& Padding(float, float, float, float) { return Me(); }
	Derived& operator[](const TSharedRef<SWidget>& Child) { Widget = Child; return Me(); }
};

class SVerticalBox : public SPanel
{
public:
	struct FSlotArguments : TBoxSlotArgs<FSlotArguments>
	{
		FSlotArguments& AutoHeight() { return *this; }
		FSlotArguments& FillHeight(const TAttribute<float>&) { return *this; }
		FSlotArguments& MaxHeight(const TAttribute<float>&) { return *this; }
	};
	static FSlotArguments Slot() { return FSlotArguments(); }
	SLATE_BEGIN_ARGS(SVerticalBox) {}
		SLATE_SLOT_ARGUMENT(FSlotArguments, Slots)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

class SHorizontalBox : public SPanel
{
public:
	struct FSlotArguments : TBoxSlotArgs<FSlotArguments>
	{
		FSlotArguments& AutoWidth() { return *this; }
		FSlotArguments& FillWidth(const TAttribute<float>&) { return *this; }
		FSlotArguments& MaxWidth(const TAttribute<float>&) { return *this; }
	};
	static FSlotArguments Slot() { return FSlotArguments(); }
	SLATE_BEGIN_ARGS(SHorizontalBox) {}
		SLATE_SLOT_ARGUMENT(FSlotArguments, Slots)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

class SWrapBox : public SPanel
{
public:
	struct FSlotArguments : TBoxSlotArgs<FSlotArguments>
	{
		FSlotArguments& FillEmptySpace(const TAttribute<bool>&) { return *this; }
		FSlotArguments& ForceNewLine(const TAttribute<bool>&) { return *this; }
	};
	static FSlotArguments Slot() { return FSlotArguments(); }
	SLATE_BEGIN_ARGS(SWrapBox) {}
		SLATE_SLOT_ARGUMENT(FSlotArguments, Slots)
		SLATE_ATTRIBUTE(float, PreferredSize)
		SLATE_ARGUMENT(FVector2D, InnerSlotPadding)
		SLATE_ARGUMENT(bool, UseAllottedSize)
		SLATE_ARGUMENT(EOrientation, Orientation)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

class SSplitter : public SPanel
{
public:
	struct FSlotArguments
	{
		TSharedPtr<SWidget> Widget;
		FSlotArguments& Value(const TAttribute<float>&) { return *this; }
		FSlotArguments& MinSize(float) { return *this; }
		FSlotArguments& operator[](const TSharedRef<SWidget>& Child) { Widget = Child; return *this; }
	};
	static FSlotArguments Slot() { return FSlotArguments(); }
	SLATE_BEGIN_ARGS(SSplitter) {}
		SLATE_SLOT_ARGUMENT(FSlotArguments, Slots)
		SLATE_ARGUMENT(EOrientation, Orientation)
		SLATE_ARGUMENT(float, PhysicalSplitterHandleSize)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

class SBorder : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBorder) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ATTRIBUTE(const FSlateBrush*, BorderImage)
		SLATE_ATTRIBUTE(FMargin, Padding)
		SLATE_ATTRIBUTE(FSlateColor, BorderBackgroundColor)
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

class SBox : public SPanel
{
public:
	SLATE_BEGIN_ARGS(SBox) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ATTRIBUTE(FMargin, Padding)
		SLATE_ATTRIBUTE(FOptionalSize, WidthOverride)
		SLATE_ATTRIBUTE(FOptionalSize, HeightOverride)
		SLATE_ATTRIBUTE(FOptionalSize, MinDesiredWidth)
		SLATE_ATTRIBUTE(FOptionalSize, MinDesiredHeight)
		SLATE_ATTRIBUTE(FOptionalSize, MaxDesiredWidth)
		SLATE_ATTRIBUTE(FOptionalSize, MaxDesiredHeight)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

class STextBlock : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STextBlock) {}
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
		SLATE_ATTRIBUTE(bool, AutoWrapText)
		SLATE_ATTRIBUTE(float, WrapTextAt)
		SLATE_ATTRIBUTE(FText, HighlightText)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
	void SetText(const TAttribute<FText>&) {}
};

class SImage : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SImage) {}
		SLATE_ATTRIBUTE(const FSlateBrush*, Image)
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
		SLATE_ATTRIBUTE(FOptionalSize, DesiredSizeOverride)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

class SSeparator : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SSeparator) {}
		SLATE_STYLE_ARGUMENT(FSlateBrush, SeparatorImage)
		SLATE_ARGUMENT(EOrientation, Orientation)
		SLATE_ARGUMENT(float, Thickness)
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

DECLARE_DELEGATE_RetVal(FReply, FOnClicked)
class SButton : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SButton) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ATTRIBUTE(FMargin, ContentPadding)
		SLATE_ATTRIBUTE(FSlateColor, ButtonColorAndOpacity)
		SLATE_ATTRIBUTE(FSlateColor, ForegroundColor)
		SLATE_EVENT(FOnClicked, OnClicked)
		SLATE_ARGUMENT(bool, IsFocusable)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

DECLARE_DELEGATE_OneParam(FOnCheckStateChanged, ECheckBoxState)
class SCheckBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCheckBox) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_STYLE_ARGUMENT(FCheckBoxStyle, Style)
		SLATE_ATTRIBUTE(ECheckBoxState, IsChecked)
		SLATE_EVENT(FOnCheckStateChanged, OnCheckStateChanged)
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ATTRIBUTE(FMargin, Padding)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
	bool IsChecked() const { return false; }
};

DECLARE_DELEGATE_RetVal(TSharedRef<SWidget>, FOnGetContent)
class SComboButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SComboButton) {}
		SLATE_NAMED_SLOT(FArguments, ButtonContent)
		SLATE_NAMED_SLOT(FArguments, MenuContent)
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)
		SLATE_EVENT(FOnGetContent, OnGetMenuContent)
		SLATE_ATTRIBUTE(FMargin, ContentPadding)
		SLATE_ARGUMENT(bool, HasDownArrow)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

DECLARE_DELEGATE_OneParam(FOnTextChanged, const FText&)
namespace ETextCommit { enum Type { Default, OnEnter, OnUserMovedFocus, OnCleared }; }
DECLARE_DELEGATE_TwoParams(FOnTextCommitted, const FText&, ETextCommit::Type)
class SSearchBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSearchBox) {}
		SLATE_ATTRIBUTE(FText, HintText)
		SLATE_ATTRIBUTE(FText, InitialText)
		SLATE_EVENT(FOnTextChanged, OnTextChanged)
		SLATE_EVENT(FOnTextCommitted, OnTextCommitted)
		SLATE_ARGUMENT(float, DelayChangeNotificationsWhileTyping)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

class SExpandableArea : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExpandableArea) {}
		SLATE_NAMED_SLOT(FArguments, HeaderContent)
		SLATE_NAMED_SLOT(FArguments, BodyContent)
		SLATE_ATTRIBUTE(FText, AreaTitle)
		SLATE_ARGUMENT(bool, InitiallyCollapsed)
		SLATE_ATTRIBUTE(FMargin, Padding)
		SLATE_ATTRIBUTE(FSlateFontInfo, AreaTitleFont)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

// ----- Lists -----

class ITableRow
{
public:
	virtual ~ITableRow() = default;
};

class STableViewBase : public SCompoundWidget
{
public:
	void RequestListRefresh() {}
	virtual int32 GetNumItemsSelected() const { return 0; }
};

DECLARE_DELEGATE_RetVal(TSharedPtr<SWidget>, FOnContextMenuOpening)

class SHeaderRow : public SBorder
{
public:
	using FOnSortModeChanged = TDelegate<void(const EColumnSortPriority::Type, const FName&, const EColumnSortMode::Type)>;
	struct FColumn
	{
		SLATE_BEGIN_ARGS(FColumn) {}
			SLATE_ATTRIBUTE(FText, DefaultLabel)
			SLATE_ATTRIBUTE(FText, DefaultTooltip)
			SLATE_ARGUMENT(TOptional<float>, FixedWidth)
			SLATE_ATTRIBUTE(float, FillWidth)
			SLATE_ARGUMENT(TOptional<float>, ManualWidth)
			SLATE_ATTRIBUTE(EColumnSortMode::Type, SortMode)
			SLATE_EVENT(FOnSortModeChanged, OnSort)
			SLATE_ARGUMENT(EHorizontalAlignment, HAlignHeader)
			SLATE_ARGUMENT(EHorizontalAlignment, HAlignCell)
		SLATE_END_ARGS()
	};
	static FColumn::FArguments Column(const FName& InColumnId) { (void)InColumnId; return FColumn::FArguments(); }
	SLATE_BEGIN_ARGS(SHeaderRow) {}
		SLATE_SLOT_ARGUMENT(FColumn::FArguments, Columns)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

template <typename ItemType>
class STableRow : public ITableRow, public SBorder
{
public:
	SLATE_BEGIN_ARGS(STableRow) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_STYLE_ARGUMENT(FTableRowStyle, Style)
		SLATE_ATTRIBUTE(FMargin, Padding)
		SLATE_ARGUMENT(bool, ShowSelection)
	SLATE_END_ARGS()
	void Construct(const FArguments&, const TSharedRef<STableViewBase>&) {}
};

template <typename ItemType>
class SMultiColumnTableRow : public STableRow<ItemType>
{
public:
	typedef SMultiColumnTableRow<ItemType> FSuperRowType;
	typedef typename STableRow<ItemType>::FArguments FTableRowArgs;
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) = 0;

protected:
	void Construct(const FTableRowArgs&, const TSharedRef<STableViewBase>&) {}
};

template <typename ItemType>
class SListView : public STableViewBase
{
public:
	using FOnGenerateRow = TDelegate<TSharedRef<ITableRow>(ItemType, const TSharedRef<STableViewBase>&)>;
	using FOnSelectionChanged = TDelegate<void(ItemType, ESelectInfo::Type)>;
	using FOnMouseButtonDoubleClick = TDelegate<void(ItemType)>;
	using FOnMouseButtonClick = TDelegate<void(ItemType)>;
	SLATE_BEGIN_ARGS(SListView) {}
		SLATE_EVENT(FOnGenerateRow, OnGenerateRow)
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
		SLATE_EVENT(FOnMouseButtonDoubleClick, OnMouseButtonDoubleClick)
		SLATE_EVENT(FOnMouseButtonClick, OnMouseButtonClick)
		SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)
		SLATE_ARGUMENT(const TArray<ItemType>*, ListItemsSource)
		SLATE_ATTRIBUTE(ESelectionMode::Type, SelectionMode)
		SLATE_ARGUMENT(TSharedPtr<SHeaderRow>, HeaderRow)
		SLATE_ATTRIBUTE(float, ItemHeight)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
	virtual int32 GetNumItemsSelected() const override { return 0; }
	TArray<ItemType> GetSelectedItems() const { return TArray<ItemType>(); }
	void ClearSelection() {}
	void SetSelection(ItemType, ESelectInfo::Type = ESelectInfo::Direct) {}
	void RequestScrollIntoView(ItemType) {}
	bool IsItemSelected(const ItemType&) const { return false; }
};

// ----- Tabs -----

class SDockTab : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SDockTab) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ARGUMENT(ETabRole, TabRole)
		SLATE_ATTRIBUTE(FText, Label)
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
};

struct FSpawnTabArgs {};
DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SDockTab>, FOnSpawnTab, const FSpawnTabArgs&)
DECLARE_DELEGATE_RetVal_OneParam(bool, FCanSpawnTab, const FSpawnTabArgs&)

struct FTabId
{
	FTabId(const FName& InTabType) { (void)InTabType; }
};

class FTabSpawnerEntry
{
public:
	FTabSpawnerEntry& SetDisplayName(const FText&) { return *this; }
	FTabSpawnerEntry& SetTooltipText(const FText&) { return *this; }
	FTabSpawnerEntry& SetIcon(const FSlateIcon&) { return *this; }
	FTabSpawnerEntry& SetMenuType(const TAttribute<ETabSpawnerMenuType::Type>&) { return *this; }
};

class FGlobalTabmanager
{
public:
	static const TSharedRef<FGlobalTabmanager>& Get() { static TSharedRef<FGlobalTabmanager> Manager(std::make_shared<FGlobalTabmanager>()); return Manager; }
	FTabSpawnerEntry& RegisterNomadTabSpawner(const FName TabId, const FOnSpawnTab& OnSpawnTab, const FCanSpawnTab& CanSpawnTab = FCanSpawnTab()) { (void)TabId; (void)OnSpawnTab; (void)CanSpawnTab; static FTabSpawnerEntry Entry; return Entry; }
	void UnregisterNomadTabSpawner(const FName TabId) { (void)TabId; }
	TSharedPtr<SDockTab> TryInvokeTab(const FTabId& TabId, bool bInvokeAsInactive = false) { (void)TabId; (void)bInvokeAsInactive; return nullptr; }
};

class FSlateApplication
{
public:
	static bool IsInitialized() { return true; }
	static FSlateApplication& Get() { static FSlateApplication App; return App; }
};

// ----- Menus -----

DECLARE_DELEGATE(FExecuteAction)
DECLARE_DELEGATE_RetVal(bool, FCanExecuteAction)
DECLARE_DELEGATE_RetVal(bool, FIsActionChecked)
DECLARE_DELEGATE_RetVal(ECheckBoxState, FGetActionCheckState)
enum class EUIActionRepeatMode { RepeatDisabled, RepeatEnabled };

struct FUIAction
{
	FUIAction() = default;
	FUIAction(FExecuteAction ExecuteAction, EUIActionRepeatMode RepeatMode = EUIActionRepeatMode::RepeatDisabled) { (void)ExecuteAction; (void)RepeatMode; }
	FUIAction(FExecuteAction ExecuteAction, FCanExecuteAction CanExecuteAction, EUIActionRepeatMode RepeatMode = EUIActionRepeatMode::RepeatDisabled) { (void)ExecuteAction; (void)CanExecuteAction; (void)RepeatMode; }
	FUIAction(FExecuteAction ExecuteAction, FCanExecuteAction CanExecuteAction, FIsActionChecked IsCheckedDelegate, EUIActionRepeatMode RepeatMode = EUIActionRepeatMode::RepeatDisabled) { (void)ExecuteAction; (void)CanExecuteAction; (void)IsCheckedDelegate; (void)RepeatMode; }
	FUIAction(FExecuteAction ExecuteAction, FCanExecuteAction CanExecuteAction, FGetActionCheckState GetActionCheckStateDelegate, EUIActionRepeatMode RepeatMode = EUIActionRepeatMode::RepeatDisabled) { (void)ExecuteAction; (void)CanExecuteAction; (void)GetActionCheckStateDelegate; (void)RepeatMode; }
};

class FUICommandList;
class FMenuBuilder
{
public:
	FMenuBuilder(const bool bInShouldCloseWindowAfterMenuSelection, TSharedPtr<const FUICommandList> InCommandList) { (void)bInShouldCloseWindowAfterMenuSelection; (void)InCommandList; }
	void BeginSection(FName InExtensionHook, const TAttribute<FText>& InHeadingText = TAttribute<FText>()) { (void)InExtensionHook; (void)InHeadingText; }
	void EndSection() {}
	void AddSeparator(FName InExtensionHook = NAME_None) { (void)InExtensionHook; }
	void AddMenuEntry(const TAttribute<FText>& Label, const TAttribute<FText>& ToolTip, const FSlateIcon& Icon, const FUIAction& UIAction, FName InExtensionHook = NAME_None,
		const EUserInterfaceActionType UserInterfaceActionType = EUserInterfaceActionType::Button, FName InTutorialHighlightName = NAME_None)
	{
		(void)Label; (void)ToolTip; (void)Icon; (void)UIAction; (void)InExtensionHook; (void)UserInterfaceActionType; (void)InTutorialHighlightName;
	}
	TSharedRef<SWidget> MakeWidget() { return SNullWidget::NullWidget; }
};

struct FToolUIActionChoice
{
	FToolUIActionChoice() = default;
	FToolUIActionChoice(const FUIAction& InAction) { (void)InAction; }
};

struct FToolMenuEntry
{
	static FToolMenuEntry InitMenuEntry(const FName InName, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const TAttribute<FSlateIcon>& InIcon, const FToolUIActionChoice& InAction,
		const EUserInterfaceActionType UserInterfaceActionType = EUserInterfaceActionType::Button, const FName InTutorialHighlightName = NAME_None)
	{
		(void)InName; (void)InLabel; (void)InToolTip; (void)InIcon; (void)InAction; (void)UserInterfaceActionType; (void)InTutorialHighlightName;
		return FToolMenuEntry();
	}
	static FToolMenuEntry InitToolBarButton(const FName InName, const FToolUIActionChoice& InAction, const TAttribute<FText>& InLabel = TAttribute<FText>(), const TAttribute<FText>& InToolTip = TAttribute<FText>(),
		const TAttribute<FSlateIcon>& InIcon = TAttribute<FSlateIcon>(), const EUserInterfaceActionType UserInterfaceActionType = EUserInterfaceActionType::Button, FName InTutorialHighlightName = NAME_None)
	{
		(void)InName; (void)InAction; (void)InLabel; (void)InToolTip; (void)InIcon; (void)UserInterfaceActionType; (void)InTutorialHighlightName;
		return FToolMenuEntry();
	}
};

struct FToolMenuSection
{
	FToolMenuEntry& AddEntry(const FToolMenuEntry& Args) { static FToolMenuEntry Entry; (void)Args; return Entry; }
};

class UToolMenu : public UObject
{
public:
	FToolMenuSection& AddSection(const FName SectionName, const TAttribute<FText>& InLabel = TAttribute<FText>()) { (void)SectionName; (void)InLabel; static FToolMenuSection Section; return Section; }
	FToolMenuSection& FindOrAddSection(const FName SectionName) { (void)SectionName; static FToolMenuSection Section; return Section; }
};

struct FToolMenuOwner
{
	FToolMenuOwner() = default;
	FToolMenuOwner(const void* InPointer) { (void)InPointer; }
	FToolMenuOwner(const FName InName) { (void)InName; }
};

struct FToolMenuOwnerScoped
{
	explicit FToolMenuOwnerScoped(const FToolMenuOwner& InOwner) { (void)InOwner; }
};

class UToolMenus : public UObject
{
public:
	static UToolMenus* Get() { static UToolMenus Menus; return &Menus; }
	UToolMenu* ExtendMenu(const FName Name) { (void)Name; return nullptr; }
	static FDelegateHandle RegisterStartupCallback(const FSimpleMulticastDelegate::FDelegate& InDelegate) { (void)InDelegate; return FDelegateHandle(); }
	static void UnRegisterStartupCallback(const void* UserPointer) { (void)UserPointer; }
	static void UnregisterOwner(FToolMenuOwner InOwner) { (void)InOwner; }
};

// ----- Notifications -----

class SNotificationItem : public SCompoundWidget
{
public:
	enum ECompletionState { CS_None, CS_Pending, CS_Success, CS_Fail };
	virtual void SetCompletionState(ECompletionState InState) { (void)InState; }
	virtual void ExpireAndFadeout() {}
	virtual void SetText(const TAttribute<FText>& InText) { (void)InText; }
};

struct FNotificationInfo
{
	explicit FNotificationInfo(const FText& InText) { (void)InText; }
	float ExpireDuration = 5.0f;
	float FadeOutDuration = 2.0f;
	bool bUseSuccessFailIcons = true;
	bool bFireAndForget = true;
	bool bUseLargeFont = true;
	TAttribute<FText> SubText;
};

class FSlateNotificationManager
{
public:
	static FSlateNotificationManager& Get() { static FSlateNotificationManager Manager; return Manager; }
	TSharedPtr<SNotificationItem> AddNotification(const FNotificationInfo& Info) { (void)Info; return nullptr; }
};

// ----- Details views -----

struct FPropertyChangedEvent
{
	FProperty* Property = nullptr;
	FProperty* MemberProperty = nullptr;
	FName GetPropertyName() const { return FName(); }
	FName GetMemberPropertyName() const { return FName(); }
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFinishedChangingProperties, const FPropertyChangedEvent&)

class IDetailsView : public SCompoundWidget
{
public:
	virtual void SetObject(UObject* InObject, bool bForceRefresh = false) { (void)InObject; (void)bForceRefresh; }
	virtual FOnFinishedChangingProperties& OnFinishedChangingProperties() const { static FOnFinishedChangingProperties Event; return Event; }
	virtual void ForceRefresh() {}
};

struct FDetailsViewArgs
{
	enum ENameAreaSettings { HideNameArea, ObjectsUseNameArea, ActorsUseNameArea, ComponentsAndActorsUseNameArea };
	bool bAllowSearch = true;
	bool bHideSelectionTip = false;
	bool bShowOptions = true;
	bool bShowPropertyMatrixButton = true;
	bool bUpdatesFromSelection = false;
	bool bLockable = false;
	ENameAreaSettings NameAreaSettings = ActorsUseNameArea;
};

class FPropertyEditorModule : public IModuleInterface
{
public:
	TSharedRef<IDetailsView> CreateDetailView(const FDetailsViewArgs& DetailsViewArgs) { (void)DetailsViewArgs; return TSharedRef<IDetailsView>(std::make_shared<IDetailsView>()); }
};
