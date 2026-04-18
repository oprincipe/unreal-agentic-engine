#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class FUnrealMCPEditorSettingsCustomization : public IDetailCustomization
{
public:
    /** Makes a new instance of this detail layout class for a specific detail view requesting it */
    static TSharedRef<IDetailCustomization> MakeInstance();

    // IDetailCustomization interface
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    FReply OnTestConnectionClicked();
};
#endif
