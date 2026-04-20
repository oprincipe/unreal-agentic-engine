#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

struct FChatMessage
{
    FString Role;    // "user" or "assistant"
    FString Content;
    FDateTime Timestamp;
};

class SUnrealMCPChatWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SUnrealMCPChatWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SUnrealMCPChatWidget() override;

private:
    // UI Actions
    FReply OnSendClicked();
    FReply OnNewChatClicked();
    FReply OnRestartAgentClicked();
    void OnInputTextCommitted(const FText& InText, ETextCommit::Type CommitMethod);

    // Drag and Drop
    virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
    virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

    // Chat Rendering
    TSharedRef<SWidget> BuildChatBubble(const FChatMessage& Message);
    void AddMessageToUI(const FChatMessage& Message);
    void ScrollToBottom();

    // Persistence (SQLite via FFileHelper)
    void LoadChatHistory();
    void SaveMessageToHistory(const FChatMessage& Message);
    void ClearCurrentSession();

    // Data
    TArray<FChatMessage> Messages;
    FString CurrentSessionId;

    // Widgets
    TSharedPtr<SScrollBox> ChatScrollBox;
    TSharedPtr<SMultiLineEditableTextBox> InputTextBox;
    TSharedPtr<SVerticalBox> ChatVBox;
    
    // UI State
    void ShowLoadingIndicator();
    void HideLoadingIndicator();
    TSharedPtr<SWidget> LoadingBubbleWidget;
};
