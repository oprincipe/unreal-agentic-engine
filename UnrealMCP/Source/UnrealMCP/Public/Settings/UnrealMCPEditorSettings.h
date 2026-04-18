#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UnrealMCPEditorSettings.generated.h"

UENUM(BlueprintType)
enum class EUnrealMCPProvider : uint8
{
    Anthropic UMETA(DisplayName = "Anthropic / Claude"),
    OpenAI UMETA(DisplayName = "OpenAI"),
    Google UMETA(DisplayName = "Google / Gemini"),
    Ollama UMETA(DisplayName = "Ollama (Local)")
};

/**
 * Settings for the Unreal AI MCP Agent.
 * Accessible from Project Settings -> Plugins -> Unreal AI Agent
 */
UCLASS(config=EditorPerProjectUserSettings, defaultconfig, meta=(DisplayName="Unreal AI Agent"))
class UNREALMCP_API UUnrealMCPEditorSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UUnrealMCPEditorSettings();

    /** The AI Provider to use for chatting and tool execution. */
    UPROPERTY(config, EditAnywhere, Category = "AI Configuration")
    EUnrealMCPProvider Provider;

    /** API Key for the selected provider. Leave blank for Ollama. */
    UPROPERTY(config, EditAnywhere, Category = "AI Configuration", meta=(PasswordField=true))
    FString ApiKey;

    /** 
     * Model name to use. 
     * Examples: claude-3-5-sonnet-20241022, gpt-4o, llama3:latest 
     */
    UPROPERTY(config, EditAnywhere, Category = "AI Configuration")
    FString ModelName;

    /** 
     * If true, the plugin will attempt to 'uv pip install' requirements 
     * for the Python environment automatically upon Editor startup.
     */
    UPROPERTY(config, EditAnywhere, Category = "Environment Initialization")
    bool bAutoInstallPythonDependencies;
};
