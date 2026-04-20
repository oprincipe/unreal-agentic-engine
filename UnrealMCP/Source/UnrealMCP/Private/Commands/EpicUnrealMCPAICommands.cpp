#include "Commands/EpicUnrealMCPAICommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

// Behavior Tree and Blackboard includes
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"

#if WITH_EDITOR
// These are needed for creating the assets
#include "BehaviorTreeFactory.h"
#include "BlackboardDataFactory.h"
// Graph includes
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#endif

TSharedPtr<FJsonObject> FEpicUnrealMCPAICommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_ai_asset"))
    {
        return HandleCreateAIAsset(Params);
    }
    else if (CommandType == TEXT("add_behavior_tree_node"))
    {
        return HandleAddBehaviorTreeNode(Params);
    }
    else if (CommandType == TEXT("connect_behavior_tree_nodes"))
    {
        return HandleConnectBehaviorTreeNodes(Params);
    }
    else if (CommandType == TEXT("layout_behavior_tree"))
    {
        return HandleLayoutBehaviorTree(Params);
    }
    else if (CommandType == TEXT("set_behavior_tree_blackboard"))
    {
        return HandleSetBehaviorTreeBlackboard(Params);
    }
    else if (CommandType == TEXT("add_blackboard_key"))
    {
        return HandleAddBlackboardKey(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAICommands::HandleCreateAIAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetName;
    if (!Params->TryGetStringField(TEXT("name"), AssetName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString AssetType;
    if (!Params->TryGetStringField(TEXT("type"), AssetType)) // "BehaviorTree" or "Blackboard"
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    FString PackagePath = TEXT("/Game/AI/");
    
    if (AssetName.StartsWith(TEXT("/Game/")))
    {
        int32 LastSlash;
        if (AssetName.FindLastChar('/', LastSlash))
        {
            PackagePath = AssetName.Left(LastSlash + 1);
            AssetName = AssetName.RightChop(LastSlash + 1);
        }
    }
    else
    {
        int32 LastSlash;
        if (AssetName.FindLastChar('/', LastSlash))
        {
            PackagePath += AssetName.Left(LastSlash + 1);
            AssetName = AssetName.RightChop(LastSlash + 1);
        }
    }

    if (UEditorAssetLibrary::DoesAssetExist(PackagePath + AssetName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset already exists: %s"), *AssetName));
    }

    UPackage* Package = CreatePackage(*(PackagePath + AssetName));
    UObject* NewAsset = nullptr;

#if WITH_EDITOR
    UFactory* Factory = nullptr;

    if (AssetType == TEXT("BehaviorTree") || AssetType == TEXT("UBehaviorTree"))
    {
        Factory = NewObject<UBehaviorTreeFactory>();
        NewAsset = Factory->FactoryCreateNew(UBehaviorTree::StaticClass(), Package, *AssetName, RF_Standalone | RF_Public, nullptr, GWarn);
    }
    else if (AssetType == TEXT("Blackboard") || AssetType == TEXT("BlackboardData") || AssetType == TEXT("UBlackboardData"))
    {
        Factory = NewObject<UBlackboardDataFactory>();
        NewAsset = Factory->FactoryCreateNew(UBlackboardData::StaticClass(), Package, *AssetName, RF_Standalone | RF_Public, nullptr, GWarn);
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported AI asset type: %s"), *AssetType));
    }
#endif

    if (NewAsset != nullptr)
    {
        FAssetRegistryModule::AssetCreated(NewAsset);
        Package->MarkPackageDirty();

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("name"), AssetName);
        ResultObj->SetStringField(TEXT("path"), PackagePath + AssetName);
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create AI asset."));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAICommands::HandleSetBehaviorTreeBlackboard(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString BehaviorTreePath;
    FString BlackboardPath;
    
    if (!Params->TryGetStringField(TEXT("behavior_tree"), BehaviorTreePath) ||
        !Params->TryGetStringField(TEXT("blackboard"), BlackboardPath))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPAICommands: Missing behavior_tree or blackboard path"));
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing behavior_tree or blackboard path"));
    }

    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPAICommands: Attempting to set Blackboard %s on BehaviorTree %s"), *BlackboardPath, *BehaviorTreePath);

    UBehaviorTree* BTAsset = Cast<UBehaviorTree>(UEditorAssetLibrary::LoadAsset(BehaviorTreePath));
    if (!BTAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPAICommands: Could not load BehaviorTree at %s"), *BehaviorTreePath);
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not load BehaviorTree"));
    }

    UBlackboardData* BBAsset = Cast<UBlackboardData>(UEditorAssetLibrary::LoadAsset(BlackboardPath));
    if (!BBAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPAICommands: Could not load Blackboard at %s"), *BlackboardPath);
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not load Blackboard"));
    }

    BTAsset->BlackboardAsset = BBAsset;
    UPackage* Package = BTAsset->GetOutermost();
    if (Package)
    {
        Package->MarkPackageDirty();
    }
    
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPAICommands: Successfully set Blackboard %s on BT %s"), *BlackboardPath, *BehaviorTreePath);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject);
    ResultJson->SetBoolField(TEXT("success"), true);
    return ResultJson;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor only feature"));
#endif
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAICommands::HandleAddBlackboardKey(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString BBPath, KeyName, KeyTypeStr;
    if (!Params->TryGetStringField(TEXT("blackboard"), BBPath) || 
        !Params->TryGetStringField(TEXT("key_name"), KeyName) || 
        !Params->TryGetStringField(TEXT("key_type"), KeyTypeStr))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPAICommands: Missing required parameters in HandleAddBlackboardKey"));
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameters: blackboard, key_name, key_type"));
    }

    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPAICommands: Attempting to add key '%s' of type '%s' to BB %s"), *KeyName, *KeyTypeStr, *BBPath);

    UBlackboardData* BB = Cast<UBlackboardData>(UEditorAssetLibrary::LoadAsset(BBPath));
    if (!BB)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPAICommands: Invalid Blackboard asset at %s"), *BBPath);
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid Blackboard asset"));
    }

    UBlackboardKeyType* NewKeyType = nullptr;
    
    if (KeyTypeStr.Equals(TEXT("Object"), ESearchCase::IgnoreCase))
    {
        UBlackboardKeyType_Object* ObjKey = BB->UpdatePersistentKey<UBlackboardKeyType_Object>(FName(*KeyName));
        if (ObjKey)
        {
            FString ClassPath;
            if (Params->TryGetStringField(TEXT("base_class"), ClassPath))
            {
                UClass* BaseClass = Cast<UClass>(UEditorAssetLibrary::LoadAsset(ClassPath));
                if (!BaseClass && ClassPath.EndsWith(TEXT("_C"))) BaseClass = LoadObject<UClass>(nullptr, *ClassPath);
                if (BaseClass) 
                {
                    ObjKey->BaseClass = BaseClass;
                    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPAICommands: Object BaseClass set to %s"), *ClassPath);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("EpicUnrealMCPAICommands: Failed to load Object BaseClass %s"), *ClassPath);
                }
            }
            else
            {
                ObjKey->BaseClass = UObject::StaticClass();
            }
        }
        NewKeyType = ObjKey;
    }
    else if (KeyTypeStr.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
    {
        NewKeyType = BB->UpdatePersistentKey<UBlackboardKeyType_Vector>(FName(*KeyName));
    }
    else if (KeyTypeStr.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
    {
        NewKeyType = BB->UpdatePersistentKey<UBlackboardKeyType_Float>(FName(*KeyName));
    }
    else if (KeyTypeStr.Equals(TEXT("Bool") )|| KeyTypeStr.Equals(TEXT("Boolean"), ESearchCase::IgnoreCase))
    {
        NewKeyType = BB->UpdatePersistentKey<UBlackboardKeyType_Bool>(FName(*KeyName));
    }
    else if (KeyTypeStr.Equals(TEXT("Class"), ESearchCase::IgnoreCase))
    {
        UBlackboardKeyType_Class* ClassKey = BB->UpdatePersistentKey<UBlackboardKeyType_Class>(FName(*KeyName));
        if (ClassKey)
        {
            FString ClassPath;
            if (Params->TryGetStringField(TEXT("base_class"), ClassPath))
            {
                UClass* BaseClass = Cast<UClass>(UEditorAssetLibrary::LoadAsset(ClassPath));
                if (!BaseClass && ClassPath.EndsWith(TEXT("_C"))) BaseClass = LoadObject<UClass>(nullptr, *ClassPath);
                if (BaseClass) ClassKey->BaseClass = BaseClass;
            }
        }
        NewKeyType = ClassKey;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPAICommands: Unsupported KeyType %s"), *KeyTypeStr);
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported KeyType: %s"), *KeyTypeStr));
    }
    
    // Broadcast changes to editor if possible
    BB->PostEditChange();
    
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPAICommands: Successfully added key %s to Blackboard %s"), *KeyName, *BBPath);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Key '%s' of type '%s' added to Blackboard"), *KeyName, *KeyTypeStr));
    return ResultObj;
#else
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Adding Blackboard keys is only supported in Editor builds."));
#endif
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAICommands::HandleAddBehaviorTreeNode(const TSharedPtr<FJsonObject>& Params)
{
    // Minimal placeholder
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("HandleAddBehaviorTreeNode not fully implemented yet"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAICommands::HandleConnectBehaviorTreeNodes(const TSharedPtr<FJsonObject>& Params)
{
    // Minimal placeholder
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("HandleConnectBehaviorTreeNodes not fully implemented yet"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAICommands::HandleLayoutBehaviorTree(const TSharedPtr<FJsonObject>& Params)
{
    // Minimal placeholder
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("HandleLayoutBehaviorTree not fully implemented yet"));
}
