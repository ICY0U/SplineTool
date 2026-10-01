#include "AutoGrindDrawMode.h"
#include "AutoGrindStyle.h"
#include "EditorModeRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "SAutoGrindPanel.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "AutoGrind"

namespace AutoGrind
{
	const FName PanelTab(TEXT("AutoGrind"));
}

// Adds Tools > AutoGrind and a toolbar button to the level editor, opening the AutoGrind panel, and the
// AutoGrind Draw viewport mode the panel switches on.
class FAutoGrindModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FAutoGrindStyle::Register();
		FEditorModeRegistry::Get().RegisterMode<FAutoGrindDrawMode>(FAutoGrindDrawMode::ModeId, LOCTEXT("DrawMode", "AutoGrind Draw"), FAutoGrindStyle::Icon(), false);
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AutoGrind::PanelTab, FOnSpawnTab::CreateStatic(&FAutoGrindModule::SpawnPanel))
			.SetDisplayName(LOCTEXT("PanelTitle", "AutoGrind"))
			.SetTooltipText(LOCTEXT("PanelTip", "Find grindable ledges, rails and coping, or draw grind lines by hand, and place Rollout Inline GrindActors along them."))
			.SetIcon(FAutoGrindStyle::Icon())
			.SetMenuType(ETabSpawnerMenuType::Hidden);
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAutoGrindModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AutoGrind::PanelTab);
		}
		FEditorModeRegistry::Get().UnregisterMode(FAutoGrindDrawMode::ModeId);
		FAutoGrindStyle::Unregister();
	}

private:
	static TSharedRef<SDockTab> SpawnPanel(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SAutoGrindPanel)];
	}

	static void OpenPanel()
	{
		FGlobalTabmanager::Get()->TryInvokeTab(AutoGrind::PanelTab);
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
		{
			FToolMenuSection& Section = Menu->AddSection("AutoGrind", LOCTEXT("Section", "Rollout Modding"));
			Section.AddEntry(FToolMenuEntry::InitMenuEntry(
				"OpenAutoGrind",
				LOCTEXT("Open", "AutoGrind"),
				LOCTEXT("OpenTip", "Find grindable ledges, rails and coping, or draw grind lines by hand."),
				FAutoGrindStyle::Icon(),
				FUIAction(FExecuteAction::CreateStatic(&FAutoGrindModule::OpenPanel))));
			Section.AddEntry(FToolMenuEntry::InitMenuEntry(
				"AutoGrindDraw",
				LOCTEXT("Draw", "AutoGrind Draw"),
				LOCTEXT("DrawTip", "Draw grind lines by clicking points in the viewport."),
				FAutoGrindStyle::Icon(),
				FUIAction(FExecuteAction::CreateLambda([] { FAutoGrindDrawMode::SetActive(!FAutoGrindDrawMode::IsActive()); }), FCanExecuteAction(),
					FIsActionChecked::CreateLambda([] { return FAutoGrindDrawMode::IsActive(); })),
				EUserInterfaceActionType::ToggleButton));
		}
		if (UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar"))
		{
			FToolMenuSection& Section = Toolbar->FindOrAddSection("AutoGrind");
			Section.AddEntry(FToolMenuEntry::InitToolBarButton(
				"AutoGrindToolbar",
				FUIAction(FExecuteAction::CreateStatic(&FAutoGrindModule::OpenPanel)),
				LOCTEXT("ToolbarLabel", "AutoGrind"),
				LOCTEXT("ToolbarTip", "Open AutoGrind: scan for grind lines or draw them by hand."),
				FAutoGrindStyle::Icon()));
		}
	}
};

IMPLEMENT_MODULE(FAutoGrindModule, AutoGrind)

#undef LOCTEXT_NAMESPACE
