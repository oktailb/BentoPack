#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FBentoPackEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterTabSpawner();
	void UnregisterTabSpawner();

	TSharedRef<class SDockTab> SpawnBentoDashboardTab(const class FSpawnTabArgs& Args);

	TSharedPtr<class IAssetTypeActions> BentoAssetTypeActions;
};
