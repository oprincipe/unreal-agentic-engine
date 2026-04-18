#include "Settings/UnrealMCPEditorSettings.h"

UUnrealMCPEditorSettings::UUnrealMCPEditorSettings()
{
    Provider = EUnrealMCPProvider::Anthropic;
    ApiKey = TEXT("");
    ModelName = TEXT("claude-3-5-sonnet-latest");
    bAutoInstallPythonDependencies = true;
}
