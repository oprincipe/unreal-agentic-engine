#include "Settings/UnrealAgenticEditorSettings.h"

UUnrealAgenticEditorSettings::UUnrealAgenticEditorSettings()
{
    Provider = EUnrealAgenticEngineProvider::Anthropic;
    ApiKey = TEXT("");
    OllamaServerUrl = TEXT("http://localhost:11434");
    ModelName = TEXT("claude-3-5-sonnet-latest");
    bAutoInstallPythonDependencies = true;
}
