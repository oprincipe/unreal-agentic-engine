#include "EpicUnrealMCPModule.h"
#include "Modules/ModuleManager.h"
#include "EditorSubsystem.h"
#include "Editor.h"

#if WITH_EDITOR
#include "PropertyEditorModule.h"
#include "Settings/UnrealMCPEditorSettingsCustomization.h"
#include "Settings/UnrealMCPEditorSettings.h"
#include "ISettingsModule.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "UI/SUnrealMCPChatWidget.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/MessageDialog.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#endif

#include "Misc/OutputDevice.h"

#define LOCTEXT_NAMESPACE "FEpicUnrealMCPModule"

class FEpicUnrealMCPLogBuffer : public FOutputDevice
{
public:
	TArray<FString> BufferedLogs;
	int32 MaxLogs = 500;
	FCriticalSection LogMutex;

	FEpicUnrealMCPLogBuffer()
	{
		BufferedLogs.Reserve(MaxLogs);
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		// Only capture warning and error severity to save space, or capture all but keep it short
		// Let's capture at least warnings and errors, and some display
		if (Verbosity <= ELogVerbosity::Warning || Verbosity == ELogVerbosity::Display)
		{
			FScopeLock Lock(&LogMutex);
			if (BufferedLogs.Num() >= MaxLogs)
			{
				BufferedLogs.RemoveAt(0);
			}
			FString Prefix = FString::Printf(TEXT("[%s][%s] "), *Category.ToString(), ToString(Verbosity));
			BufferedLogs.Add(Prefix + V);
		}
	}
};

void FEpicUnrealMCPModule::StartupModule()
{
	UE_LOG(LogTemp, Display, TEXT("Epic Unreal MCP Module has started"));
	LogBuffer = new FEpicUnrealMCPLogBuffer();
	GLog->AddOutputDevice(LogBuffer);

#if WITH_EDITOR
	// Register settings page via ISettingsModule (most reliable way in UE 5.5+)
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
			"Project", "Plugins", "UnrealAIAgent",
			FText::FromString("Unreal AI Agent"),
			FText::FromString("Configure the AI provider for the native Unreal AI Agent chat panel."),
			GetMutableDefault<UUnrealMCPEditorSettings>()
		);
	}

	// Register settings detail customization (Test button)
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(
		"UnrealMCPEditorSettings",
		FOnGetDetailCustomizationInstance::CreateStatic(&FUnrealMCPEditorSettingsCustomization::MakeInstance)
	);

	// Register the dockable Chat Tab
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		FName("UnrealMCPChatTab"),
		FOnSpawnTab::CreateRaw(this, &FEpicUnrealMCPModule::OnSpawnChatTab)
	)
	.SetDisplayName(LOCTEXT("ChatTabTitle", "Unreal AI Agent"))
	.SetTooltipText(LOCTEXT("ChatTabTooltip", "Chat with the AI Agent directly inside Unreal Engine."))
	.SetMenuType(ETabSpawnerMenuType::Hidden); // We add it manually via ToolMenus below

	// Defer menu registration until ToolMenus is ready
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
		this, &FEpicUnrealMCPModule::RegisterMenus
	));
#endif
}

void FEpicUnrealMCPModule::ShutdownModule()
{
	UE_LOG(LogTemp, Display, TEXT("Epic Unreal MCP Module has shut down"));
	if (LogBuffer)
	{
		GLog->RemoveOutputDevice(LogBuffer);
		delete LogBuffer;
		LogBuffer = nullptr;
	}

#if WITH_EDITOR
	StopAgent();

	// Unregister settings page
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "UnrealAIAgent");
	}

	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(FName("UnrealMCPChatTab"));

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("UnrealMCPEditorSettings");
	}
#endif
}

#if WITH_EDITOR
void FEpicUnrealMCPModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("MainFrame.MainMenu.Tools");
	if (!Menu) return;

	FToolMenuSection& Section = Menu->FindOrAddSection("UnrealMCPSection");
	Section.AddMenuEntry(
		"OpenUnrealMCPChat",
		LOCTEXT("OpenChatLabel",   "Unreal AI Agent"),
		LOCTEXT("OpenChatTooltip", "Open the Unreal AI Agent chat window."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() {
			FGlobalTabmanager::Get()->TryInvokeTab(FName("UnrealMCPChatTab"));
		}))
	);
}

TSharedRef<SDockTab> FEpicUnrealMCPModule::OnSpawnChatTab(const FSpawnTabArgs& SpawnTabArgs)
{
	EnsureAgentRunning(); // launch Python agent if not already running

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SUnrealMCPChatWidget)
		];
}

void FEpicUnrealMCPModule::EnsureAgentRunning()
{
	// Already running?
	if (AgentProcess.IsValid() && FPlatformProcess::IsProcRunning(AgentProcess))
	{
		return;
	}

	// Locate python executable
	FString PythonExe = TEXT("python");
#if PLATFORM_WINDOWS
	PythonExe = TEXT("python.exe");
#elif PLATFORM_MAC || PLATFORM_LINUX
	PythonExe = TEXT("python3");
#endif

	// Locate unreal_mcp_agent.py relative to this plugin
	FString PluginDir = IPluginManager::Get().FindPlugin(TEXT("UnrealMCP"))->GetBaseDir();
	FString ScriptPath = FPaths::Combine(PluginDir, TEXT("Python"), TEXT("unreal_mcp_agent.py"));
	ScriptPath = FPaths::ConvertRelativePathToFull(ScriptPath);
	FPaths::NormalizeFilename(ScriptPath);

	if (!FPaths::FileExists(ScriptPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealMCP: Agent script not found at %s"), *ScriptPath);
		// Show user-facing notification
		TSharedRef<FTokenizedMessage> Msg = FTokenizedMessage::Create(EMessageSeverity::Warning);
		Msg->AddToken(FTextToken::Create(FText::FromString(
			FString::Printf(TEXT("Unreal AI Agent: script not found at '%s'. Check your plugin installation."), *ScriptPath)
		)));
		FMessageLog("PIE").AddMessage(Msg);
		return;
	}

	// Build project Saved path so agent knows where to create the SQLite DB
	FString SavedDir = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealMCP"))
	);
	FString DbPath = FPaths::Combine(SavedDir, TEXT("chat_history.db"));

	FString Args = FString::Printf(TEXT("\"%s\""), *ScriptPath);

	// Expose the project DB path to the Python agent via environment variable.
	// Both unreal_mcp_agent.py and graph_memory.py read UNREALMCP_DB_PATH to
	// locate the correct [PROJECT]/Saved/UnrealMCP/ directory.
	FPlatformMisc::SetEnvironmentVar(TEXT("UNREALMCP_DB_PATH"), *DbPath);

	uint32 OutProcessId = 0;
	AgentProcess = FPlatformProcess::CreateProc(
		*PythonExe,
		*Args,
		/*bLaunchDetached=*/ false,
		/*bLaunchHidden=*/   true,
		/*bLaunchReallyHidden=*/ true,
		&OutProcessId,
		/*PriorityModifier=*/ 0,
		/*OptionalWorkingDirectory=*/ nullptr,
		/*PipeWriteChild=*/ nullptr
	);

	if (AgentProcess.IsValid())
	{
		UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Launched Python Agent (PID %d)"), OutProcessId);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCP: Failed to launch Python. Is Python installed and in your PATH?"));
		// Show user-facing error dialog once
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
			TEXT("⚠️ Unreal AI Agent could not start the Python backend.\n\n"
			     "Please make sure Python 3.10+ is installed and available in your system PATH.\n\n"
			     "You can download it from https://www.python.org/downloads/")
		));
	}
}

void FEpicUnrealMCPModule::StopAgent()
{
	if (AgentProcess.IsValid())
	{
		if (FPlatformProcess::IsProcRunning(AgentProcess))
		{
			FPlatformProcess::TerminateProc(AgentProcess, /*bKillTree=*/ true);
			UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Python Agent terminated."));
		}
		FPlatformProcess::CloseProc(AgentProcess);
	}
}

void FEpicUnrealMCPModule::RestartAgent()
{
	StopAgent();
	EnsureAgentRunning();
}
#endif

TArray<FString> FEpicUnrealMCPModule::GetRecentLogs() const
{
	if (LogBuffer)
	{
		FScopeLock Lock(&LogBuffer->LogMutex);
		return LogBuffer->BufferedLogs;
	}
	return TArray<FString>();
}

void FEpicUnrealMCPModule::ClearLogs()
{
	if (LogBuffer)
	{
		FScopeLock Lock(&LogBuffer->LogMutex);
		LogBuffer->BufferedLogs.Empty(LogBuffer->MaxLogs);
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FEpicUnrealMCPModule, UnrealMCP) 