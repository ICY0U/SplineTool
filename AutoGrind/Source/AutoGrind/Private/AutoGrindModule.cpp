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

// Adds Tools > AutoGrind to the level editor, opening the AutoGrind panel.
class FAutoGrindModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AutoGrind::PanelTab, FOnSpawnTab::CreateStatic(&FAutoGrindModule::SpawnPanel))
			.SetDisplayName(LOCTEXT("PanelTitle", "AutoGrind"))
			.SetTooltipText(LOCTEXT("PanelTip", "Find grindable edges and rails on the selected meshes."))
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
	}

private:
	static TSharedRef<SDockTab> SpawnPanel(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SAutoGrindPanel)];
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = Menu->AddSection("AutoGrind", LOCTEXT("Section", "Rollout Modding"));
		Section.AddEntry(FToolMenuEntry::InitMenuEntry(
			"OpenAutoGrind",
			LOCTEXT("Open", "AutoGrind"),
			LOCTEXT("OpenTip", "Find grindable edges and rails on the selected meshes."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(AutoGrind::PanelTab); }))));
	}
};

IMPLEMENT_MODULE(FAutoGrindModule, AutoGrind)

#undef LOCTEXT_NAMESPACE
