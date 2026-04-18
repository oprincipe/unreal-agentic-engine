#include "Settings/UnrealMCPEditorSettingsCustomization.h"

#if WITH_EDITOR
#include "Settings/UnrealMCPEditorSettings.h"
#include "EpicUnrealMCPModule.h"
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

// ── Static model cache (persists across detail panel rebuilds) ──
static TArray<TSharedPtr<FString>> GModelOptions;
static EUnrealMCPProvider          GModelOptionsProvider = EUnrealMCPProvider::Anthropic;

static TArray<FString> GetDefaultModelsForProvider(EUnrealMCPProvider Provider)
{
    switch (Provider)
    {
    case EUnrealMCPProvider::Anthropic:
        return { TEXT("claude-opus-4-5"), TEXT("claude-sonnet-4-5"),
                 TEXT("claude-3-5-sonnet-latest"), TEXT("claude-3-5-haiku-latest") };
    case EUnrealMCPProvider::OpenAI:
        return { TEXT("gpt-4o"), TEXT("gpt-4o-mini"), TEXT("gpt-4-turbo"), TEXT("gpt-3.5-turbo") };
    case EUnrealMCPProvider::Google:
        return { TEXT("gemini-2.0-flash"), TEXT("gemini-2.0-flash-lite"),
                 TEXT("gemini-1.5-pro"), TEXT("gemini-1.5-flash") };
    case EUnrealMCPProvider::Ollama:
        return {};   // populated at runtime via REST
    default:
        return {};
    }
}

static void RebuildModelOptions(EUnrealMCPProvider Provider, const TArray<FString>& Models)
{
    GModelOptions.Empty();
    GModelOptionsProvider = Provider;
    for (const FString& M : Models)
    {
        GModelOptions.Add(MakeShared<FString>(M));
    }
}

// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<IDetailCustomization> FUnrealMCPEditorSettingsCustomization::MakeInstance()
{
    return MakeShareable(new FUnrealMCPEditorSettingsCustomization);
}

void FUnrealMCPEditorSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    CachedDetailBuilder = &DetailBuilder;

    const UUnrealMCPEditorSettings* Settings = GetDefault<UUnrealMCPEditorSettings>();
    EUnrealMCPProvider CurrentProvider = Settings ? Settings->Provider : EUnrealMCPProvider::Anthropic;

    // ── Rebuild static list if provider changed ──
    if (GModelOptionsProvider != CurrentProvider || GModelOptions.IsEmpty())
    {
        RebuildModelOptions(CurrentProvider, GetDefaultModelsForProvider(CurrentProvider));
    }

    // ── Hook Provider change → force panel rebuild + reset model list ──
    TSharedRef<IPropertyHandle> ProviderProp = DetailBuilder.GetProperty(
        GET_MEMBER_NAME_CHECKED(UUnrealMCPEditorSettings, Provider));

    ProviderProp->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this]()
    {
        const UUnrealMCPEditorSettings* S = GetDefault<UUnrealMCPEditorSettings>();
        if (S)
        {
            RebuildModelOptions(S->Provider, GetDefaultModelsForProvider(S->Provider));
        }
        if (CachedDetailBuilder)
        {
            CachedDetailBuilder->ForceRefreshDetails();
        }
    }));

    // ── Hide/show ApiKey vs OllamaServerUrl ──
    bool bIsOllama = (CurrentProvider == EUnrealMCPProvider::Ollama);
    TSharedRef<IPropertyHandle> ApiKeyProp    = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UUnrealMCPEditorSettings, ApiKey));
    TSharedRef<IPropertyHandle> OllamaUrlProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UUnrealMCPEditorSettings, OllamaServerUrl));
    TSharedRef<IPropertyHandle> ModelNameProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UUnrealMCPEditorSettings, ModelName));

    if (bIsOllama)   DetailBuilder.HideProperty(ApiKeyProp);
    else             DetailBuilder.HideProperty(OllamaUrlProp);

    // ── Hide default ModelName row — replaced with our ComboBox ──
    DetailBuilder.HideProperty(ModelNameProp);

    IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("AI Configuration");

    // ── Make sure current value is in the list ──
    FString CurrentModel = Settings ? Settings->ModelName : TEXT("");
    bool bCurrentInList = GModelOptions.ContainsByPredicate(
        [&](const TSharedPtr<FString>& S){ return S.IsValid() && *S == CurrentModel; });
    if (!CurrentModel.IsEmpty() && !bCurrentInList)
    {
        GModelOptions.Insert(MakeShared<FString>(CurrentModel), 0);
    }

    // ── Model dropdown row ──
    Category.AddCustomRow(FText::FromString("Model"))
    .NameContent()
    [
        SNew(STextBlock)
        .Text(FText::FromString("Model"))
        .Font(IDetailLayoutBuilder::GetDetailFont())
    ]
    .ValueContent()
    .MinDesiredWidth(320.f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .FillWidth(1.f)
        [
            SNew(SComboBox<TSharedPtr<FString>>)
            .OptionsSource(&GModelOptions)
            .InitiallySelectedItem(GModelOptions.FindByPredicate(
                [&](const TSharedPtr<FString>& S){ return S.IsValid() && *S == CurrentModel; })
                ? *GModelOptions.FindByPredicate([&](const TSharedPtr<FString>& S){ return S.IsValid() && *S == CurrentModel; })
                : (GModelOptions.Num() > 0 ? GModelOptions[0] : nullptr))
            .OnSelectionChanged_Lambda([ModelNameProp](TSharedPtr<FString> Item, ESelectInfo::Type)
            {
                if (Item.IsValid())
                {
                    ModelNameProp->SetValue(*Item);
                }
            })
            .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) -> TSharedRef<SWidget>
            {
                return SNew(STextBlock)
                    .Text(FText::FromString(Item.IsValid() ? *Item : TEXT("")));
            })
            [
                SNew(STextBlock)
                .Text_Lambda([CurrentModel]() { return FText::FromString(CurrentModel); })
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.f, 0.f, 0.f, 0.f)
        .VAlign(VAlign_Center)
        [
            SNew(SButton)
            .Text(FText::FromString(bIsOllama ? TEXT("↻ Load") : TEXT("↻")))
            .ToolTipText(FText::FromString(bIsOllama
                ? TEXT("Fetch available models from your Ollama server.")
                : TEXT("Show available models for this provider.")))
            .OnClicked(this, &FUnrealMCPEditorSettingsCustomization::OnRefreshModelsClicked)
        ]
    ];

    // ── Test Connection row ──
    Category.AddCustomRow(FText::FromString("Test Connection"))
    .NameContent()
    [
        SNew(STextBlock)
        .Text(FText::FromString("Connection"))
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
        FString OllamaTagsUrl = Settings->OllamaServerUrl;
        if (!OllamaTagsUrl.EndsWith(TEXT("/"))) OllamaTagsUrl += TEXT("/");
        OllamaTagsUrl += TEXT("api/tags");

        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
        Req->SetURL(OllamaTagsUrl);
        Req->SetVerb(TEXT("GET"));
        Req->OnProcessRequestComplete().BindLambda(
            [this](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK)
            {
                if (!bOK || !Resp.IsValid() || Resp->GetResponseCode() != 200)
                {
                    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
                        TEXT("Could not reach Ollama. Check the Server URL in settings.")));
                    return;
                }
                TSharedPtr<FJsonObject> Json;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
                if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid()) return;

                const TArray<TSharedPtr<FJsonValue>>* ModelsPtr = nullptr;
                TArray<FString> Names;
                if (Json->TryGetArrayField(TEXT("models"), ModelsPtr) && ModelsPtr)
                {
                    for (const TSharedPtr<FJsonValue>& M : *ModelsPtr)
                    {
                        const TSharedPtr<FJsonObject>* Obj;
                        if (M->TryGetObject(Obj))
                        {
                            FString Name;
                            if ((*Obj)->TryGetStringField(TEXT("name"), Name))
                            {
                                Names.Add(Name);
                            }
                        }
                    }
                }

                if (Names.IsEmpty())
                {
                    FMessageDialog::Open(EAppMsgType::Ok,
                        FText::FromString(TEXT("Ollama is running but has no models.\nRun: ollama pull <modelname>")));
                    return;
                }

                // Update static cache and rebuild the panel
                RebuildModelOptions(EUnrealMCPProvider::Ollama, Names);
                if (CachedDetailBuilder)
                {
                    CachedDetailBuilder->ForceRefreshDetails();
                }
            });
        Req->ProcessRequest();
    }
    else
    {
        // For cloud providers just refresh from hardcoded list
        const UUnrealMCPEditorSettings* S = GetDefault<UUnrealMCPEditorSettings>();
        if (S)
        {
            RebuildModelOptions(S->Provider, GetDefaultModelsForProvider(S->Provider));
            if (CachedDetailBuilder) CachedDetailBuilder->ForceRefreshDetails();
        }
    }

    return FReply::Handled();
}

FReply FUnrealMCPEditorSettingsCustomization::OnTestConnectionClicked()
{
    const UUnrealMCPEditorSettings* Settings = GetDefault<UUnrealMCPEditorSettings>();
    if (!Settings) return FReply::Handled();

    // ─── Ollama: test directly against the local REST API, no Python agent needed ───
    if (Settings->Provider == EUnrealMCPProvider::Ollama)
    {
        FString VersionUrl = Settings->OllamaServerUrl;
        if (!VersionUrl.EndsWith(TEXT("/"))) VersionUrl += TEXT("/");
        VersionUrl += TEXT("api/version");

        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
        Req->SetURL(VersionUrl);
        Req->SetVerb(TEXT("GET"));
        Req->OnProcessRequestComplete().BindLambda(
            [](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK)
            {
                if (bOK && Resp.IsValid() && Resp->GetResponseCode() == 200)
                {
                    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
                        FString::Printf(TEXT("✅ Ollama is reachable!\n\n%s"), *Resp->GetContentAsString())));
                }
                else
                {
                    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
                        TEXT("❌ Could not reach Ollama.\nCheck your Server URL in the settings above.")));
                }
            });
        Req->ProcessRequest();
        return FReply::Handled();
    }

    // ─── Cloud providers: need the Python agent — auto-start it first ───
    if (FEpicUnrealMCPModule::IsAvailable())
    {
        FEpicUnrealMCPModule::Get().EnsureAgentRunning();
    }

    const UEnum* ProviderEnum = StaticEnum<EUnrealMCPProvider>();
    FString ProviderStr = ProviderEnum
        ? ProviderEnum->GetDisplayNameTextByValue((int64)Settings->Provider).ToString()
        : TEXT("Anthropic");

    FString JsonBody = FString::Printf(
        TEXT("{\"provider\":\"%s\",\"api_key\":\"%s\",\"model\":\"%s\"}"),
        *ProviderStr, *Settings->ApiKey, *Settings->ModelName);

    // Delay 2 seconds to let the Python process boot before hitting it
    FTimerDelegate TimerDel;
    TimerDel.BindLambda([JsonBody]()
    {
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
                        ? FString::Printf(TEXT("HTTP %d — Python agent returned an error."), Resp.IsValid() ? Resp->GetResponseCode() : 0)
                        : TEXT("❌ Could not reach the Python agent after 2s.\nMake sure Python is installed and in your PATH.");
                }
                FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(ResultText));
            });
        Req->ProcessRequest();
    });

    if (GEditor)
    {
        GEditor->GetTimerManager()->SetTimer(
            TestConnectionTimerHandle, TimerDel, 2.0f, /*bLoop=*/false);
    }

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

