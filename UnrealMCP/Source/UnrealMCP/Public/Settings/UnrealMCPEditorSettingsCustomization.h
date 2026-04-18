#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class IDetailLayoutBuilder;

class FUnrealMCPEditorSettingsCustomization : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();

    // IDetailCustomization interface
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    FReply OnTestConnectionClicked();
    FReply OnRefreshModelsClicked();

    /** Returns whether the API Key row should be visible (i.e. provider != Ollama). */
    EVisibility GetApiKeyVisibility()      const;
    /** Returns whether the Ollama URL row should be visible (provider == Ollama). */
    EVisibility GetOllamaUrlVisibility()   const;

    IDetailLayoutBuilder* CachedDetailBuilder = nullptr;
};
#endif
