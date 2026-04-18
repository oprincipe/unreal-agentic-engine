#include "Settings/UnrealMCPEditorSettings.h"

UUnrealMCPEditorSettings::UUnrealMCPEditorSettings()
{
    Provider = EUnrealMCPProvider::Anthropic;
    ApiKey = TEXT("");
    OllamaServerUrl = TEXT("http://localhost:11434");
    ModelName = TEXT("claude-3-5-sonnet-latest");
    bAutoInstallPythonDependencies = true;
}
