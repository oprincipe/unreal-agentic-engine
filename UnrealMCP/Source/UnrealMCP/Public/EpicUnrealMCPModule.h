#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformProcess.h"

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

	/** Starts unreal_mcp_agent.py if not already running. Can be called externally (e.g. from Settings). */
	void EnsureAgentRunning();

	/** Kills the agent process if running, and relaunches it. */
	void RestartAgent();

private:
	void RegisterMenus();
	TSharedRef<SDockTab> OnSpawnChatTab(const FSpawnTabArgs& SpawnTabArgs);

	/** Kills the agent process on shutdown. */
	void StopAgent();

	class FEpicUnrealMCPLogBuffer* LogBuffer = nullptr;
	TSharedPtr<FTabSpawnerEntry>   ChatTabSpawner;
	FProcHandle                    AgentProcess;
}; 