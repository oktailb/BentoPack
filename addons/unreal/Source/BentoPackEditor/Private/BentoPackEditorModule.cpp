#include "BentoPackEditorModule.h"
#include "BentoPackDashboard.h"
#include "BentoAssetTypeActions.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FBentoPackEditorModule"

static const FName BentoDashboardTabName = FName("BentoPackDashboard");

void FBentoPackEditorModule::StartupModule()
{
	// 1. Register Asset Type Actions
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TSharedRef<IAssetTypeActions> Actions = MakeShareable(new FBentoAssetTypeActions());
	AssetTools.RegisterAssetTypeActions(Actions);
	BentoAssetTypeActions = Actions;

	// 2. Register Slate Tab Spawner
	RegisterTabSpawner();
}

void FBentoPackEditorModule::ShutdownModule()
{
	UnregisterTabSpawner();

	if (FModuleManager::Get().IsModuleLoaded("AssetTools") && BentoAssetTypeActions.IsValid())
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().UnregisterAssetTypeActions(BentoAssetTypeActions.ToSharedRef());
	}
}

void FBentoPackEditorModule::RegisterTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		BentoDashboardTabName,
		FOnSpawnTab::CreateRaw(this, &FBentoPackEditorModule::SpawnBentoDashboardTab))
		.SetDisplayName(LOCTEXT("BentoDashboardTitle", "BentoPack Dashboard"))
		.SetTooltipText(LOCTEXT("BentoDashboardTooltip", "Open the BentoPack 2D Atlas and Watch Daemon Dashboard."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
}

void FBentoPackEditorModule::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BentoDashboardTabName);
}

TSharedRef<SDockTab> FBentoPackEditorModule::SpawnBentoDashboardTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBentoPackDashboard)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBentoPackEditorModule, BentoPackEditor)
