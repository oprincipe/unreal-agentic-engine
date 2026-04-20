#include "UI/SUnrealAgenticChatWidget.h"
#include "UnrealAgenticModule.h"
#include "Settings/UnrealAgenticEditorSettings.h"
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
#include "HttpModule.h"
#include "Http.h"
#include "DragAndDrop/AssetDragDropOp.h"

#define LOCTEXT_NAMESPACE "UnrealAgenticEngineChat"

static constexpr FLinearColor UserBubbleColor(0.12f, 0.18f, 0.28f, 1.0f);
static constexpr FLinearColor AssistantBubbleColor(0.1f, 0.1f, 0.1f, 1.0f);
static constexpr FLinearColor BackgroundColor(0.06f, 0.06f, 0.06f, 1.0f);

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SUnrealAgenticChatWidget::Construct(const FArguments& InArgs)
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
                    .Padding(FMargin(0, 0, 8, 0))
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("RestartAgent", "⟳ Restart Agent"))
                        .ToolTipText(LOCTEXT("RestartAgentTip", "Restart the Python Agent server if it is frozen or blocked."))
                        .OnClicked(this, &SUnrealAgenticChatWidget::OnRestartAgentClicked)
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("NewChat", "+ New Chat"))
                        .ToolTipText(LOCTEXT("NewChatTip", "Start a new conversation. History is saved."))
                        .OnClicked(this, &SUnrealAgenticChatWidget::OnNewChatClicked)
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
                    [
                        SNew(SBox)
                        .MinDesiredHeight(40.f)
                        [
                            SAssignNew(InputTextBox, SMultiLineEditableTextBox)
                            .HintText(LOCTEXT("InputHint", "Ask anything about your Unreal scene... (Shift+Enter for new line)"))
                            .AutoWrapText(true)
                            .OnKeyDownHandler_Lambda([this](const FGeometry&, const FKeyEvent& Key) -> FReply
                            {
                                if (Key.GetKey() == EKeys::Enter && !Key.IsShiftDown())
                                {
                                    return OnSendClicked();
                                }
                                return FReply::Unhandled();
                            })
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Bottom)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("Send", "Send"))
                        .OnClicked(this, &SUnrealAgenticChatWidget::OnSendClicked)
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

SUnrealAgenticChatWidget::~SUnrealAgenticChatWidget()
{
}

FReply SUnrealAgenticChatWidget::OnSendClicked()
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
    const UUnrealAgenticEditorSettings* Settings = GetDefault<UUnrealAgenticEditorSettings>();
    if (!Settings)
    {
        return FReply::Handled();
    }

    // Build JSON payload for the Python Agent HTTP server
    const TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject());
    Payload->SetStringField(TEXT("message"),    UserText);
    Payload->SetStringField(TEXT("session_id"), CurrentSessionId);
    Payload->SetStringField(TEXT("api_key"),    Settings->ApiKey);
    Payload->SetStringField(TEXT("model"),      Settings->ModelName);

    const UEnum* ProviderEnum = StaticEnum<EUnrealAgenticEngineProvider>();
    const FString ProviderStr = ProviderEnum
        ? ProviderEnum->GetDisplayNameTextByValue(static_cast<int64>(Settings->Provider)).ToString()
        : TEXT("Anthropic");
    Payload->SetStringField(TEXT("provider"), ProviderStr);

    FString PayloadStr;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadStr);
    FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);

    // Fire async HTTP POST → Python agent at port 55558
    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("http://127.0.0.1:55558/chat"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetTimeout(300.0f); // 5-minute timeout for slow LLMs
    Request->SetContentAsString(PayloadStr);

    ShowLoadingIndicator();

    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr Req, const FHttpResponsePtr& Resp, const bool bSucceeded)
        {
            HideLoadingIndicator();
            
            FString Reply;
            if (bSucceeded && Resp.IsValid() && Resp->GetResponseCode() == 200)
            {
                TSharedPtr<FJsonObject> RespJson;
                const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
                if (FJsonSerializer::Deserialize(Reader, RespJson) && RespJson.IsValid())
                {
                    RespJson->TryGetStringField(TEXT("reply"), Reply);
                }
            }
            else
            {
                Reply = bSucceeded
                    ? FString::Printf(TEXT("HTTP error %d"), Resp.IsValid() ? Resp->GetResponseCode() : 0)
                    : TEXT("❌ Could not reach Unreal AI Agent. Is 'unreal_agentic_client.py' running?");
            }

            if (!Reply.IsEmpty())
            {
                FChatMessage AssistantMsg;
                AssistantMsg.Role      = TEXT("assistant");
                AssistantMsg.Content   = Reply;
                AssistantMsg.Timestamp = FDateTime::Now();
                Messages.Add(AssistantMsg);
                AddMessageToUI(AssistantMsg);
                SaveMessageToHistory(AssistantMsg);
                ScrollToBottom();
            }
        }
    );
    Request->ProcessRequest();

    return FReply::Handled();
}

void SUnrealAgenticChatWidget::ShowLoadingIndicator()
{
    if (!ChatVBox.IsValid()) return;
    
    LoadingBubbleWidget = SNew(SHorizontalBox)
    + SHorizontalBox::Slot()
    .HAlign(HAlign_Left)
    .FillWidth(1.f)
    [
        SNew(SBorder)
        .Padding(FMargin(10.f, 6.f))
        .BorderBackgroundColor(AssistantBubbleColor)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Agent is thinking and processing scene...\n(This might take a while if indexing large levels)")))
            .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.f))
        ]
    ];
    
    ChatVBox->AddSlot()
    .AutoHeight()
    .Padding(FMargin(8.f, 4.f))
    [
        LoadingBubbleWidget.ToSharedRef()
    ];
    ScrollToBottom();
}

void SUnrealAgenticChatWidget::HideLoadingIndicator()
{
    if (ChatVBox.IsValid() && LoadingBubbleWidget.IsValid())
    {
        ChatVBox->RemoveSlot(LoadingBubbleWidget.ToSharedRef());
        LoadingBubbleWidget.Reset();
    }
}

	FReply SUnrealAgenticChatWidget::OnRestartAgentClicked()
{
#if WITH_EDITOR
	if (FUnrealAgenticModule::IsAvailable())
	{
		FUnrealAgenticModule::Get().RestartAgent();
	}
#endif

	// Show immediate feedback locally
	FChatMessage NoticeMsg;
	NoticeMsg.Role      = TEXT("assistant");
	NoticeMsg.Content   = TEXT("🔄 System: Rebooted Python Agent natively. The script interface has been refreshed.");
	NoticeMsg.Timestamp = FDateTime::Now();
	Messages.Add(NoticeMsg);
	AddMessageToUI(NoticeMsg);
	ScrollToBottom();

	return FReply::Handled();
}

FReply SUnrealAgenticChatWidget::OnNewChatClicked()
{
    ClearCurrentSession();
    return FReply::Handled();
}

void SUnrealAgenticChatWidget::OnInputTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
}

TSharedRef<SWidget> SUnrealAgenticChatWidget::BuildChatBubble(const FChatMessage& Message)
{
    return SNew(SBorder)
        .Padding(FMargin(10.f, 6.f))
        .BorderBackgroundColor(Message.Role == TEXT("user") ? UserBubbleColor : AssistantBubbleColor)
        [
            SNew(SMultiLineEditableText)
            .Text(FText::FromString(Message.Content))
            .AutoWrapText(true)
            .IsReadOnly(true)
        ];
}

void SUnrealAgenticChatWidget::AddMessageToUI(const FChatMessage& Message)
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
                SNew(SMultiLineEditableText)
                .Text(FText::FromString(Message.Content))
                .AutoWrapText(true)
                .IsReadOnly(true)
            ]
        ]
    ];
}

void SUnrealAgenticChatWidget::ScrollToBottom()
{
    if (ChatScrollBox.IsValid())
    {
        ChatScrollBox->ScrollToEnd();
    }
}

// ─────────────── Persistence (JSON in Saved/) ───────────────

static FString GetHistoryFilePath()
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgenticEngine"), TEXT("ChatHistory.json"));
}

void SUnrealAgenticChatWidget::LoadChatHistory()
{
    FString FilePath = GetHistoryFilePath();
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath)) return;

    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *FilePath)) return;

    TArray<TSharedPtr<FJsonValue>> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root)) return;

    for (const TSharedPtr<FJsonValue>& Val : Root)
    {
        const TSharedPtr<FJsonObject>* Obj;
        if (!Val->TryGetObject(Obj)) continue;

        FChatMessage Msg;
        (*Obj)->TryGetStringField(TEXT("role"),    Msg.Role);
        (*Obj)->TryGetStringField(TEXT("content"), Msg.Content);
        
        FString SavedSessionId;
        if ((*Obj)->TryGetStringField(TEXT("session_id"), SavedSessionId) && !SavedSessionId.IsEmpty())
        {
            CurrentSessionId = SavedSessionId;
        }
        
        Messages.Add(Msg);
    }
}

void SUnrealAgenticChatWidget::SaveMessageToHistory(const FChatMessage& Message)
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

    FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgenticEngine"));
    IFileManager::Get().MakeDirectory(*Dir, true);
    FFileHelper::SaveStringToFile(Out, *GetHistoryFilePath());
}

void SUnrealAgenticChatWidget::ClearCurrentSession()
{
    Messages.Empty();
    CurrentSessionId = FGuid::NewGuid().ToString();
    if (ChatVBox.IsValid()) ChatVBox->ClearChildren();
    // Wipe persistence for a clean start
    FFileHelper::SaveStringToFile(TEXT("[]"), *GetHistoryFilePath());
}

FReply SUnrealAgenticChatWidget::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    TSharedPtr<FAssetDragDropOp> AssetDragDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
    if (AssetDragDropOp.IsValid() && AssetDragDropOp->HasAssets())
    {
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply SUnrealAgenticChatWidget::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    TSharedPtr<FAssetDragDropOp> AssetDragDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
    if (AssetDragDropOp.IsValid() && AssetDragDropOp->HasAssets() && InputTextBox.IsValid())
    {
        FString AssetRefs;
        for (const FAssetData& AssetData : AssetDragDropOp->GetAssets())
        {
            if (!AssetRefs.IsEmpty())
            {
                AssetRefs += TEXT(", ");
            }
            AssetRefs += AssetData.GetObjectPathString();
        }

        FString CurrentText = InputTextBox->GetText().ToString();
        if (!CurrentText.IsEmpty() && !CurrentText.EndsWith(TEXT(" ")))
        {
            CurrentText += TEXT(" ");
        }
        CurrentText += AssetRefs;
        
        InputTextBox->SetText(FText::FromString(CurrentText));
        
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
