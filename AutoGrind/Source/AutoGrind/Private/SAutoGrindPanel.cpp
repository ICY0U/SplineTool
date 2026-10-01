#include "SAutoGrindPanel.h"

#include "AutoGrindDrawMode.h"
#include "AutoGrindGenerate.h"
#include "AutoGrindPreview.h"
#include "AutoGrindSettings.h"
#include "AutoGrindStyle.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformProcess.h"
#include "IDetailsView.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "AutoGrind"

namespace AutoGrind
{
	namespace
	{
		const FName KeepColumn(TEXT("Keep"));
		const FName KindColumn(TEXT("Kind"));
		const FName LengthColumn(TEXT("Length"));
		const FName DropColumn(TEXT("Drop"));
		const FName ConfidenceColumn(TEXT("Confidence"));
		const FName SourceColumn(TEXT("Source"));
		const FName NotesColumn(TEXT("Notes"));

		FText Metres(double Centimetres)
		{
			return FText::Format(LOCTEXT("Metres", "{0} m"), FText::AsNumber(Centimetres / 100, &FNumberFormattingOptions().SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2)));
		}

		FText Percent(float Share)
		{
			return FText::Format(LOCTEXT("Percent", "{0}%"), FText::AsNumber(FMath::RoundToInt(Share * 100)));
		}

		FSlateColor ConfidenceColour(float Confidence)
		{
			return Confidence >= 0.75f ? FSlateColor(FLinearColor(0.35f, 0.85f, 0.4f)) : Confidence >= 0.5f ? FSlateColor(FLinearColor(0.95f, 0.8f, 0.3f)) : FSlateColor(FLinearColor(0.95f, 0.5f, 0.25f));
		}

		// A word or two per note, for the list.
		FString ShortNotes(const FAutoGrindLine& Line)
		{
			TArray<FString> Words;
			auto Add = [&Words, &Line](uint32 Note, const TCHAR* Word)
			{
				if (Line.Notes & Note)
				{
					Words.Add(Word);
				}
			};
			Add(AutoGrindCore::Notes::Joined, *FString::Printf(TEXT("joined x%d"), FMath::Max(2, Line.Sources.Num())));
			Add(AutoGrindCore::Notes::LowLedge, TEXT("low"));
			Add(AutoGrindCore::Notes::HighDrop, TEXT("high"));
			Add(AutoGrindCore::Notes::SoftEdge, TEXT("rounded"));
			Add(AutoGrindCore::Notes::RoundTop, TEXT("pipe"));
			Add(AutoGrindCore::Notes::Coping, TEXT("coping"));
			Add(AutoGrindCore::Notes::PartialRail, TEXT("partial"));
			Add(AutoGrindCore::Notes::SteepLip, TEXT("bank lip"));
			Add(AutoGrindCore::Notes::Ridge, TEXT("ridge"));
			Add(AutoGrindCore::Notes::Short, TEXT("short"));
			if (Line.bClosed)
			{
				Words.Add(TEXT("closed"));
			}
			return FString::Join(Words, TEXT(", "));
		}

		TSharedRef<SWidget> SectionTitle(const FText& Title)
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 2)
				[
					SNew(STextBlock).Text(Title).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
				[
					SNew(SSeparator).Thickness(1)
				];
		}

		// A button with an icon and a label.
		TSharedRef<SWidget> IconLabel(const FName& Icon, const TAttribute<FText>& Label)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
				[
					SNew(SImage).Image(FAppStyle::Get().GetBrush(Icon)).ColorAndOpacity(FSlateColor::UseForeground())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Label)
				];
		}

		// A toggle button, one of a group: checked while IsOn says so, OnPick when clicked.
		TSharedRef<SWidget> Choice(const FText& Label, const FText& ToolTip, TFunction<bool()> IsOn, TFunction<void()> OnPick)
		{
			return SNew(SCheckBox)
				.Style(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckbox"))
				.ToolTipText(ToolTip)
				.IsChecked_Lambda([IsOn] { return IsOn() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([OnPick](ECheckBoxState) { OnPick(); })
				[
					SNew(SBox).Padding(FMargin(8, 2))
					[
						SNew(STextBlock).Text(Label)
					]
				];
		}

		class SLineRow : public SMultiColumnTableRow<TSharedPtr<FAutoGrindLine>>
		{
		public:
			SLATE_BEGIN_ARGS(SLineRow) {}
				SLATE_ARGUMENT(TSharedPtr<FAutoGrindLine>, Line)
				SLATE_EVENT(FSimpleDelegate, OnChanged)
			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& Owner)
			{
				Line = InArgs._Line;
				OnChanged = InArgs._OnChanged;
				FSuperRowType::Construct(FSuperRowType::FArguments().Padding(FMargin(0, 1)), Owner);
				if (Line)
				{
					SetToolTipText(Line->Describe());
				}
			}

			virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
			{
				if (!Line)
				{
					return SNullWidget::NullWidget;
				}
				if (Column == KeepColumn)
				{
					return SNew(SBox).HAlign(HAlign_Center)
						[
							SNew(SCheckBox)
							.ToolTipText(LOCTEXT("KeepTip", "Ticked lines are placed by Generate. Space toggles the selected lines."))
							.IsChecked_Lambda([this] { return Line->bKeep ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
							.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
							{
								Line->bKeep = State == ECheckBoxState::Checked;
								OnChanged.ExecuteIfBound();
							})
						];
				}
				if (Column == KindColumn)
				{
					return SNew(SBox).Padding(FMargin(2, 0))
						[
							SNew(SButton)
							.ToolTipText(LOCTEXT("KindTip", "Rail places a rail grind; stone places a ledge grind. Click to switch."))
							.HAlign(HAlign_Center)
							.ButtonColorAndOpacity_Lambda([this] { return FSlateColor(Line->bRail ? RailColour() : StoneColour()); })
							.OnClicked_Lambda([this]
							{
								Line->bRail = !Line->bRail;
								OnChanged.ExecuteIfBound();
								return FReply::Handled();
							})
							[
								SNew(STextBlock)
								.Text_Lambda([this] { return Line->bRail ? LOCTEXT("Rail", "Rail") : LOCTEXT("Stone", "Stone"); })
								.ColorAndOpacity(FLinearColor::White)
							]
						];
				}
				if (Column == LengthColumn)
				{
					return SNew(STextBlock).Text(Metres(Line->Length));
				}
				if (Column == DropColumn)
				{
					return SNew(STextBlock).Text(Metres(Line->Drop));
				}
				if (Column == ConfidenceColumn)
				{
					return SNew(STextBlock).Text(Percent(Line->Confidence)).ColorAndOpacity(ConfidenceColour(Line->Confidence));
				}
				if (Column == NotesColumn)
				{
					return SNew(STextBlock).Text(FText::FromString(ShortNotes(*Line))).ColorAndOpacity(FSlateColor::UseSubduedForeground());
				}
				return SNew(STextBlock).Text(FText::FromString(Line->SourceLabel));
			}

		private:
			TSharedPtr<FAutoGrindLine> Line;
			FSimpleDelegate OnChanged;
		};
	}
}

void SAutoGrindPanel::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	const TSharedRef<IDetailsView> Details = PropertyEditor.CreateDetailView(DetailsArgs);
	Details->SetObject(GetMutableDefault<UAutoGrindSettings>());
	Details->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent& Event)
	{
		GetMutableDefault<UAutoGrindSettings>()->SaveConfig();
		// Preview and output settings take effect at once; the rest need a new scan. An array element's own
		// property has no category: its member property, the array, does.
		const FProperty* Changed = Event.MemberProperty ? Event.MemberProperty : Event.Property;
		const FName Category = Changed ? FName(*Changed->GetMetaData(TEXT("Category"))) : NAME_None;
		if (Category == TEXT("Review"))
		{
			Redraw();
			return;
		}
		if (Category != TEXT("Output") && !Lines.IsEmpty())
		{
			bSettingsChanged = true;
			SetStatus(LOCTEXT("SettingsChanged", "Settings changed: scan again to apply them."));
		}
	});

	FString Version;
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AutoGrind")))
	{
		Version = Plugin->GetDescriptor().VersionName;
	}
	Summary = LOCTEXT("Intro", "Select the meshes that should carry grind lines (or choose Whole Level) and scan. Or draw lines by hand.");
	DrawnHandle = FAutoGrindDrawShared::Get().OnDrawn.AddSP(this, &SAutoGrindPanel::OnDrawn);

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Vertical)
		+ SSplitter::Slot().Value(0.68f)
		[
			SNew(SVerticalBox)
			// Title bar.
			+ SVerticalBox::Slot().AutoHeight().Padding(6, 6, 6, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
				[
					SNew(SBox).WidthOverride(24).HeightOverride(24)
					[
						SNew(SImage).Image(FAutoGrindStyle::Icon().GetIcon())
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Title", "AutoGrind")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(6, 0, 0, 2)
				[
					SNew(STextBlock).Text(FText::FromString(Version)).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot().FillWidth(1)
				[
					SNullWidget::NullWidget
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
					.ToolTipText(LOCTEXT("HelpTip", "Open the AutoGrind manual."))
					.OnClicked_Lambda([]
					{
						FPlatformProcess::LaunchURL(TEXT("https://github.com/ICY0U/SplineTool/blob/main/AutoGrind/README.md"), nullptr, nullptr);
						return FReply::Handled();
					})
					[
						SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.Help")).ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(6, 0)
			[
				MakeScanSection()
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(6, 0)
			[
				MakeDrawSection()
			]
			+ SVerticalBox::Slot().FillHeight(1).Padding(6, 0)
			[
				MakeResultsSection()
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(6, 0, 6, 6)
			[
				MakeGenerateSection()
			]
		]
		+ SSplitter::Slot().Value(0.32f)
		[
			SNew(SExpandableArea)
			.AreaTitle(LOCTEXT("SettingsTitle", "Settings"))
			.InitiallyCollapsed(false)
			.BodyContent()
			[
				Details
			]
		]
	];
}

SAutoGrindPanel::~SAutoGrindPanel()
{
	FAutoGrindDrawShared::Get().OnDrawn.Remove(DrawnHandle);
	FAutoGrindDrawShared::Get().SetSnapLines({});
	if (const UWorld* World = PreviewWorld.Get())
	{
		AutoGrind::ClearPreview(*World);
	}
}

TSharedRef<SWidget> SAutoGrindPanel::MakeScanSection()
{
	UAutoGrindSettings* Settings = GetMutableDefault<UAutoGrindSettings>();
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			AutoGrind::SectionTitle(LOCTEXT("ScanTitle", "Scan"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4, 4))
			+ SWrapBox::Slot()
			[
				AutoGrind::Choice(LOCTEXT("ScopeSelected", "Selected Actors"), LOCTEXT("ScopeSelectedTip", "Scan the static meshes of the actors selected in the level."),
					[Settings] { return Settings->Scope == EAutoGrindScanScope::Selected; },
					[Settings] { Settings->Scope = EAutoGrindScanScope::Selected; Settings->SaveConfig(); })
			]
			+ SWrapBox::Slot()
			[
				AutoGrind::Choice(LOCTEXT("ScopeLevel", "Whole Level"), LOCTEXT("ScopeLevelTip", "Scan every static mesh in the level, leaving out what the Filtering settings exclude: foliage, scenery, meshes without collision, hidden actors and tiny props."),
					[Settings] { return Settings->Scope == EAutoGrindScanScope::Level; },
					[Settings] { Settings->Scope = EAutoGrindScanScope::Level; Settings->SaveConfig(); })
			]
			+ SWrapBox::Slot()
			[
				SNew(SComboButton)
				.ToolTipText(LOCTEXT("PresetTip", "Set the detection settings for a kind of map. Your output and filter settings are kept."))
				.OnGetMenuContent(this, &SAutoGrindPanel::MakePresetMenu)
				.ButtonContent()
				[
					AutoGrind::IconLabel("Icons.Settings", LOCTEXT("Preset", "Preset"))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
				.ToolTipText(LOCTEXT("ScanTip", "Find grind lines on the selected actors, or on the whole level."))
				.OnClicked(this, &SAutoGrindPanel::OnScan)
				[
					AutoGrind::IconLabel("Icons.Search", LOCTEXT("Scan", "Scan"))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("ClearTip", "Forget the lines and remove the preview."))
				.IsEnabled_Lambda([this] { return !Lines.IsEmpty() || !NearMisses.IsEmpty(); })
				.OnClicked(this, &SAutoGrindPanel::OnClear)
				[
					AutoGrind::IconLabel("Icons.X", LOCTEXT("Clear", "Clear"))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text_Lambda([Settings]
				{
					if (Settings->Scope == EAutoGrindScanScope::Level)
					{
						return LOCTEXT("LevelScope", "Every mesh in the level, filtered.");
					}
					const int32 Count = GEditor ? GEditor->GetSelectedActorCount() : 0;
					return Count > 0 ? FText::Format(LOCTEXT("SelectedCount", "{0} actor(s) selected."), Count) : LOCTEXT("NoneSelected", "Nothing selected yet.");
				})
			]
		];
}

TSharedRef<SWidget> SAutoGrindPanel::MakeDrawSection()
{
	UAutoGrindSettings* Settings = GetMutableDefault<UAutoGrindSettings>();
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			AutoGrind::SectionTitle(LOCTEXT("DrawTitle", "Draw by Hand"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4, 4))
			+ SWrapBox::Slot()
			[
				SNew(SCheckBox)
				.Style(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckbox"))
				.ToolTipText(LOCTEXT("DrawToggleTip", "Click points in the level viewport and a grind line is placed along them. Clicks snap to scanned lines and sharp edges, and the line follows them between clicks."))
				.IsChecked_Lambda([] { return FAutoGrindDrawMode::IsActive() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([](ECheckBoxState State) { FAutoGrindDrawMode::SetActive(State == ECheckBoxState::Checked); })
				[
					SNew(SBox).Padding(FMargin(8, 2))
					[
						AutoGrind::IconLabel("Icons.Edit", TAttribute<FText>::CreateLambda([] { return FAutoGrindDrawMode::IsActive() ? LOCTEXT("Drawing", "Drawing...") : LOCTEXT("DrawLines", "Draw Lines"); }))
					]
				]
			]
			+ SWrapBox::Slot()
			[
				AutoGrind::Choice(LOCTEXT("DrawAuto", "Auto"), LOCTEXT("DrawAutoTip", "Rail or stone as the scanned line under your clicks; stone elsewhere."),
					[Settings] { return Settings->DrawKind == EAutoGrindDrawKind::Auto; }, [Settings] { Settings->DrawKind = EAutoGrindDrawKind::Auto; Settings->SaveConfig(); })
			]
			+ SWrapBox::Slot()
			[
				AutoGrind::Choice(LOCTEXT("DrawRail", "Rail"), LOCTEXT("DrawRailTip", "Lines drawn now are rails."),
					[Settings] { return Settings->DrawKind == EAutoGrindDrawKind::Rail; }, [Settings] { Settings->DrawKind = EAutoGrindDrawKind::Rail; Settings->SaveConfig(); })
			]
			+ SWrapBox::Slot()
			[
				AutoGrind::Choice(LOCTEXT("DrawStone", "Stone"), LOCTEXT("DrawStoneTip", "Lines drawn now are stone (ledge) grinds."),
					[Settings] { return Settings->DrawKind == EAutoGrindDrawKind::Stone; }, [Settings] { Settings->DrawKind = EAutoGrindDrawKind::Stone; Settings->SaveConfig(); })
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("FollowTip", "Between two clicks on the same edge or line, follow it round curves and corners. Off: straight between clicks. F in the viewport."))
				.IsChecked_Lambda([Settings] { return Settings->bDrawFollowEdges ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Settings](ECheckBoxState State) { Settings->bDrawFollowEdges = State == ECheckBoxState::Checked; Settings->SaveConfig(); })
				[
					SNew(STextBlock).Text(LOCTEXT("Follow", "Follow edges"))
				]
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("SnapTip", "Snap clicks to scanned lines and sharp edges. Hold Shift to place one point without snapping. S in the viewport."))
				.IsChecked_Lambda([Settings] { return Settings->bDrawSnap ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Settings](ECheckBoxState State) { Settings->bDrawSnap = State == ECheckBoxState::Checked; Settings->SaveConfig(); })
				[
					SNew(STextBlock).Text(LOCTEXT("Snap", "Snap"))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Visibility_Lambda([] { return FAutoGrindDrawMode::IsActive() ? EVisibility::Visible : EVisibility::Collapsed; })
			.Text(LOCTEXT("DrawHint", "In the viewport: click points along the edge, Enter or double-click to place. Ctrl+Click takes a whole edge or scanned line at once. Backspace removes a point; Esc drops the points, then leaves."))
		];
}

TSharedRef<SWidget> SAutoGrindPanel::MakeResultsSection()
{
	auto FilterChoice = [this](EFilter Which, const FText& Label, const FText& ToolTip)
	{
		return AutoGrind::Choice(Label, ToolTip, [this, Which] { return Filter == Which; }, [this, Which] { Filter = Which; RefreshList(); });
	};
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			AutoGrind::SectionTitle(LOCTEXT("LinesTitle", "Lines"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Summary; })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Visibility_Lambda([this] { return Reasons.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text_Lambda([this] { return Reasons; })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 4)
		[
			SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4, 4))
			+ SWrapBox::Slot().FillEmptySpace(true)
			[
				SNew(SBox).MinDesiredWidth(180)
				[
					SNew(SSearchBox)
					.HintText(LOCTEXT("SearchHint", "Search actor, rail, stone, note..."))
					.OnTextChanged_Lambda([this](const FText& Text) { Search = Text.ToString(); RefreshList(); })
				]
			]
			+ SWrapBox::Slot()
			[
				FilterChoice(EFilter::All, LOCTEXT("FilterAll", "All"), LOCTEXT("FilterAllTip", "Every line."))
			]
			+ SWrapBox::Slot()
			[
				FilterChoice(EFilter::Kept, LOCTEXT("FilterKept", "Kept"), LOCTEXT("FilterKeptTip", "Ticked lines: what Generate will place."))
			]
			+ SWrapBox::Slot()
			[
				FilterChoice(EFilter::Review, LOCTEXT("FilterReview", "Review"), LOCTEXT("FilterReviewTip", "Unticked lines: low ledges, ridges and others the scan was less sure of."))
			]
			+ SWrapBox::Slot()
			[
				FilterChoice(EFilter::Rails, LOCTEXT("FilterRails", "Rails"), LOCTEXT("FilterRailsTip", "Rail lines only."))
			]
			+ SWrapBox::Slot()
			[
				FilterChoice(EFilter::Stone, LOCTEXT("FilterStone", "Stone"), LOCTEXT("FilterStoneTip", "Stone lines only."))
			]
		]
		+ SVerticalBox::Slot().FillHeight(1)
		[
			SAssignNew(List, SListView<FLinePtr>)
			.ListItemsSource(&VisibleLines)
			.SelectionMode(ESelectionMode::Multi)
			.OnGenerateRow(this, &SAutoGrindPanel::MakeRow)
			.OnSelectionChanged(this, &SAutoGrindPanel::OnLineSelected)
			.OnMouseButtonDoubleClick(this, &SAutoGrindPanel::OnLineDoubleClicked)
			.OnContextMenuOpening(this, &SAutoGrindPanel::MakeContextMenu)
			.HeaderRow
			(
				SAssignNew(Header, SHeaderRow)
				+ SHeaderRow::Column(AutoGrind::KeepColumn).DefaultLabel(FText::GetEmpty()).FixedWidth(28)
				+ SHeaderRow::Column(AutoGrind::KindColumn).DefaultLabel(LOCTEXT("KindHeader", "Kind")).FixedWidth(64)
					.SortMode_Lambda([this] { return SortModeFor(AutoGrind::KindColumn); }).OnSort(this, &SAutoGrindPanel::OnSortColumn)
				+ SHeaderRow::Column(AutoGrind::LengthColumn).DefaultLabel(LOCTEXT("LengthHeader", "Length")).FixedWidth(70)
					.SortMode_Lambda([this] { return SortModeFor(AutoGrind::LengthColumn); }).OnSort(this, &SAutoGrindPanel::OnSortColumn)
				+ SHeaderRow::Column(AutoGrind::DropColumn).DefaultLabel(LOCTEXT("DropHeader", "Drop")).FixedWidth(64)
					.DefaultTooltip(LOCTEXT("DropTip", "How far the surface falls just past the edge."))
					.SortMode_Lambda([this] { return SortModeFor(AutoGrind::DropColumn); }).OnSort(this, &SAutoGrindPanel::OnSortColumn)
				+ SHeaderRow::Column(AutoGrind::ConfidenceColumn).DefaultLabel(LOCTEXT("ConfidenceHeader", "Sure")).FixedWidth(48)
					.DefaultTooltip(LOCTEXT("ConfidenceTip", "How sure the scan is that this line belongs. Lines below Keep Confidence are listed unticked."))
					.SortMode_Lambda([this] { return SortModeFor(AutoGrind::ConfidenceColumn); }).OnSort(this, &SAutoGrindPanel::OnSortColumn)
				+ SHeaderRow::Column(AutoGrind::SourceColumn).DefaultLabel(LOCTEXT("SourceHeader", "Actor")).FillWidth(0.55f)
					.SortMode_Lambda([this] { return SortModeFor(AutoGrind::SourceColumn); }).OnSort(this, &SAutoGrindPanel::OnSortColumn)
				+ SHeaderRow::Column(AutoGrind::NotesColumn).DefaultLabel(LOCTEXT("NotesHeader", "Notes")).FillWidth(0.45f)
			)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4, 4))
			+ SWrapBox::Slot().VAlign(VAlign_Center)
			[
				SNew(STextBlock).ColorAndOpacity(FSlateColor::UseSubduedForeground()).Text_Lambda([this]
				{
					const int32 Selected = List.IsValid() ? List->GetNumItemsSelected() : 0;
					return Selected > 0 ? FText::Format(LOCTEXT("ActOnSelected", "{0} selected:"), Selected) : FText::Format(LOCTEXT("ActOnVisible", "{0} shown:"), VisibleLines.Num());
				})
			]
			+ SWrapBox::Slot()
			[
				SNew(SButton).Text(LOCTEXT("Keep", "Keep")).ToolTipText(LOCTEXT("KeepBulkTip", "Tick them: Generate places them."))
				.IsEnabled_Lambda([this] { return !VisibleLines.IsEmpty(); })
				.OnClicked_Lambda([this] { SetKeep(true); return FReply::Handled(); })
			]
			+ SWrapBox::Slot()
			[
				SNew(SButton).Text(LOCTEXT("Exclude", "Exclude")).ToolTipText(LOCTEXT("ExcludeBulkTip", "Untick them: Generate leaves them out. Delete key too."))
				.IsEnabled_Lambda([this] { return !VisibleLines.IsEmpty(); })
				.OnClicked_Lambda([this] { SetKeep(false); return FReply::Handled(); })
			]
			+ SWrapBox::Slot()
			[
				SNew(SButton).Text(LOCTEXT("Suggested", "As Suggested")).ToolTipText(LOCTEXT("SuggestedTip", "Tick every line the scan was sure of and untick the rest, as after the scan."))
				.IsEnabled_Lambda([this] { return !Lines.IsEmpty(); })
				.OnClicked_Lambda([this] { KeepSuggested(); return FReply::Handled(); })
			]
			+ SWrapBox::Slot()
			[
				SNew(SButton).Text(LOCTEXT("ToRail", "Rail")).ToolTipText(LOCTEXT("ToRailTip", "Make them rails."))
				.IsEnabled_Lambda([this] { return !VisibleLines.IsEmpty(); })
				.OnClicked_Lambda([this] { SetKind(true); return FReply::Handled(); })
			]
			+ SWrapBox::Slot()
			[
				SNew(SButton).Text(LOCTEXT("ToStone", "Stone")).ToolTipText(LOCTEXT("ToStoneTip", "Make them stone."))
				.IsEnabled_Lambda([this] { return !VisibleLines.IsEmpty(); })
				.OnClicked_Lambda([this] { SetKind(false); return FReply::Handled(); })
			]
			+ SWrapBox::Slot()
			[
				SNew(SButton).ToolTipText(LOCTEXT("FrameTip", "Move the viewport camera to them. F key or double-click too."))
				.IsEnabled_Lambda([this] { return !VisibleLines.IsEmpty(); })
				.OnClicked_Lambda([this] { FrameLines(); return FReply::Handled(); })
				[
					AutoGrind::IconLabel("Icons.Visible", LOCTEXT("Frame", "Frame"))
				]
			]
		];
}

TSharedRef<SWidget> SAutoGrindPanel::MakeGenerateSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			AutoGrind::SectionTitle(LOCTEXT("GenerateTitle", "Place"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4, 4))
			+ SWrapBox::Slot()
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
				.ToolTipText(LOCTEXT("GenerateTip", "Place a grind actor along each ticked line, hidden ones included. Lines generated earlier from the same actors are replaced; lines drawn by hand are kept. Ctrl+Z undoes it."))
				.IsEnabled_Lambda([this] { return CountKept() > 0; })
				.OnClicked(this, &SAutoGrindPanel::OnGenerate)
				[
					AutoGrind::IconLabel("Icons.Plus", TAttribute<FText>::CreateLambda([this] { return FText::Format(LOCTEXT("GenerateCount", "Generate {0} Line(s)"), CountKept()); }))
				]
			]
			+ SWrapBox::Slot()
			[
				SNew(SComboButton)
				.ToolTipText(LOCTEXT("RemoveTip", "Remove grind actors AutoGrind placed. Hand-placed ones are never touched. Ctrl+Z undoes it."))
				.OnGetMenuContent(this, &SAutoGrindPanel::MakeRemoveMenu)
				.ButtonContent()
				[
					AutoGrind::IconLabel("Icons.Delete", LOCTEXT("Remove", "Remove"))
				]
			]
			+ SWrapBox::Slot()
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("SelectGeneratedTip", "Select every grind actor AutoGrind placed in the level."))
				.OnClicked(this, &SAutoGrindPanel::OnSelectGenerated)
				[
					SNew(STextBlock).Text(LOCTEXT("SelectGenerated", "Select Placed"))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Status; })
		];
}

TSharedRef<SWidget> SAutoGrindPanel::MakePresetMenu()
{
	FMenuBuilder Menu(true, nullptr);
	const UEnum* Presets = StaticEnum<EAutoGrindPreset>();
	for (int32 I = 0; Presets && I < Presets->NumEnums() - 1; ++I)
	{
		const int64 Value = Presets->GetValueByIndex(I);
		Menu.AddMenuEntry(Presets->GetDisplayNameTextByIndex(I), Presets->GetToolTipTextByIndex(I), FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SAutoGrindPanel::ApplyPreset, Value)));
	}
	return Menu.MakeWidget();
}

TSharedRef<SWidget> SAutoGrindPanel::MakeRemoveMenu()
{
	FMenuBuilder Menu(true, nullptr);
	Menu.AddMenuEntry(LOCTEXT("RemoveScanned", "Remove Scanned Lines"), LOCTEXT("RemoveScannedTip", "Remove the grind actors Generate placed. Lines drawn by hand stay."), FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SAutoGrindPanel::RemoveGenerated, true, false)));
	Menu.AddMenuEntry(LOCTEXT("RemoveDrawn", "Remove Drawn Lines"), LOCTEXT("RemoveDrawnTip", "Remove the grind actors drawn by hand."), FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SAutoGrindPanel::RemoveGenerated, false, true)));
	Menu.AddMenuEntry(LOCTEXT("RemoveAll", "Remove All AutoGrind Lines"), LOCTEXT("RemoveAllTip", "Remove every grind actor AutoGrind placed, scanned or drawn."), FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SAutoGrindPanel::RemoveGenerated, true, true)));
	return Menu.MakeWidget();
}

TSharedPtr<SWidget> SAutoGrindPanel::MakeContextMenu()
{
	if (!List.IsValid() || List->GetNumItemsSelected() == 0)
	{
		return nullptr;
	}
	FMenuBuilder Menu(true, nullptr);
	Menu.BeginSection("Lines", LOCTEXT("SelectedLines", "Selected Lines"));
	Menu.AddMenuEntry(LOCTEXT("MenuKeep", "Keep"), LOCTEXT("MenuKeepTip", "Tick them."), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SAutoGrindPanel::SetKeep, true)));
	Menu.AddMenuEntry(LOCTEXT("MenuExclude", "Exclude"), LOCTEXT("MenuExcludeTip", "Untick them."), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SAutoGrindPanel::SetKeep, false)));
	Menu.AddMenuEntry(LOCTEXT("MenuRail", "Make Rail"), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SAutoGrindPanel::SetKind, true)));
	Menu.AddMenuEntry(LOCTEXT("MenuStone", "Make Stone"), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SAutoGrindPanel::SetKind, false)));
	Menu.AddMenuEntry(LOCTEXT("MenuFrame", "Frame in Viewport"), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SAutoGrindPanel::FrameLines)));
	Menu.AddMenuEntry(LOCTEXT("MenuSelect", "Select Their Actors"), LOCTEXT("MenuSelectTip", "Select the actors the lines run along."), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SAutoGrindPanel::SelectSources)));
	Menu.AddMenuEntry(LOCTEXT("MenuForget", "Remove from List"), LOCTEXT("MenuForgetTip", "Drop them from this scan's list."), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SAutoGrindPanel::RemoveFromList)));
	Menu.EndSection();
	return Menu.MakeWidget();
}

TSharedRef<ITableRow> SAutoGrindPanel::MakeRow(FLinePtr Line, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(AutoGrind::SLineRow, Owner)
		.Line(Line)
		.OnChanged_Lambda([this]
		{
			RefreshList();
			Redraw();
		});
}

UWorld* SAutoGrindPanel::EditorWorld() const
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

int32 SAutoGrindPanel::CountKept() const
{
	int32 Kept = 0;
	for (const FLinePtr& Line : Lines)
	{
		Kept += Line->bKeep ? 1 : 0;
	}
	return Kept;
}

void SAutoGrindPanel::SetStatus(const FText& Text)
{
	Status = Text;
}

void SAutoGrindPanel::Notify(const FText& Text, bool bSuccess) const
{
	FNotificationInfo Info(Text);
	Info.ExpireDuration = 4.0f;
	Info.bUseSuccessFailIcons = true;
	if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
	{
		Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
}

FReply SAutoGrindPanel::OnScan()
{
	UWorld* World = EditorWorld();
	if (!World)
	{
		return FReply::Handled();
	}
	const UAutoGrindSettings& Settings = *GetDefault<UAutoGrindSettings>();
	FAutoGrindScanRequest Request;
	int32 FilteredOut = 0;
	if (Settings.Scope == EAutoGrindScanScope::Level)
	{
		Request.Actors = AutoGrind::CollectLevelActors(*World, Settings, &FilteredOut);
		Request.ComponentFilter = [&Settings](const UStaticMeshComponent& Component) { return AutoGrind::PassesLevelFilters(Component, Settings); };
	}
	else
	{
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			AActor* Actor = Cast<AActor>(*It);
			if (Actor && AutoGrind::IsScannable(*Actor, Settings))
			{
				Request.Actors.Add(Actor);
			}
		}
	}
	if (Request.Actors.IsEmpty())
	{
		Summary = Settings.Scope == EAutoGrindScanScope::Level
			? LOCTEXT("NothingInLevel", "No static mesh in the level passed the Filtering settings.")
			: LOCTEXT("NothingSelected", "Nothing is selected. Select the meshes that should carry grind lines, or choose Whole Level, then scan.");
		return FReply::Handled();
	}
	Request.Core = Settings.ToCore();
	Request.bCollectNearMisses = Settings.bShowNearMisses;
	Request.bCheckWalls = Settings.bCheckWalls;
	const FAutoGrindScanResult Result = AutoGrind::Scan(*World, Request);

	if (const UWorld* OldWorld = PreviewWorld.Get())
	{
		AutoGrind::ClearPreview(*OldWorld);
	}
	ScannedTransforms.Reset();
	for (AActor* Actor : Request.Actors)
	{
		ScannedTransforms.Add(Actor, Actor->GetActorTransform());
	}
	bSettingsChanged = false;
	Lines.Reset();
	for (const FAutoGrindLine& Line : Result.Lines)
	{
		Lines.Add(MakeShared<FAutoGrindLine>(Line));
	}
	NearMisses = Result.NearMisses;
	PreviewWorld = World;
	UpdateSummary(Result, FilteredOut);
	SetStatus(Result.bCancelled ? LOCTEXT("Cancelled", "Scan cancelled: the lines found before then are listed.") : FText::GetEmpty());
	FAutoGrindDrawShared::Get().SetSnapLines(Lines);
	if (List.IsValid())
	{
		List->ClearSelection();
	}
	RefreshList();
	Redraw();
	return FReply::Handled();
}

void SAutoGrindPanel::UpdateSummary(const FAutoGrindScanResult& Result, int32 FilteredOut)
{
	int32 Rails = 0;
	int32 Kept = 0;
	double KeptLength = 0;
	for (const FLinePtr& Line : Lines)
	{
		Rails += Line->bRail ? 1 : 0;
		Kept += Line->bKeep ? 1 : 0;
		KeptLength += Line->bKeep ? Line->Length : 0;
	}
	FFormatNamedArguments Args;
	Args.Add(TEXT("Actors"), Result.ActorCount);
	Args.Add(TEXT("Meshes"), Result.MeshCount);
	Args.Add(TEXT("Triangles"), FText::AsNumber(Result.TriangleCount));
	Args.Add(TEXT("Seconds"), FText::AsNumber(Result.Seconds, &FNumberFormattingOptions().SetMaximumFractionalDigits(2)));
	Args.Add(TEXT("Lines"), Lines.Num());
	Args.Add(TEXT("Rails"), Rails);
	Args.Add(TEXT("Stone"), Lines.Num() - Rails);
	Args.Add(TEXT("Kept"), Kept);
	Args.Add(TEXT("Review"), Lines.Num() - Kept);
	Args.Add(TEXT("Length"), AutoGrind::Metres(KeptLength));
	FText Text = FText::Format(LOCTEXT("Summary", "Scanned {Actors} actor(s), {Meshes} mesh(es), {Triangles} triangles in {Seconds} s: {Lines} line(s), {Rails} rail and {Stone} stone. {Kept} kept ({Length}), {Review} for review."), Args);
	if (FilteredOut > 0)
	{
		Text = FText::Format(LOCTEXT("SummaryFiltered", "{0} {1} actor(s) left out by the Filtering settings."), Text, FilteredOut);
	}
	if (!Result.Skipped.IsEmpty())
	{
		Text = FText::Format(LOCTEXT("SummarySkipped", "{0} Skipped {1} mesh(es) with no source geometry, such as {2}."), Text, Result.Skipped.Num(), FText::FromString(Result.Skipped[0]));
	}
	Summary = Text;
	// With near misses shown, say why edges were turned down, most common reason first, and which setting decides.
	FString Why;
	if (!NearMisses.IsEmpty())
	{
		TMap<int32, int32> ByReason;
		for (const FAutoGrindNearMiss& Miss : NearMisses)
		{
			++ByReason.FindOrAdd(int32(Miss.Reason));
		}
		ByReason.ValueSort([](int32 L, int32 R) { return L > R; });
		for (const TPair<int32, int32>& Reason : ByReason)
		{
			const AutoGrindCore::Reject Code = AutoGrindCore::Reject(Reason.Key);
			Why += FString::Printf(TEXT("\n  %d: %s  (%s)"), Reason.Value, UTF8_TO_TCHAR(AutoGrindCore::Describe(Code)), UTF8_TO_TCHAR(AutoGrindCore::SettingFor(Code)));
		}
		Why = TEXT("Grey edges were turned down because:") + Why;
	}
	Reasons = FText::FromString(Why);
}

FReply SAutoGrindPanel::OnClear()
{
	Lines.Reset();
	VisibleLines.Reset();
	ScannedTransforms.Reset();
	NearMisses.Reset();
	bSettingsChanged = false;
	FAutoGrindDrawShared::Get().SetSnapLines({});
	if (List.IsValid())
	{
		List->RequestListRefresh();
	}
	if (const UWorld* World = PreviewWorld.Get())
	{
		AutoGrind::ClearPreview(*World);
	}
	Summary = LOCTEXT("Cleared", "Cleared. Select the meshes that should carry grind lines, or choose Whole Level, and scan.");
	Reasons = FText::GetEmpty();
	SetStatus(FText::GetEmpty());
	return FReply::Handled();
}

bool SAutoGrindPanel::IsStale(FText* OutWhy)
{
	UWorld* World = EditorWorld();
	if (!World || PreviewWorld.Get() != World)
	{
		if (OutWhy)
		{
			*OutWhy = LOCTEXT("WorldChanged", "The level changed since the scan. Scan again before generating.");
		}
		return true;
	}
	if (bSettingsChanged)
	{
		if (OutWhy)
		{
			*OutWhy = LOCTEXT("RescanRequired", "Settings changed since the scan. Scan again before generating.");
		}
		return true;
	}
	for (const TPair<TWeakObjectPtr<AActor>, FTransform>& Entry : ScannedTransforms)
	{
		if (!Entry.Key.IsValid() || Entry.Key->GetWorld() != World || !Entry.Key->GetActorTransform().Equals(Entry.Value))
		{
			if (OutWhy)
			{
				*OutWhy = LOCTEXT("SourceMoved", "A scanned actor moved or was deleted. Scan again before generating.");
			}
			return true;
		}
	}
	return false;
}

FReply SAutoGrindPanel::OnGenerate()
{
	UWorld* World = EditorWorld();
	if (!World)
	{
		return FReply::Handled();
	}
	FText Why;
	if (IsStale(&Why))
	{
		SetStatus(Why);
		Notify(Why, false);
		return FReply::Handled();
	}
	AutoGrind::FPlaceOptions Options;
	FString Error;
	if (!AutoGrind::MakePlaceOptions(*GetDefault<UAutoGrindSettings>(), Options, Error))
	{
		SetStatus(FText::FromString(Error));
		Notify(FText::FromString(Error), false);
		return FReply::Handled();
	}
	TArray<const FAutoGrindLine*> Ticked;
	for (const FLinePtr& Line : Lines)
	{
		if (Line->bKeep)
		{
			Ticked.Add(Line.Get());
		}
	}
	const AutoGrind::FGenerateResult Result = AutoGrind::Generate(*World, Ticked, Options);
	if (!Result.Error.IsEmpty())
	{
		const FText Message = FText::Format(LOCTEXT("GenerateFailed", "Placed none of {0}. {1}"), Ticked.Num(), FText::FromString(Result.Error));
		SetStatus(Message);
		Notify(Message, false);
		return FReply::Handled();
	}
	const FText Message = FText::Format(LOCTEXT("Generated", "Placed {0} grind actor(s){1}. Ctrl+Z undoes it."), Result.Placed,
		Result.Replaced > 0 ? FText::Format(LOCTEXT("Replaced", ", replacing {0} from an earlier Generate"), Result.Replaced) : FText::GetEmpty());
	SetStatus(Message);
	Notify(Message, true);
	return FReply::Handled();
}

void SAutoGrindPanel::RemoveGenerated(bool bScanned, bool bDrawn)
{
	if (UWorld* World = EditorWorld())
	{
		const int32 Removed = AutoGrind::RemoveGenerated(*World, bScanned, bDrawn);
		const FText Message = FText::Format(LOCTEXT("RemovedGenerated", "Removed {0} grind actor(s) AutoGrind placed. Ctrl+Z undoes it."), Removed);
		SetStatus(Message);
		Notify(Message, true);
	}
}

FReply SAutoGrindPanel::OnSelectGenerated()
{
	UWorld* World = EditorWorld();
	if (!World || !GEditor)
	{
		return FReply::Handled();
	}
	const TArray<AActor*> Placed = AutoGrind::FindGenerated(*World, true, true);
	GEditor->SelectNone(false, true, false);
	for (AActor* Actor : Placed)
	{
		GEditor->SelectActor(Actor, true, false, true);
	}
	GEditor->NoteSelectionChange();
	SetStatus(FText::Format(LOCTEXT("SelectedPlaced", "Selected {0} grind actor(s) AutoGrind placed."), Placed.Num()));
	return FReply::Handled();
}

TArray<SAutoGrindPanel::FLinePtr> SAutoGrindPanel::Targets() const
{
	if (List.IsValid() && List->GetNumItemsSelected() > 0)
	{
		return List->GetSelectedItems();
	}
	return VisibleLines;
}

void SAutoGrindPanel::SetKeep(bool bKeep)
{
	for (const FLinePtr& Line : Targets())
	{
		Line->bKeep = bKeep;
	}
	RefreshList();
	Redraw();
}

void SAutoGrindPanel::ToggleKeep()
{
	const TArray<FLinePtr> Chosen = Targets();
	const bool bAnyUnticked = Chosen.ContainsByPredicate([](const FLinePtr& Line) { return !Line->bKeep; });
	for (const FLinePtr& Line : Chosen)
	{
		Line->bKeep = bAnyUnticked;
	}
	RefreshList();
	Redraw();
}

void SAutoGrindPanel::SetKind(bool bRail)
{
	for (const FLinePtr& Line : Targets())
	{
		Line->bRail = bRail;
	}
	RefreshList();
	Redraw();
}

void SAutoGrindPanel::KeepSuggested()
{
	for (const FLinePtr& Line : Lines)
	{
		Line->bKeep = Line->bSuggested;
	}
	RefreshList();
	Redraw();
}

void SAutoGrindPanel::FrameLines()
{
	const TArray<FLinePtr> Chosen = Targets();
	if (!GEditor || Chosen.IsEmpty())
	{
		return;
	}
	FBox Bounds(ForceInit);
	for (const FLinePtr& Line : Chosen)
	{
		for (const FVector& Point : Line->Points)
		{
			Bounds += Point;
		}
	}
	if (Bounds.IsValid)
	{
		GEditor->MoveViewportCamerasToBox(Bounds.ExpandBy(200), true);
	}
}

void SAutoGrindPanel::SelectSources()
{
	if (!GEditor)
	{
		return;
	}
	GEditor->SelectNone(false, true, false);
	for (const FLinePtr& Line : Targets())
	{
		for (const TWeakObjectPtr<AActor>& Source : Line->Sources)
		{
			if (AActor* Actor = Source.Get())
			{
				GEditor->SelectActor(Actor, true, false, true);
			}
		}
	}
	GEditor->NoteSelectionChange();
}

void SAutoGrindPanel::RemoveFromList()
{
	const TArray<FLinePtr> Chosen = Targets();
	Lines.RemoveAll([&Chosen](const FLinePtr& Line) { return Chosen.Contains(Line); });
	FAutoGrindDrawShared::Get().SetSnapLines(Lines);
	if (List.IsValid())
	{
		List->ClearSelection();
	}
	RefreshList();
	Redraw();
}

void SAutoGrindPanel::ApplyPreset(int64 Preset)
{
	UAutoGrindSettings* Settings = GetMutableDefault<UAutoGrindSettings>();
	Settings->ApplyPreset(EAutoGrindPreset(Preset));
	Settings->SaveConfig();
	if (!Lines.IsEmpty())
	{
		bSettingsChanged = true;
	}
	const UEnum* Presets = StaticEnum<EAutoGrindPreset>();
	SetStatus(FText::Format(LOCTEXT("PresetApplied", "{0} settings applied. Scan again to use them."), Presets ? Presets->GetDisplayNameTextByValue(Preset) : FText::GetEmpty()));
}

void SAutoGrindPanel::OnLineSelected(FLinePtr Line, ESelectInfo::Type How)
{
	Redraw();
}

void SAutoGrindPanel::OnLineDoubleClicked(FLinePtr Line)
{
	if (Line && GEditor)
	{
		FBox Bounds(Line->Points);
		GEditor->MoveViewportCamerasToBox(Bounds.ExpandBy(200), true);
	}
}

EColumnSortMode::Type SAutoGrindPanel::SortModeFor(FName Column) const
{
	return Column == SortColumn ? SortMode : EColumnSortMode::None;
}

void SAutoGrindPanel::OnSortColumn(EColumnSortPriority::Type Priority, const FName& Column, EColumnSortMode::Type Mode)
{
	SortColumn = Column;
	SortMode = Mode;
	RefreshList();
}

void SAutoGrindPanel::RefreshList()
{
	VisibleLines.Reset();
	for (const FLinePtr& Line : Lines)
	{
		const bool bPasses = Filter == EFilter::All || (Filter == EFilter::Kept && Line->bKeep) || (Filter == EFilter::Review && !Line->bKeep)
			|| (Filter == EFilter::Rails && Line->bRail) || (Filter == EFilter::Stone && !Line->bRail);
		if (!bPasses)
		{
			continue;
		}
		if (!Search.IsEmpty())
		{
			const FString Text = Line->SourceLabel + (Line->bRail ? TEXT(" rail") : TEXT(" stone")) + (Line->bKeep ? TEXT(" kept") : TEXT(" review")) + TEXT(" ") + AutoGrind::ShortNotes(*Line);
			if (!Text.Contains(Search))
			{
				continue;
			}
		}
		VisibleLines.Add(Line);
	}
	if (SortMode != EColumnSortMode::None && !SortColumn.IsNone())
	{
		const FName Column = SortColumn;
		auto Less = [Column](const FLinePtr& L, const FLinePtr& R)
		{
			if (Column == AutoGrind::KindColumn)
			{
				return !L->bRail && R->bRail;
			}
			if (Column == AutoGrind::DropColumn)
			{
				return L->Drop < R->Drop;
			}
			if (Column == AutoGrind::ConfidenceColumn)
			{
				return L->Confidence < R->Confidence;
			}
			if (Column == AutoGrind::SourceColumn)
			{
				return L->SourceLabel < R->SourceLabel;
			}
			return L->Length < R->Length;
		};
		const bool bAscending = SortMode == EColumnSortMode::Ascending;
		VisibleLines.StableSort([bAscending, Less](const FLinePtr& L, const FLinePtr& R) { return bAscending ? Less(L, R) : Less(R, L); });
	}
	if (List.IsValid())
	{
		List->RequestListRefresh();
	}
}

void SAutoGrindPanel::Redraw()
{
	const UWorld* World = PreviewWorld.Get();
	if (!World)
	{
		return;
	}
	TSet<const FAutoGrindLine*> Selected;
	if (List.IsValid())
	{
		for (const FLinePtr& Line : List->GetSelectedItems())
		{
			Selected.Add(Line.Get());
		}
	}
	const UAutoGrindSettings& Settings = *GetDefault<UAutoGrindSettings>();
	AutoGrind::FPreviewStyle Style;
	Style.Thickness = Settings.PreviewThickness;
	Style.bDirections = Settings.bShowDirections;
	AutoGrind::DrawPreview(*World, Lines, Settings.bShowNearMisses ? NearMisses : TArray<FAutoGrindNearMiss>(), Selected, Style);
}

void SAutoGrindPanel::OnDrawn(AActor* Placed, const FText& Message)
{
	DrawnCount += Placed ? 1 : 0;
	SetStatus(Placed ? FText::Format(LOCTEXT("DrawnStatus", "{0} ({1} drawn this session.)"), Message, DrawnCount) : Message);
	if (!Placed)
	{
		Notify(Message, false);
	}
}

FReply SAutoGrindPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Shortcuts act on the list only while it has focus: letters typed in the search box are text.
	if (!List.IsValid() || !(List->HasKeyboardFocus() || List->HasFocusedDescendants()))
	{
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::SpaceBar)
	{
		ToggleKeep();
		return FReply::Handled();
	}
	if (Key == EKeys::Delete)
	{
		SetKeep(false);
		return FReply::Handled();
	}
	if (Key == EKeys::F)
	{
		FrameLines();
		return FReply::Handled();
	}
	if (Key == EKeys::R && !InKeyEvent.IsControlDown())
	{
		SetKind(true);
		return FReply::Handled();
	}
	if (Key == EKeys::S && !InKeyEvent.IsControlDown())
	{
		SetKind(false);
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
