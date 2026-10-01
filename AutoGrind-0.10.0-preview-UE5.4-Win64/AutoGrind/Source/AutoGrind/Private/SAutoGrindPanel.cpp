#include "SAutoGrindPanel.h"

#include "AutoGrindGenerate.h"
#include "AutoGrindPreview.h"
#include "AutoGrindSettings.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Misc/ScopedSlowTask.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
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
		const FName SourceColumn(TEXT("Source"));

		FText Metres(double Centimetres)
		{
			return FText::Format(LOCTEXT("Metres", "{0} m"), FText::AsNumber(Centimetres / 100, &FNumberFormattingOptions().SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2)));
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
				FSuperRowType::Construct(FSuperRowType::FArguments(), Owner);
			}

			virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
			{
				if (Column == KeepColumn)
				{
					return SNew(SCheckBox)
						.ToolTipText(LOCTEXT("KeepTip", "Unticked lines are not placed."))
						.IsChecked_Lambda([this] { return Line->bKeep ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
						{
							Line->bKeep = State == ECheckBoxState::Checked;
							OnChanged.ExecuteIfBound();
						});
				}
				if (Column == KindColumn)
				{
					return SNew(SButton)
						.ToolTipText(LOCTEXT("KindTip", "Rail places GrindType 0; stone places the GrindActor's default ledge grind. Click to switch."))
						.Text_Lambda([this] { return Line->bRail ? LOCTEXT("Rail", "Rail") : LOCTEXT("Stone", "Stone"); })
						.OnClicked_Lambda([this]
						{
							Line->bRail = !Line->bRail;
							OnChanged.ExecuteIfBound();
							return FReply::Handled();
						});
				}
				if (Column == LengthColumn)
				{
					return SNew(STextBlock).Text(Metres(Line->Length));
				}
				if (Column == DropColumn)
				{
					return SNew(STextBlock).Text(Metres(Line->Drop)).ToolTipText(LOCTEXT("DropTip", "How far the surface falls just past the edge."));
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
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	const TSharedRef<IDetailsView> Details = PropertyEditor.CreateDetailView(DetailsArgs);
	Details->SetObject(GetMutableDefault<UAutoGrindSettings>());
	Details->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent&)
	{
		GetMutableDefault<UAutoGrindSettings>()->SaveConfig();
		bScanStale = true;
		Status = LOCTEXT("SettingsChanged", "Settings changed: scan again to apply them.");
	});

	Summary = LOCTEXT("Intro", "Select the meshes that should carry grind lines, then scan.");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Scan", "Scan Selected"))
				.ToolTipText(LOCTEXT("ScanTip", "Find grind lines on the static meshes of the selected actors."))
				.OnClicked(this, &SAutoGrindPanel::OnScan)
			]
			+ SWrapBox::Slot().Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Clear", "Clear"))
				.ToolTipText(LOCTEXT("ClearTip", "Forget the lines and remove the preview."))
				.OnClicked(this, &SAutoGrindPanel::OnClear)
			]
			+ SWrapBox::Slot().Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Generate", "Generate"))
				.ToolTipText(LOCTEXT("GenerateTip", "Place a GrindActor along each ticked line. Lines generated earlier from the same actors are replaced. Ctrl+Z undoes it."))
				.IsEnabled_Lambda([this] { return !bScanStale && PreviewWorld.Get() == EditorWorld() && Lines.ContainsByPredicate([](const FLinePtr& Line) { return Line->bKeep; }); })
				.OnClicked(this, &SAutoGrindPanel::OnGenerate)
			]
			+ SWrapBox::Slot().Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("RemoveGenerated", "Remove Generated"))
				.ToolTipText(LOCTEXT("RemoveGeneratedTip", "Remove every GrindActor AutoGrind placed in this level. Hand-placed ones are left alone. Ctrl+Z undoes it."))
				.OnClicked(this, &SAutoGrindPanel::OnRemoveGenerated)
			]
			+ SWrapBox::Slot().Padding(12, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("TickAll", "Keep Visible"))
				.OnClicked(this, &SAutoGrindPanel::OnTickAll, true)
			]
			+ SWrapBox::Slot()
			[
				SNew(SButton)
				.Text(LOCTEXT("TickNone", "Exclude Visible"))
				.OnClicked(this, &SAutoGrindPanel::OnTickAll, false)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 4, 0)
			[
				SNew(SSearchBox).HintText(LOCTEXT("SearchHint", "Filter by actor, rail, stone or closed"))
				.OnTextChanged_Lambda([this](const FText& Text) { Search = Text.ToString(); RefreshFilter(); UpdateStatus(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton).Text(LOCTEXT("VisibleRail", "Visible to Rail")).OnClicked(this, &SAutoGrindPanel::SetVisibleKind, true)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("VisibleStone", "Visible to Stone")).OnClicked(this, &SAutoGrindPanel::SetVisibleKind, false)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4, 0, 4, 2)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Summary; })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4, 0, 4, 4)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Status; })
		]
		+ SVerticalBox::Slot().FillHeight(1)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot().Value(0.6f)
			[
				SAssignNew(List, SListView<FLinePtr>)
				.ListItemsSource(&VisibleLines)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SAutoGrindPanel::MakeRow)
				.OnSelectionChanged(this, &SAutoGrindPanel::OnLineSelected)
				.OnMouseButtonDoubleClick(this, &SAutoGrindPanel::OnLineDoubleClicked)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(AutoGrind::KeepColumn).DefaultLabel(FText::GetEmpty()).FixedWidth(28)
					+ SHeaderRow::Column(AutoGrind::KindColumn).DefaultLabel(LOCTEXT("KindHeader", "Kind")).FixedWidth(64)
					+ SHeaderRow::Column(AutoGrind::LengthColumn).DefaultLabel(LOCTEXT("LengthHeader", "Length")).FixedWidth(72)
					+ SHeaderRow::Column(AutoGrind::DropColumn).DefaultLabel(LOCTEXT("DropHeader", "Drop")).FixedWidth(72)
					+ SHeaderRow::Column(AutoGrind::SourceColumn).DefaultLabel(LOCTEXT("SourceHeader", "Actor"))
				)
			]
			+ SSplitter::Slot().Value(0.4f)
			[
				SNew(SExpandableArea)
				.AreaTitle(LOCTEXT("SettingsTitle", "Settings"))
				.InitiallyCollapsed(false)
				.BodyContent()
				[
					Details
				]
			]
		]
	];
}

SAutoGrindPanel::~SAutoGrindPanel()
{
	if (const UWorld* World = PreviewWorld.Get())
	{
		AutoGrind::ClearPreview(*World);
	}
}

UWorld* SAutoGrindPanel::EditorWorld() const
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

FReply SAutoGrindPanel::OnScan()
{
	UWorld* World = EditorWorld();
	if (!World)
	{
		return FReply::Handled();
	}
	TArray<AActor*> Actors;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			Actors.Add(Actor);
		}
	}
	if (Actors.IsEmpty())
	{
		Summary = LOCTEXT("NothingSelected", "Nothing is selected. Select the meshes that should carry grind lines, then scan.");
		return FReply::Handled();
	}

	const UAutoGrindSettings& Settings = *GetDefault<UAutoGrindSettings>();
	FScopedSlowTask Progress(1, LOCTEXT("ScanningProgress", "AutoGrind: checking selected mesh edges..."));
	Progress.MakeDialog();
	Progress.EnterProgressFrame();
	const FAutoGrindScanResult Result = AutoGrind::Scan(*World, Actors, Settings.ToCore(), Settings.bShowNearMisses, Settings.bCheckWalls);
	if (const UWorld* OldWorld = PreviewWorld.Get()) AutoGrind::ClearPreview(*OldWorld);
	ScannedTransforms.Reset();
	for (AActor* Actor : Actors) ScannedTransforms.Add(Actor, Actor->GetActorTransform());
	bScanStale = false;
	Lines.Reset();
	int32 Rails = 0;
	double Length = 0;
	for (const FAutoGrindLine& Line : Result.Lines)
	{
		Lines.Add(MakeShared<FAutoGrindLine>(Line));
		Rails += Line.bRail ? 1 : 0;
		Length += Line.Length;
	}
	NearMisses = Result.NearMisses;
	FText Skipped = FText::GetEmpty();
	if (!Result.Skipped.IsEmpty())
	{
		Skipped = FText::Format(LOCTEXT("Skipped", " Skipped {0} unsupported or unreadable mesh(es), such as {1}."), Result.Skipped.Num(), FText::FromString(Result.Skipped[0]));
	}
	// With near misses shown, say why edges were turned down, most common reason first.
	FString Reasons;
	if (!NearMisses.IsEmpty())
	{
		TMap<FString, int32> ByReason;
		for (const FAutoGrindNearMiss& Miss : NearMisses)
		{
			++ByReason.FindOrAdd(Miss.Reason);
		}
		ByReason.ValueSort([](int32 L, int32 R) { return L > R; });
		for (const TPair<FString, int32>& Reason : ByReason)
		{
			Reasons += FString::Printf(TEXT("\n  %d: %s"), Reason.Value, *Reason.Key);
		}
		Reasons = TEXT("\nGrey edges were turned down because:") + Reasons;
	}
	Summary = FText::Format(LOCTEXT("Summary", "Scanned {0} mesh(es) in {1} s: {2} line(s), {3} of them rails, {4} in all.{5}{6}"),
		Result.MeshCount, FText::AsNumber(Result.Seconds, &FNumberFormattingOptions().SetMaximumFractionalDigits(2)), Lines.Num(), Rails, AutoGrind::Metres(Length), Skipped, FText::FromString(Reasons));
	PreviewWorld = World;
	RefreshFilter();
	UpdateStatus();
	Redraw();
	return FReply::Handled();
}

FReply SAutoGrindPanel::OnClear()
{
	Lines.Reset();
	VisibleLines.Reset();
	ScannedTransforms.Reset();
	bScanStale = false;
	NearMisses.Reset();
	List->RequestListRefresh();
	if (const UWorld* World = PreviewWorld.Get())
	{
		AutoGrind::ClearPreview(*World);
	}
	Summary = LOCTEXT("Cleared", "Cleared. Select the meshes that should carry grind lines, then scan.");
	Status = FText::GetEmpty();
	return FReply::Handled();
}

FReply SAutoGrindPanel::OnGenerate()
{
	UWorld* World = EditorWorld();
	if (!World)
	{
		return FReply::Handled();
	}
	if (PreviewWorld.Get() != World || bScanStale)
	{
		Status = LOCTEXT("RescanRequired", "The world or settings changed. Scan again before generating.");
		return FReply::Handled();
	}
	for (const auto& Entry : ScannedTransforms)
	{
		if (!Entry.Key.IsValid() || Entry.Key->GetWorld() != World || !Entry.Key->GetActorTransform().Equals(Entry.Value))
		{
			bScanStale = true;
			Status = LOCTEXT("SourceMoved", "A scanned actor moved or was deleted. Scan again before generating.");
			return FReply::Handled();
		}
	}
	TArray<const FAutoGrindLine*> Ticked;
	for (const FLinePtr& Line : Lines)
	{
		if (Line->bKeep)
		{
			Ticked.Add(Line.Get());
		}
	}
	const AutoGrind::FGenerateResult Result = AutoGrind::Generate(*World, Ticked);
	Status = !Result.Error.IsEmpty()
		? FText::Format(LOCTEXT("GenerateFailed", "Placed {0} of {1}. {2}"), Result.Placed, Ticked.Num(), FText::FromString(Result.Error))
		: FText::Format(LOCTEXT("Generated", "Placed {0} GrindActor(s) in the AutoGrind folder{1}. Ctrl+Z undoes it."), Result.Placed,
			Result.Replaced > 0 ? FText::Format(LOCTEXT("Replaced", ", replacing {0} from an earlier Generate"), Result.Replaced) : FText::GetEmpty());
	return FReply::Handled();
}

FReply SAutoGrindPanel::OnRemoveGenerated()
{
	if (UWorld* World = EditorWorld())
	{
		const int32 Removed = AutoGrind::RemoveGenerated(*World);
		Status = FText::Format(LOCTEXT("RemovedGenerated", "Removed {0} generated GrindActor(s). Ctrl+Z undoes it."), Removed);
	}
	return FReply::Handled();
}

FReply SAutoGrindPanel::OnTickAll(bool bKeep)
{
	for (const FLinePtr& Line : VisibleLines)
	{
		Line->bKeep = bKeep;
	}
	UpdateStatus();
	Redraw();
	return FReply::Handled();
}

TSharedRef<ITableRow> SAutoGrindPanel::MakeRow(FLinePtr Line, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(AutoGrind::SLineRow, Owner)
		.Line(Line)
		.OnChanged_Lambda([this]
		{
			RefreshFilter();
			UpdateStatus();
			Redraw();
		});
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

void SAutoGrindPanel::Redraw()
{
	const UWorld* World = PreviewWorld.Get();
	if (!World)
	{
		return;
	}
	const TArray<FLinePtr> Selected = List->GetSelectedItems();
	const FAutoGrindLine* Highlighted = Selected.Num() > 0 ? Selected[0].Get() : nullptr;
	AutoGrind::DrawPreview(*World, Lines, GetDefault<UAutoGrindSettings>()->bShowNearMisses ? NearMisses : TArray<FAutoGrindNearMiss>(), Highlighted);
}

void SAutoGrindPanel::UpdateStatus()
{
	int32 Ticked = 0;
	for (const FLinePtr& Line : Lines)
	{
		Ticked += Line->bKeep ? 1 : 0;
	}
	Status = bScanStale ? LOCTEXT("StaleStatus", "Scan again to apply changes before generating.") : Lines.IsEmpty() ? FText::GetEmpty() : FText::Format(LOCTEXT("Ticked", "{0} of {1} kept; {2} visible. Generate includes ALL kept lines, including hidden results. Double-click to frame."), Ticked, Lines.Num(), VisibleLines.Num());
}

void SAutoGrindPanel::RefreshFilter()
{
	VisibleLines.Reset();
	for (const FLinePtr& Line : Lines)
	{
		const FString Description = Line->SourceLabel + (Line->bRail ? TEXT(" rail") : TEXT(" stone")) + (Line->bClosed ? TEXT(" closed") : TEXT(" open"));
		if (Search.IsEmpty() || Description.Contains(Search)) VisibleLines.Add(Line);
	}
	List->RequestListRefresh();
}

FReply SAutoGrindPanel::SetVisibleKind(bool bRail)
{
	for (const FLinePtr& Line : VisibleLines) Line->bRail = bRail;
	RefreshFilter();
	UpdateStatus();
	Redraw();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
