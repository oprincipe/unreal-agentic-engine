#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UNREALMCP_API FEpicUnrealMCPBlueprintQueryCommands
{
public:
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleSearchBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadBlueprintFunctions(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadBlueprintMacros(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadBlueprintEnum(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadBlueprintStruct(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadDataAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadInputAction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadInputMappingContext(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadWidgetVariables(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadSaveGameBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadMaterial(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadSoundClass(const TSharedPtr<FJsonObject>& Params);
    
    TSharedPtr<FJsonObject> HandleReadBlueprintGraph(const TSharedPtr<FJsonObject>& Params);
};
