#include "EpicUnrealMCPModule.h"
#include "EpicUnrealMCPBridge.h"
#include "Modules/ModuleManager.h"
#include "EditorSubsystem.h"
#include "Editor.h"

#if WITH_EDITOR
#include "PropertyEditorModule.h"
#include "Settings/UnrealMCPEditorSettingsCustomization.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "LevelEditor.h"
#include "UI/SUnrealMCPChatWidget.h"
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
	// Register settings detail customization
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
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SUnrealMCPChatWidget)
		];
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