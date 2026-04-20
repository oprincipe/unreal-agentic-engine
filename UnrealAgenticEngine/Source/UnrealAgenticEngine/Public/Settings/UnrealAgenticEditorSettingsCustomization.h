#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class IDetailLayoutBuilder;

class FUnrealAgenticEditorSettingsCustomization : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    FReply OnTestConnectionClicked();
    FReply OnRefreshModelsClicked();

    EVisibility GetApiKeyVisibility()    const;
    EVisibility GetOllamaUrlVisibility() const;

    IDetailLayoutBuilder* CachedDetailBuilder = nullptr;
    FTimerHandle          TestConnectionTimerHandle;
};
#endif
