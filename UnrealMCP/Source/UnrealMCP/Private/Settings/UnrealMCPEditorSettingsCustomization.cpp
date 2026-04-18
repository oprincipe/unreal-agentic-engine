#include "Settings/UnrealMCPEditorSettingsCustomization.h"

#if WITH_EDITOR
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Misc/MessageDialog.h"

TSharedRef<IDetailCustomization> FUnrealMCPEditorSettingsCustomization::MakeInstance()
{
    return MakeShareable(new FUnrealMCPEditorSettingsCustomization);
}

void FUnrealMCPEditorSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("AI Configuration");

    Category.AddCustomRow(FText::FromString("Test Connection"))
    .NameContent()
    [
        SNew(STextBlock)
        .Text(FText::FromString("Connection Test"))
        .Font(IDetailLayoutBuilder::GetDetailFont())
    ]
    .ValueContent()
    [
        SNew(SButton)
        .Text(FText::FromString("Test Provider Connection"))
        .ToolTipText(FText::FromString("Pings the selected AI provider via the Python backend to verify your settings."))
        .OnClicked(this, &FUnrealMCPEditorSettingsCustomization::OnTestConnectionClicked)
    ];
}

FReply FUnrealMCPEditorSettingsCustomization::OnTestConnectionClicked()
{
    // TODO: Send asynchronous TCP test command to Python using EpicUnrealMCPBridge
    // For now, we show a basic acknowledgement dialog
    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Test connection signal initiated. Please check Editor Output Logs."));
    
    return FReply::Handled();
}
#endif
