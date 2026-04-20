#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UNREALMCP_API FEpicUnrealMCPAssetMutatorCommands
{
public:
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMeshMaterialColor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDestroyActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleStartPlayInEditor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleStopPlayInEditor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetEditorLogs(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params);
};
