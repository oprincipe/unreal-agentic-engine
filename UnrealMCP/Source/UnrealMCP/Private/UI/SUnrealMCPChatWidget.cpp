#include "UI/SUnrealMCPChatWidget.h"
#include "Settings/UnrealMCPEditorSettings.h"
#include "EpicUnrealMCPBridge.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/PlatformFileManager.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "UnrealMCPChat"

static const FLinearColor UserBubbleColor(0.12f, 0.18f, 0.28f, 1.0f);
static const FLinearColor AssistantBubbleColor(0.1f, 0.1f, 0.1f, 1.0f);
static const FLinearColor BackgroundColor(0.06f, 0.06f, 0.06f, 1.0f);

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SUnrealMCPChatWidget::Construct(const FArguments& InArgs)
{
    CurrentSessionId = FGuid::NewGuid().ToString();
    LoadChatHistory();

    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(BackgroundColor)
        .Padding(0)
        [
            SNew(SVerticalBox)

            // ──────────────── Top Bar ────────────────
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .Padding(FMargin(8.f, 6.f))
                .BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("ChatTitle", "Unreal AI Agent"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                        .ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f, 1.f))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("NewChat", "+ New Chat"))
                        .ToolTipText(LOCTEXT("NewChatTip", "Start a new conversation. History is saved."))
                        .OnClicked(this, &SUnrealMCPChatWidget::OnNewChatClicked)
                    ]
                ]
            ]

            // ──────────────── Chat Body ────────────────
            + SVerticalBox::Slot()
            .FillHeight(1.f)
            [
                SAssignNew(ChatScrollBox, SScrollBox)
                .ScrollBarAlwaysVisible(false)
                + SScrollBox::Slot()
                [
                    SAssignNew(ChatVBox, SVerticalBox)
                ]
            ]

            // ──────────────── Separator ────────────────
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SSeparator)
                .Orientation(Orient_Horizontal)
            ]

            // ──────────────── Input Bar ────────────────
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .Padding(FMargin(8.f, 6.f))
                .BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.f)
                    .Padding(FMargin(0, 0, 8, 0))
                    [
                        SAssignNew(InputTextBox, SMultiLineEditableTextBox)
                        .HintText(LOCTEXT("InputHint", "Ask anything about your Unreal scene..."))
                        .OnKeyDownHandler_Lambda([this](const FGeometry&, const FKeyEvent& Key) -> FReply
                        {
                            if (Key.GetKey() == EKeys::Enter && !Key.IsShiftDown())
                            {
                                return OnSendClicked();
                            }
                            return FReply::Unhandled();
                        })
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Bottom)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("Send", "Send"))
                        .OnClicked(this, &SUnrealMCPChatWidget::OnSendClicked)
                    ]
                ]
            ]
        ]
    ];

    // Rebuild UI from history
    for (const FChatMessage& Msg : Messages)
    {
        AddMessageToUI(Msg);
    }
    ScrollToBottom();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SUnrealMCPChatWidget::~SUnrealMCPChatWidget()
{
}

FReply SUnrealMCPChatWidget::OnSendClicked()
{
    if (!InputTextBox.IsValid()) return FReply::Handled();

    FString UserText = InputTextBox->GetText().ToString().TrimStartAndEnd();
    if (UserText.IsEmpty()) return FReply::Handled();

    // Add user bubble
    FChatMessage UserMsg;
    UserMsg.Role    = TEXT("user");
    UserMsg.Content = UserText;
    UserMsg.Timestamp = FDateTime::Now();
    Messages.Add(UserMsg);
    AddMessageToUI(UserMsg);
    SaveMessageToHistory(UserMsg);
    InputTextBox->SetText(FText::GetEmpty());
    ScrollToBottom();

    // Get settings
    const UUnrealMCPEditorSettings* Settings = GetDefault<UUnrealMCPEditorSettings>();
    if (!Settings)
    {
        return FReply::Handled();
    }

    // Build payload and dispatch via Bridge
    TSharedPtr<FJsonObject> ChatParams = MakeShareable(new FJsonObject());
    ChatParams->SetStringField(TEXT("message"),    UserText);
    ChatParams->SetStringField(TEXT("session_id"), CurrentSessionId);
    ChatParams->SetStringField(TEXT("api_key"),    Settings->ApiKey);
    ChatParams->SetStringField(TEXT("model"),      Settings->ModelName);
    const UEnum* ProviderEnum = StaticEnum<EUnrealMCPProvider>();
    ChatParams->SetStringField(TEXT("provider"),   ProviderEnum ? ProviderEnum->GetNameStringByValue((int64)Settings->Provider) : TEXT("Anthropic"));

    // Fire and forget - the Python loop will push the response back via a dedicated response channel
    // (Phase 4 - async streaming). For now we do a synchronous TCP call for simplicity.
    UEpicUnrealMCPBridge* Bridge = GEditor ? GEditor->GetEditorSubsystem<UEpicUnrealMCPBridge>() : nullptr;
    if (Bridge)
    {
        FString Response = Bridge->ExecuteCommand(TEXT("chat_message"), ChatParams);

        // Parse response and extract assistant text
        TSharedPtr<FJsonObject> ResponseJson;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);
        if (FJsonSerializer::Deserialize(Reader, ResponseJson) && ResponseJson.IsValid())
        {
            const TSharedPtr<FJsonObject>* ResultObj;
            FString AssistantText;
            if (ResponseJson->TryGetObjectField(TEXT("result"), ResultObj))
            {
                (*ResultObj)->TryGetStringField(TEXT("reply"), AssistantText);
            }

            if (!AssistantText.IsEmpty())
            {
                FChatMessage AssistantMsg;
                AssistantMsg.Role      = TEXT("assistant");
                AssistantMsg.Content   = AssistantText;
                AssistantMsg.Timestamp = FDateTime::Now();
                Messages.Add(AssistantMsg);
                AddMessageToUI(AssistantMsg);
                SaveMessageToHistory(AssistantMsg);
                ScrollToBottom();
            }
        }
    }

    return FReply::Handled();
}

FReply SUnrealMCPChatWidget::OnNewChatClicked()
{
    ClearCurrentSession();
    return FReply::Handled();
}

void SUnrealMCPChatWidget::AddMessageToUI(const FChatMessage& Message)
{
    if (!ChatVBox.IsValid()) return;

    const bool bIsUser = (Message.Role == TEXT("user"));
    const FLinearColor BubbleColor = bIsUser ? UserBubbleColor : AssistantBubbleColor;
    const EHorizontalAlignment BubbleAlign = bIsUser ? HAlign_Right : HAlign_Left;

    ChatVBox->AddSlot()
    .AutoHeight()
    .Padding(FMargin(8.f, 4.f))
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .HAlign(BubbleAlign)
        .FillWidth(1.f)
        [
            SNew(SBorder)
            .Padding(FMargin(10.f, 6.f))
            .BorderBackgroundColor(BubbleColor)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Message.Content))
                .AutoWrapText(true)
                .ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.f))
            ]
        ]
    ];
}

void SUnrealMCPChatWidget::ScrollToBottom()
{
    if (ChatScrollBox.IsValid())
    {
        ChatScrollBox->ScrollToEnd();
    }
}

// ─────────────── Persistence (JSON in Saved/) ───────────────

static FString GetHistoryFilePath()
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealMCP"), TEXT("ChatHistory.json"));
}

void SUnrealMCPChatWidget::LoadChatHistory()
{
    FString FilePath = GetHistoryFilePath();
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath)) return;

    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *FilePath)) return;

    TArray<TSharedPtr<FJsonValue>> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root)) return;

    for (const TSharedPtr<FJsonValue>& Val : Root)
    {
        const TSharedPtr<FJsonObject>* Obj;
        if (!Val->TryGetObject(Obj)) continue;

        FChatMessage Msg;
        (*Obj)->TryGetStringField(TEXT("role"),    Msg.Role);
        (*Obj)->TryGetStringField(TEXT("content"), Msg.Content);
        Messages.Add(Msg);
    }
}

void SUnrealMCPChatWidget::SaveMessageToHistory(const FChatMessage& Message)
{
    // Build full history array and rewrite the file
    TArray<TSharedPtr<FJsonValue>> Root;
    for (const FChatMessage& Msg : Messages)
    {
        TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject());
        Obj->SetStringField(TEXT("role"),       Msg.Role);
        Obj->SetStringField(TEXT("content"),    Msg.Content);
        Obj->SetStringField(TEXT("session_id"), CurrentSessionId);
        Root.Add(MakeShareable(new FJsonValueObject(Obj)));
    }

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Root, Writer);

    FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealMCP"));
    IFileManager::Get().MakeDirectory(*Dir, true);
    FFileHelper::SaveStringToFile(Out, *GetHistoryFilePath());
}

void SUnrealMCPChatWidget::ClearCurrentSession()
{
    Messages.Empty();
    CurrentSessionId = FGuid::NewGuid().ToString();
    if (ChatVBox.IsValid()) ChatVBox->ClearChildren();
    // Wipe persistence for a clean start
    FFileHelper::SaveStringToFile(TEXT("[]"), *GetHistoryFilePath());
}

#undef LOCTEXT_NAMESPACE
