#pragma once

#include "CoreMinimal.h"
#include "AutoGrindScan.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

// The AutoGrind tab: scans the selected actors, previews the lines found in the level viewport, and
// places a GrindActor along each ticked line. Each line can be unticked or switched between rail and
// stone first; each Generate and Remove is one undo step.
class SAutoGrindPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAutoGrindPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAutoGrindPanel() override;

private:
	using FLinePtr = TSharedPtr<FAutoGrindLine>;

	FReply OnScan();
	FReply OnClear();
	FReply OnGenerate();
	FReply OnRemoveGenerated();
	FReply OnTickAll(bool bKeep);
	TSharedRef<ITableRow> MakeRow(FLinePtr Line, const TSharedRef<STableViewBase>& Owner);
	void OnLineSelected(FLinePtr Line, ESelectInfo::Type How);
	void OnLineDoubleClicked(FLinePtr Line);
	void Redraw();
	void UpdateStatus();
	void RefreshFilter();
	FReply SetVisibleKind(bool bRail);
	UWorld* EditorWorld() const;

	TArray<FLinePtr> Lines;
	TArray<FLinePtr> VisibleLines;
	FString Search;
	bool bScanStale = false;
	TMap<TWeakObjectPtr<AActor>, FTransform> ScannedTransforms;
	TArray<FAutoGrindNearMiss> NearMisses;
	TSharedPtr<SListView<FLinePtr>> List;
	TWeakObjectPtr<UWorld> PreviewWorld;
	FText Summary;
	FText Status;
};
