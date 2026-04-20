#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UnrealAgenticEditorSettings.generated.h"

UENUM(BlueprintType)
enum class EUnrealAgenticEngineProvider : uint8
{
    Anthropic UMETA(DisplayName = "Anthropic / Claude"),
    OpenAI    UMETA(DisplayName = "OpenAI"),
    Google    UMETA(DisplayName = "Google / Gemini"),
    Ollama    UMETA(DisplayName = "Ollama (Local)")
};

/**
 * Settings for the Unreal AI MCP Agent.
 * Accessible from Edit -> Project Settings -> Plugins -> Unreal AI Agent
 */
UCLASS(config=EditorPerProjectUserSettings, defaultconfig, meta=(DisplayName="Unreal AI Agent"))
class UNREALAGENTICENGINE_API UUnrealAgenticEditorSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UUnrealAgenticEditorSettings();

    // ── Required for the page to appear in Project Settings ──
    virtual FName GetCategoryName() const override { return FName("Plugins"); }
    virtual FText GetSectionText()  const override { return FText::FromString("Unreal AI Agent"); }
    virtual FText GetSectionDescription() const override
    {
        return FText::FromString("Configure the AI provider used by the native Unreal AI Agent chat panel.");
    }

    /** The AI Provider to use for chatting and tool execution. */
    UPROPERTY(config, EditAnywhere, Category = "AI Configuration")
    EUnrealAgenticEngineProvider Provider;

    /** API Key for the selected provider. Leave blank for Ollama (use server URL instead). */
    UPROPERTY(config, EditAnywhere, Category = "AI Configuration", meta=(PasswordField=true))
    FString ApiKey;

    /** Ollama server URL. Only used when Provider is set to Ollama. */
    UPROPERTY(config, EditAnywhere, Category = "AI Configuration")
    FString OllamaServerUrl;

    /**
     * Model name to use.
     * Examples: claude-3-5-sonnet-20241022, gpt-4o, gemini-2.0-flash, llama3:latest
     */
    UPROPERTY(config, EditAnywhere, Category = "AI Configuration")
    FString ModelName;

    /**
     * If true, the plugin automatically runs 'pip install -r requirements.txt'
     * the first time the chat panel is opened.
     */
    UPROPERTY(config, EditAnywhere, Category = "Python Environment")
    bool bAutoInstallPythonDependencies;
};
