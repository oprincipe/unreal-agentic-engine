#include "Settings/UnrealMCPEditorSettingsCustomization.h"

#if WITH_EDITOR
#include "Settings/UnrealMCPEditorSettings.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Misc/MessageDialog.h"
#include "HttpModule.h"
#include "Http.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

TSharedRef<IDetailCustomization> FUnrealMCPEditorSettingsCustomization::MakeInstance()
{
    return MakeShareable(new FUnrealMCPEditorSettingsCustomization);
}

void FUnrealMCPEditorSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    CachedDetailBuilder = &DetailBuilder;

    // ─── Hook Provider change → force panel rebuild ───
    TSharedRef<IPropertyHandle> ProviderProp = DetailBuilder.GetProperty(
        GET_MEMBER_NAME_CHECKED(UUnrealMCPEditorSettings, Provider));

    ProviderProp->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this]()
    {
        if (CachedDetailBuilder)
        {
            CachedDetailBuilder->ForceRefreshDetails();
        }
    }));

    // ─── Hide/show ApiKey vs OllamaServerUrl ───
    TSharedRef<IPropertyHandle> ApiKeyProp      = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UUnrealMCPEditorSettings, ApiKey));
    TSharedRef<IPropertyHandle> OllamaUrlProp   = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UUnrealMCPEditorSettings, OllamaServerUrl));
    TSharedRef<IPropertyHandle> ModelNameProp   = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UUnrealMCPEditorSettings, ModelName));

    // Determine current visibility
    bool bIsOllama = (GetDefault<UUnrealMCPEditorSettings>()->Provider == EUnrealMCPProvider::Ollama);

    // Hide the one not in use
    if (bIsOllama)
    {
        DetailBuilder.HideProperty(ApiKeyProp);
    }
    else
    {
        DetailBuilder.HideProperty(OllamaUrlProp);
    }

    // ─── Hide ModelName default row so we can rebuild it with the Refresh button ───
    DetailBuilder.HideProperty(ModelNameProp);

    IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("AI Configuration");

    // ─── ModelName row with inline Refresh button ───
    Category.AddCustomRow(FText::FromString("Model Name"))
    .NameContent()
    [
        SNew(STextBlock)
        .Text(FText::FromString("Model Name"))
        .Font(IDetailLayoutBuilder::GetDetailFont())
    ]
    .ValueContent()
    .MinDesiredWidth(300.f)
    [
        SNew(SHorizontalBox)
        // The actual text field proxied from property
        + SHorizontalBox::Slot()
        .FillWidth(1.f)
        [
            ModelNameProp->CreatePropertyValueWidget()
        ]
        // Refresh button
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.f, 0.f, 0.f, 0.f)
        .VAlign(VAlign_Center)
        [
            SNew(SButton)
            .Text(FText::FromString(bIsOllama ? "↻ Load Models" : "↻ Refresh"))
            .ToolTipText(FText::FromString("Fetch available models from the selected provider."))
            .OnClicked(this, &FUnrealMCPEditorSettingsCustomization::OnRefreshModelsClicked)
        ]
    ];

    // ─── Test Connection row ───
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
        .ToolTipText(FText::FromString("Verify API key / Ollama URL is reachable."))
        .OnClicked(this, &FUnrealMCPEditorSettingsCustomization::OnTestConnectionClicked)
    ];
}

// ─────────────────────────────────────────────────────────────────────────────
FReply FUnrealMCPEditorSettingsCustomization::OnRefreshModelsClicked()
{
    const UUnrealMCPEditorSettings* Settings = GetDefault<UUnrealMCPEditorSettings>();
    if (!Settings) return FReply::Handled();

    bool bIsOllama = (Settings->Provider == EUnrealMCPProvider::Ollama);

    if (bIsOllama)
    {
        // HTTP GET to Ollama /api/tags via Python agent endpoint
        FString JsonBody = FString::Printf(
            TEXT("{\"api_key\":\"%s\"}"), *Settings->OllamaServerUrl);

        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
        Req->SetURL(TEXT("http://127.0.0.1:55558/models/ollama"));
        Req->SetVerb(TEXT("POST"));
        Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        Req->SetContentAsString(JsonBody);
        Req->OnProcessRequestComplete().BindLambda(
            [](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK)
            {
                if (!bOK || !Resp.IsValid() || Resp->GetResponseCode() != 200)
                {
                    FMessageDialog::Open(EAppMsgType::Ok,
                        FText::FromString("Could not reach Ollama. Is the server running?"));
                    return;
                }
                TSharedPtr<FJsonObject> Json;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
                if (!FJsonSerializer::Deserialize(Reader, Json)) return;

                TArray<TSharedPtr<FJsonValue>> Models;
                if (Json->TryGetArrayField(TEXT("models"), Models))
                {
                    TArray<FString> Names;
                    for (auto& M : Models)
                    {
                        FString Name;
                        if (M->TryGetString(Name)) Names.Add(Name);
                    }
                    FString List = FString::Join(Names, TEXT("\n"));
                    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
                        FString::Printf(TEXT("Available Ollama Models:\n\n%s"), *List)));
                }
            });
        Req->ProcessRequest();
    }
    else
    {
        // For cloud providers we show a hardcoded list of popular models
        FString Models;
        switch (Settings->Provider)
        {
        case EUnrealMCPProvider::Anthropic:
            Models = TEXT("claude-opus-4-5\nclaude-sonnet-4-5\nclaude-3-5-sonnet-latest\nclaude-3-5-haiku-latest");
            break;
        case EUnrealMCPProvider::OpenAI:
            Models = TEXT("gpt-4o\ngpt-4o-mini\ngpt-4-turbo\ngpt-3.5-turbo");
            break;
        case EUnrealMCPProvider::Google:
            Models = TEXT("gemini-2.0-flash\ngemini-2.0-flash-lite\ngemini-1.5-pro\ngemini-1.5-flash");
            break;
        default:
            break;
        }
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
            FString::Printf(TEXT("Common models for this provider:\n\n%s\n\nCopy and paste one into the Model Name field."), *Models)));
    }

    return FReply::Handled();
}

FReply FUnrealMCPEditorSettingsCustomization::OnTestConnectionClicked()
{
    const UUnrealMCPEditorSettings* Settings = GetDefault<UUnrealMCPEditorSettings>();
    if (!Settings) return FReply::Handled();

    const UEnum* ProviderEnum = StaticEnum<EUnrealMCPProvider>();
    FString ProviderStr = ProviderEnum
        ? ProviderEnum->GetDisplayNameTextByValue((int64)Settings->Provider).ToString()
        : TEXT("Anthropic");

    FString ApiKeyOrUrl = (Settings->Provider == EUnrealMCPProvider::Ollama)
        ? Settings->OllamaServerUrl
        : Settings->ApiKey;

    FString JsonBody = FString::Printf(
        TEXT("{\"provider\":\"%s\",\"api_key\":\"%s\",\"model\":\"%s\"}"),
        *ProviderStr, *ApiKeyOrUrl, *Settings->ModelName);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(TEXT("http://127.0.0.1:55558/test"));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetContentAsString(JsonBody);
    Req->OnProcessRequestComplete().BindLambda(
        [](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK)
        {
            FString ResultText;
            if (bOK && Resp.IsValid() && Resp->GetResponseCode() == 200)
            {
                TSharedPtr<FJsonObject> Json;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
                if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
                {
                    Json->TryGetStringField(TEXT("result"), ResultText);
                }
            }
            else
            {
                ResultText = bOK
                    ? FString::Printf(TEXT("HTTP %d — Is the Python agent running?"), Resp.IsValid() ? Resp->GetResponseCode() : 0)
                    : TEXT("Could not reach agent. Open 'Tools → Unreal AI Agent' first to auto-start it.");
            }
            FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(ResultText));
        });
    Req->ProcessRequest();

    return FReply::Handled();
}

EVisibility FUnrealMCPEditorSettingsCustomization::GetApiKeyVisibility() const
{
    const UUnrealMCPEditorSettings* Settings = GetDefault<UUnrealMCPEditorSettings>();
    return (Settings && Settings->Provider == EUnrealMCPProvider::Ollama)
        ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility FUnrealMCPEditorSettingsCustomization::GetOllamaUrlVisibility() const
{
    const UUnrealMCPEditorSettings* Settings = GetDefault<UUnrealMCPEditorSettings>();
    return (Settings && Settings->Provider == EUnrealMCPProvider::Ollama)
        ? EVisibility::Visible : EVisibility::Collapsed;
}

#endif
