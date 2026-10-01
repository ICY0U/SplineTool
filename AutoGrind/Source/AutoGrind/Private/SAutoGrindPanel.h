#pragma once

#include "CoreMinimal.h"
#include "AutoGrindScan.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

class AActor;
class UWorld;

// The AutoGrind tab. Scans the selected actors or the whole level, lists the lines found for review and
// draws them in the level viewport, switches on the Draw mode for drawing lines by hand, and places a grind
// actor along each kept line. Each Generate and Remove is one undo step.
class SAutoGrindPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAutoGrindPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAutoGrindPanel() override;

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	using FLinePtr = TSharedPtr<FAutoGrindLine>;

	// Which lines the list shows.
	enum class EFilter : uint8
	{
		All,
		Kept,
		Review,
		Rails,
		Stone
	};

	// ----- Building the panel -----
	TSharedRef<SWidget> MakeScanSection();
	TSharedRef<SWidget> MakeDrawSection();
	TSharedRef<SWidget> MakeResultsSection();
	TSharedRef<SWidget> MakeGenerateSection();
	TSharedRef<SWidget> MakePresetMenu();
	TSharedRef<SWidget> MakeRemoveMenu();
	TSharedPtr<SWidget> MakeContextMenu();
	TSharedRef<ITableRow> MakeRow(FLinePtr Line, const TSharedRef<STableViewBase>& Owner);

	// ----- Actions -----
	FReply OnScan();
	FReply OnClear();
	FReply OnGenerate();
	FReply OnSelectGenerated();
	void RemoveGenerated(bool bScanned, bool bDrawn);
	void SetKeep(bool bKeep);
	void ToggleKeep();
	void SetKind(bool bRail);
	void KeepSuggested();
	void FrameLines();
	void SelectSources();
	void RemoveFromList();
	void ApplyPreset(int64 Preset);

	// ----- List -----
	void OnLineSelected(FLinePtr Line, ESelectInfo::Type How);
	void OnLineDoubleClicked(FLinePtr Line);
	void OnSortColumn(EColumnSortPriority::Type Priority, const FName& Column, EColumnSortMode::Type Mode);
	EColumnSortMode::Type SortModeFor(FName Column) const;
	void RefreshList();
	// The selected lines, or every visible line when none is selected.
	TArray<FLinePtr> Targets() const;

	// ----- State -----
	void Redraw();
	void UpdateSummary(const FAutoGrindScanResult& Result, int32 FilteredOut);
	void SetStatus(const FText& Text);
	void Notify(const FText& Text, bool bSuccess) const;
	bool IsStale(FText* OutWhy = nullptr);
	int32 CountKept() const;
	UWorld* EditorWorld() const;
	void OnDrawn(AActor* Placed, const FText& Message);

	TArray<FLinePtr> Lines;
	TArray<FLinePtr> VisibleLines;
	TArray<FAutoGrindNearMiss> NearMisses;
	TSharedPtr<SListView<FLinePtr>> List;
	TSharedPtr<SHeaderRow> Header;
	TWeakObjectPtr<UWorld> PreviewWorld;
	TMap<TWeakObjectPtr<AActor>, FTransform> ScannedTransforms;
	FString Search;
	EFilter Filter = EFilter::All;
	FName SortColumn;
	EColumnSortMode::Type SortMode = EColumnSortMode::None;
	bool bSettingsChanged = false;
	int32 DrawnCount = 0;
	FText Summary;
	FText Reasons;
	FText Status;
	FDelegateHandle DrawnHandle;
};
