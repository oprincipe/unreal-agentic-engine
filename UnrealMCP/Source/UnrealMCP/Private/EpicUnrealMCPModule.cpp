#include "EpicUnrealMCPModule.h"
#include "EpicUnrealMCPBridge.h"
#include "Modules/ModuleManager.h"
#include "EditorSubsystem.h"
#include "Editor.h"

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
}

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