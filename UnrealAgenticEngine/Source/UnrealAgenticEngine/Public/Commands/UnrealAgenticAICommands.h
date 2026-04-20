#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UNREALAGENTICENGINE_API FUnrealAgenticAICommands
{
public:
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleCreateAIAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetBehaviorTreeBlackboard(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlackboardKey(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBehaviorTreeNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectBehaviorTreeNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleLayoutBehaviorTree(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRenderAIIrToAsset(const TSharedPtr<FJsonObject>& Params);
};
