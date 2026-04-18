#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"

class FEpicUnrealMCPModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FEpicUnrealMCPModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FEpicUnrealMCPModule>("UnrealMCP");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("UnrealMCP");
	}

	TArray<FString> GetRecentLogs() const;
	void ClearLogs();

private:
	void RegisterMenus();
	TSharedRef<SDockTab> OnSpawnChatTab(const FSpawnTabArgs& SpawnTabArgs);

	class FEpicUnrealMCPLogBuffer* LogBuffer = nullptr;
	TSharedPtr<FTabSpawnerEntry> ChatTabSpawner;
}; 