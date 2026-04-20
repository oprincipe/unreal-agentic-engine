#include "Commands/UnrealAgenticAICommands.h"
#include "Commands/UnrealAgenticCommonUtils.h"
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
#include "BehaviorTreeGraphNode_Task.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Service.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "ScopedTransaction.h"
#endif

TSharedPtr<FJsonObject> FUnrealAgenticAICommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
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
    else if (CommandType == TEXT("render_ai_ir_to_asset"))
    {
        return HandleRenderAIIrToAsset(Params);
    }

    return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealAgenticAICommands::HandleCreateAIAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetName;
    if (!Params->TryGetStringField(TEXT("name"), AssetName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString AssetType;
    if (!Params->TryGetStringField(TEXT("type"), AssetType)) // "BehaviorTree" or "Blackboard"
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
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
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset already exists: %s"), *AssetName));
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
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported AI asset type: %s"), *AssetType));
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

    return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to create AI asset."));
}

TSharedPtr<FJsonObject> FUnrealAgenticAICommands::HandleSetBehaviorTreeBlackboard(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString BehaviorTreePath;
    FString BlackboardPath;
    
    if (!Params->TryGetStringField(TEXT("behavior_tree"), BehaviorTreePath) ||
        !Params->TryGetStringField(TEXT("blackboard"), BlackboardPath))
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealAgenticAICommands: Missing behavior_tree or blackboard path"));
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing behavior_tree or blackboard path"));
    }

    UE_LOG(LogTemp, Display, TEXT("UnrealAgenticAICommands: Attempting to set Blackboard %s on BehaviorTree %s"), *BlackboardPath, *BehaviorTreePath);

    UBehaviorTree* BTAsset = Cast<UBehaviorTree>(UEditorAssetLibrary::LoadAsset(BehaviorTreePath));
    if (!BTAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealAgenticAICommands: Could not load BehaviorTree at %s"), *BehaviorTreePath);
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Could not load BehaviorTree"));
    }

    UBlackboardData* BBAsset = Cast<UBlackboardData>(UEditorAssetLibrary::LoadAsset(BlackboardPath));
    if (!BBAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealAgenticAICommands: Could not load Blackboard at %s"), *BlackboardPath);
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Could not load Blackboard"));
    }

    BTAsset->BlackboardAsset = BBAsset;
    UPackage* Package = BTAsset->GetOutermost();
    if (Package)
    {
        Package->MarkPackageDirty();
    }
    
    UE_LOG(LogTemp, Display, TEXT("UnrealAgenticAICommands: Successfully set Blackboard %s on BT %s"), *BlackboardPath, *BehaviorTreePath);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject);
    ResultJson->SetBoolField(TEXT("success"), true);
    return ResultJson;
#else
    return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Editor only feature"));
#endif
}

TSharedPtr<FJsonObject> FUnrealAgenticAICommands::HandleAddBlackboardKey(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString BBPath, KeyName, KeyTypeStr;
    if (!Params->TryGetStringField(TEXT("blackboard"), BBPath) || 
        !Params->TryGetStringField(TEXT("key_name"), KeyName) || 
        !Params->TryGetStringField(TEXT("key_type"), KeyTypeStr))
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealAgenticAICommands: Missing required parameters in HandleAddBlackboardKey"));
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing required parameters: blackboard, key_name, key_type"));
    }

    UE_LOG(LogTemp, Display, TEXT("UnrealAgenticAICommands: Attempting to add key '%s' of type '%s' to BB %s"), *KeyName, *KeyTypeStr, *BBPath);

    UBlackboardData* BB = Cast<UBlackboardData>(UEditorAssetLibrary::LoadAsset(BBPath));
    if (!BB)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealAgenticAICommands: Invalid Blackboard asset at %s"), *BBPath);
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Invalid Blackboard asset"));
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
                    UE_LOG(LogTemp, Display, TEXT("UnrealAgenticAICommands: Object BaseClass set to %s"), *ClassPath);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("UnrealAgenticAICommands: Failed to load Object BaseClass %s"), *ClassPath);
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
        UE_LOG(LogTemp, Error, TEXT("UnrealAgenticAICommands: Unsupported KeyType %s"), *KeyTypeStr);
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported KeyType: %s"), *KeyTypeStr));
    }
    
    // Broadcast changes to editor if possible
    BB->PostEditChange();
    
    UE_LOG(LogTemp, Display, TEXT("UnrealAgenticAICommands: Successfully added key %s to Blackboard %s"), *KeyName, *BBPath);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Key '%s' of type '%s' added to Blackboard"), *KeyName, *KeyTypeStr));
    return ResultObj;
#else
    return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Adding Blackboard keys is only supported in Editor builds."));
#endif
}

TSharedPtr<FJsonObject> FUnrealAgenticAICommands::HandleAddBehaviorTreeNode(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString BTPath, NodeTypeStr, TaskClassStr, ParentNodeStr;
    if (!Params->TryGetStringField(TEXT("behavior_tree"), BTPath) ||
        !Params->TryGetStringField(TEXT("node_type"), NodeTypeStr))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing behavior_tree or node_type"));
    }

    Params->TryGetStringField(TEXT("task_class"), TaskClassStr);
    Params->TryGetStringField(TEXT("parent_node"), ParentNodeStr);

    auto FindAIAsset = [](const FString& AssetPath, UClass* AssetClass) -> UObject* {
        UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
        if (!Asset && !AssetPath.Contains(TEXT("/"))) Asset = FindObject<UObject>(nullptr, *AssetPath);
        if (!Asset)
        {
            FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
            TArray<FAssetData> AssetDataList;
            AssetRegistryModule.Get().GetAssetsByClass(AssetClass->GetClassPathName(), AssetDataList);
            for (const FAssetData& Data : AssetDataList)
            {
                if (Data.AssetName.ToString() == AssetPath || Data.ObjectPath.ToString().Contains(AssetPath))
                {
                    return Data.GetAsset();
                }
            }
        }
        return Asset;
    };

    UBehaviorTree* BTAsset = Cast<UBehaviorTree>(FindAIAsset(BTPath, UBehaviorTree::StaticClass()));
    if (!BTAsset) return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Invalid BehaviorTree"));

    UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(BTAsset->BTGraph);
    if (!BTGraph) return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("BTAsset has no valid BTGraph"));

    UClass* NodeClass = nullptr;
    if (NodeTypeStr.Contains(TEXT("Task"))) NodeClass = UBehaviorTreeGraphNode_Task::StaticClass();
    else if (NodeTypeStr.Contains(TEXT("Decorator"))) NodeClass = UBehaviorTreeGraphNode_Decorator::StaticClass();
    else if (NodeTypeStr.Contains(TEXT("Service"))) NodeClass = UBehaviorTreeGraphNode_Service::StaticClass();
    else if (NodeTypeStr.Contains(TEXT("Sequence")) || NodeTypeStr.Contains(TEXT("Selector")) || NodeTypeStr.Contains(TEXT("Composite"))) NodeClass = UBehaviorTreeGraphNode_Composite::StaticClass();
    else if (NodeTypeStr.Contains(TEXT("Root"))) NodeClass = UBehaviorTreeGraphNode_Root::StaticClass();
    else NodeClass = UBehaviorTreeGraphNode_Task::StaticClass();

    FGraphNodeCreator<UBehaviorTreeGraphNode> NodeCreator(*BTGraph);
    UBehaviorTreeGraphNode* NewNode = NodeCreator.CreateNode(true, NodeClass);
    if (!NewNode) return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to create BTGraphNode"));

    if (!TaskClassStr.IsEmpty())
    {
        UClass* InternalClass = nullptr;

        // Strip ".uasset" or "_C" if the AI added it incorrectly for string matching
        FString CleanTaskClass = TaskClassStr.Replace(TEXT(".uasset"), TEXT("")).Replace(TEXT("_C"), TEXT(""));
        
        // 1. Direct path load (Blueprint)
        if (TaskClassStr.Contains(TEXT("/")))
        {
            FString BPPath = TaskClassStr;
            if (!BPPath.EndsWith(TEXT("_C"))) BPPath += TEXT("_C");
            InternalClass = Cast<UClass>(UEditorAssetLibrary::LoadAsset(BPPath));
            if (!InternalClass) InternalClass = LoadObject<UClass>(nullptr, *BPPath);
        }

        // 2. Iterate all loaded classes (Native & Blueprint)
        if (!InternalClass)
        {
            for (TObjectIterator<UClass> It; It; ++It)
            {
                FString ClassName = It->GetName();
                // Match exact, or with U prefix, or if ClassName ends with short name (BTTask_MoveTo ends with MoveTo)
                if (ClassName.Equals(CleanTaskClass, ESearchCase::IgnoreCase) || 
                   (TEXT("U") + ClassName).Equals(CleanTaskClass, ESearchCase::IgnoreCase) ||
                   ClassName.EndsWith(CleanTaskClass, ESearchCase::IgnoreCase))
                {
                    InternalClass = *It;
                    // If it's a blueprint generated class, prefer it
                    if (InternalClass->IsChildOf(UBlueprintGeneratedClass::StaticClass()) && ClassName.Equals(CleanTaskClass + TEXT("_C"), ESearchCase::IgnoreCase))
                        break;
                    
                    // If it's a native BT node, prefer it
                    if (InternalClass->IsChildOf(UBTNode::StaticClass()))
                        break;
                }
            }
        }

        if (InternalClass)
        {
            UE_LOG(LogTemp, Display, TEXT("UnrealAgenticAICommands: Resolved BT class %s"), *InternalClass->GetName());
            
            // For instances in AIGraph, the NodeInstance expects an initialized UObject.
            NewNode->NodeInstance = NewObject<UObject>(NewNode, InternalClass);
            UAIGraphNode::UpdateNodeClassDataFrom(InternalClass, NewNode->ClassData);
            NewNode->UpdateNodeClassData();
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("UnrealAgenticAICommands: Failed to resolve BT class for %s"), *TaskClassStr);
        }
    }

    NodeCreator.Finalize();
    
    // Attempt connection if parent provided
    if (!ParentNodeStr.IsEmpty())
    {
        UBehaviorTreeGraphNode* ParentNode = nullptr;
        for (UEdGraphNode* GraphNode : BTGraph->Nodes)
        {
            if (GraphNode->NodeGuid.ToString() == ParentNodeStr ||
               (ParentNodeStr.Equals(TEXT("Root"), ESearchCase::IgnoreCase) && GraphNode->IsA(UBehaviorTreeGraphNode_Root::StaticClass())))
            {
                ParentNode = Cast<UBehaviorTreeGraphNode>(GraphNode);
                break;
            }
        }
        
        UEdGraphPin* OutputPin = nullptr;
        if (ParentNode)
        {
            for (UEdGraphPin* Pin : ParentNode->Pins)
                if (Pin->Direction == EGPD_Output) { OutputPin = Pin; break; }
        }
        
        UEdGraphPin* InputPin = nullptr;
        if (NewNode)
        {
            for (UEdGraphPin* Pin : NewNode->Pins)
                if (Pin->Direction == EGPD_Input) { InputPin = Pin; break; }
        }

        if (OutputPin && InputPin)
        {
            OutputPin->MakeLinkTo(InputPin);
            ParentNode->NodeConnectionListChanged();
            NewNode->NodeConnectionListChanged();
        }
    }

    BTGraph->UpdateAsset();
    UPackage* Package = BTAsset->GetOutermost();
    if (Package) Package->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("node_id"), NewNode->NodeGuid.ToString());
    return ResultObj;
#else
    return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Editor only feature"));
#endif
}

TSharedPtr<FJsonObject> FUnrealAgenticAICommands::HandleConnectBehaviorTreeNodes(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString BTPath, SourceNodeStr, TargetNodeStr;
    if (!Params->TryGetStringField(TEXT("behavior_tree"), BTPath) ||
        !Params->TryGetStringField(TEXT("source_node"), SourceNodeStr) ||
        !Params->TryGetStringField(TEXT("target_node"), TargetNodeStr))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing parameters"));
    }

    auto FindAIAsset = [](const FString& AssetPath, UClass* AssetClass) -> UObject* {
        UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
        if (!Asset && !AssetPath.Contains(TEXT("/"))) Asset = FindObject<UObject>(nullptr, *AssetPath);
        if (!Asset)
        {
            FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
            TArray<FAssetData> AssetDataList;
            AssetRegistryModule.Get().GetAssetsByClass(AssetClass->GetClassPathName(), AssetDataList);
            for (const FAssetData& Data : AssetDataList)
            {
                if (Data.AssetName.ToString() == AssetPath || Data.ObjectPath.ToString().Contains(AssetPath))
                {
                    return Data.GetAsset();
                }
            }
        }
        return Asset;
    };

    UBehaviorTree* BTAsset = Cast<UBehaviorTree>(FindAIAsset(BTPath, UBehaviorTree::StaticClass()));
    if (!BTAsset) return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Invalid BehaviorTree"));
    UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(BTAsset->BTGraph);
    if (!BTGraph) return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("No BTGraph"));

    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;

    for (UEdGraphNode* Node : BTGraph->Nodes)
    {
        if (Node->NodeGuid.ToString() == SourceNodeStr || 
           (SourceNodeStr.Equals(TEXT("Root"), ESearchCase::IgnoreCase) && Node->IsA(UBehaviorTreeGraphNode_Root::StaticClass())))
        {
            SourceNode = Node;
        }
        
        if (Node->NodeGuid.ToString() == TargetNodeStr ||
           (TargetNodeStr.Equals(TEXT("Root"), ESearchCase::IgnoreCase) && Node->IsA(UBehaviorTreeGraphNode_Root::StaticClass())))
        {
            TargetNode = Node;
        }
    }

    if (!SourceNode || !TargetNode) return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Nodes not found"));

    UEdGraphPin* OutputPin = nullptr;
    for (UEdGraphPin* Pin : SourceNode->Pins)
        if (Pin->Direction == EGPD_Output) { OutputPin = Pin; break; }
        
    UEdGraphPin* InputPin = nullptr;
    for (UEdGraphPin* Pin : TargetNode->Pins)
        if (Pin->Direction == EGPD_Input) { InputPin = Pin; break; }

    if (OutputPin && InputPin)
    {
        OutputPin->MakeLinkTo(InputPin);
        SourceNode->NodeConnectionListChanged();
        TargetNode->NodeConnectionListChanged();
        
        BTGraph->UpdateAsset();
        UPackage* Package = BTAsset->GetOutermost();
        if (Package) Package->MarkPackageDirty();
        
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        return ResultObj;
    }
    return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to connect pins"));
#else
    return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Editor only feature"));
#endif
}

TSharedPtr<FJsonObject> FUnrealAgenticAICommands::HandleLayoutBehaviorTree(const TSharedPtr<FJsonObject>& Params)
{
    // Minimal placeholder
    return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("HandleLayoutBehaviorTree not fully implemented yet"));
}

TSharedPtr<FJsonObject> FUnrealAgenticAICommands::HandleRenderAIIrToAsset(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    const TSharedPtr<FJsonObject>* IrDataObj;
    if (!Params->TryGetObjectField(TEXT("ir_data"), IrDataObj))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'ir_data' parameter object"));
    }

    TSharedPtr<FJsonObject> IR = *IrDataObj;

    FString AssetPath, BlackboardPath;
    if (!IR->TryGetStringField(TEXT("asset_path"), AssetPath) || !IR->TryGetStringField(TEXT("blackboard_path"), BlackboardPath))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("IR missing asset_path or blackboard_path"));
    }

    // Wrap the entire parsing and asset generation in a transaction
    FScopedTransaction Transaction(FText::FromString(TEXT("AI Render IR to Asset")));

    // 1. Create BT and BB Assets
    TSharedPtr<FJsonObject> CreateBTParams = MakeShared<FJsonObject>();
    CreateBTParams->SetStringField(TEXT("name"), AssetPath);
    CreateBTParams->SetStringField(TEXT("type"), TEXT("BehaviorTree"));
    TSharedPtr<FJsonObject> BTResult = HandleCreateAIAsset(CreateBTParams);
    
    TSharedPtr<FJsonObject> CreateBBParams = MakeShared<FJsonObject>();
    CreateBBParams->SetStringField(TEXT("name"), BlackboardPath);
    CreateBBParams->SetStringField(TEXT("type"), TEXT("Blackboard"));
    TSharedPtr<FJsonObject> BBResult = HandleCreateAIAsset(CreateBBParams);

    // 2. Link BT and BB
    TSharedPtr<FJsonObject> LinkParams = MakeShared<FJsonObject>();
    LinkParams->SetStringField(TEXT("behavior_tree"), AssetPath);
    LinkParams->SetStringField(TEXT("blackboard"), BlackboardPath);
    HandleSetBehaviorTreeBlackboard(LinkParams);

    // 3. Process Blackboard Keys
    const TArray<TSharedPtr<FJsonValue>>* KeysArray;
    if (IR->TryGetArrayField(TEXT("blackboard_keys"), KeysArray))
    {
        for (auto& KeyVal : *KeysArray)
        {
            TSharedPtr<FJsonObject> KeyObj = KeyVal->AsObject();
            if (KeyObj.IsValid())
            {
                TSharedPtr<FJsonObject> KeyParams = MakeShared<FJsonObject>();
                KeyParams->SetStringField(TEXT("blackboard"), BlackboardPath);
                
                FString KeyName, KeyType, BaseClass;
                if (KeyObj->TryGetStringField(TEXT("name"), KeyName)) KeyParams->SetStringField(TEXT("key_name"), KeyName);
                if (KeyObj->TryGetStringField(TEXT("type"), KeyType)) KeyParams->SetStringField(TEXT("key_type"), KeyType);
                if (KeyObj->TryGetStringField(TEXT("base_class"), BaseClass)) KeyParams->SetStringField(TEXT("base_class"), BaseClass);
                
                HandleAddBlackboardKey(KeyParams);
            }
        }
    }

    // 4. Process Tree Nodes
    UBehaviorTree* BTAsset = Cast<UBehaviorTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (BTAsset)
    {
        UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(BTAsset->BTGraph);
        if (BTGraph)
        {
            UBehaviorTreeGraphNode* ExistingRoot = nullptr;
            for (UEdGraphNode* GraphNode : BTGraph->Nodes)
            {
                if (GraphNode->IsA(UBehaviorTreeGraphNode_Root::StaticClass()))
                {
                    ExistingRoot = Cast<UBehaviorTreeGraphNode>(GraphNode);
                    break;
                }
            }

            const TSharedPtr<FJsonObject>* TreeRootObj;
            if (ExistingRoot && IR->TryGetObjectField(TEXT("tree_root"), TreeRootObj))
            {
                const TSharedPtr<FJsonObject>* ChildObj;
                if ((*TreeRootObj)->TryGetObjectField(TEXT("child"), ChildObj))
                {
                    ProcessIRNode(*ChildObj, BTGraph, ExistingRoot);
                }
            }
            
            BTGraph->UpdateAsset();
            BTAsset->GetOutermost()->MarkPackageDirty();
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Successfully rendered IR. BT: %s, BB: %s"), *AssetPath, *BlackboardPath));
    return ResultObj;
#else
    return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Editor only feature"));
#endif
}

class UBehaviorTreeGraphNode* FUnrealAgenticAICommands::ProcessIRNode(const TSharedPtr<FJsonObject>& NodeJson, class UBehaviorTreeGraph* BTGraph, class UBehaviorTreeGraphNode* ParentNode)
{
#if WITH_EDITOR
    if (!NodeJson.IsValid() || !BTGraph) return nullptr;

    FString NodeTypeStr;
    if (!NodeJson->TryGetStringField(TEXT("type"), NodeTypeStr)) return nullptr;

    UClass* NodeClass = nullptr;
    if (NodeTypeStr.Contains(TEXT("Task"))) NodeClass = UBehaviorTreeGraphNode_Task::StaticClass();
    else if (NodeTypeStr.Contains(TEXT("Sequence")) || NodeTypeStr.Contains(TEXT("Selector")) || NodeTypeStr.Contains(TEXT("SimpleParallel"))) NodeClass = UBehaviorTreeGraphNode_Composite::StaticClass();
    else NodeClass = UBehaviorTreeGraphNode_Task::StaticClass(); // fallback

    FGraphNodeCreator<UBehaviorTreeGraphNode> NodeCreator(*BTGraph);
    UBehaviorTreeGraphNode* NewNode = NodeCreator.CreateNode(true, NodeClass);
    if (!NewNode) return nullptr;

    FString TaskClassStr;
    NodeJson->TryGetStringField(TEXT("task_class"), TaskClassStr);
    
    if (NodeTypeStr.Contains(TEXT("Sequence"))) TaskClassStr = TEXT("BTComposite_Sequence");
    if (NodeTypeStr.Contains(TEXT("Selector"))) TaskClassStr = TEXT("BTComposite_Selector");
    if (NodeTypeStr.Contains(TEXT("SimpleParallel"))) TaskClassStr = TEXT("BTComposite_SimpleParallel");

    // Resolve internal class
    if (!TaskClassStr.IsEmpty())
    {
        UClass* InternalClass = nullptr;
        FString CleanTaskClass = TaskClassStr.Replace(TEXT(".uasset"), TEXT("")).Replace(TEXT("_C"), TEXT(""));
        
        if (TaskClassStr.Contains(TEXT("/")))
        {
            FString BPPath = TaskClassStr;
            if (!BPPath.EndsWith(TEXT("_C"))) BPPath += TEXT("_C");
            InternalClass = Cast<UClass>(UEditorAssetLibrary::LoadAsset(BPPath));
            if (!InternalClass) InternalClass = LoadObject<UClass>(nullptr, *BPPath);
        }

        if (!InternalClass)
        {
            for (TObjectIterator<UClass> It; It; ++It)
            {
                FString ClassName = It->GetName();
                if (ClassName.Equals(CleanTaskClass, ESearchCase::IgnoreCase) || 
                   (TEXT("U") + ClassName).Equals(CleanTaskClass, ESearchCase::IgnoreCase) ||
                   ClassName.EndsWith(CleanTaskClass, ESearchCase::IgnoreCase))
                {
                    InternalClass = *It;
                    if (InternalClass->IsChildOf(UBlueprintGeneratedClass::StaticClass()) && ClassName.Equals(CleanTaskClass + TEXT("_C"), ESearchCase::IgnoreCase)) break;
                    if (InternalClass->IsChildOf(UBTNode::StaticClass())) break;
                }
            }
        }

        if (InternalClass)
        {
            NewNode->NodeInstance = NewObject<UObject>(NewNode, InternalClass);
            UAIGraphNode::UpdateNodeClassDataFrom(InternalClass, NewNode->ClassData);
            NewNode->UpdateNodeClassData();
            
            // Set properties if any
            const TSharedPtr<FJsonObject>* PropsObj;
            if (NodeJson->TryGetObjectField(TEXT("properties"), PropsObj))
            {
                // Property reflection injection (advanced)
            }
        }
    }

    NodeCreator.Finalize();

    if (ParentNode)
    {
        UEdGraphPin* OutputPin = nullptr;
        for (UEdGraphPin* Pin : ParentNode->Pins)
            if (Pin->Direction == EGPD_Output) { OutputPin = Pin; break; }

        UEdGraphPin* InputPin = nullptr;
        for (UEdGraphPin* Pin : NewNode->Pins)
            if (Pin->Direction == EGPD_Input) { InputPin = Pin; break; }

        if (OutputPin && InputPin)
        {
            OutputPin->MakeLinkTo(InputPin);
            ParentNode->NodeConnectionListChanged();
            NewNode->NodeConnectionListChanged();
        }
    }

    // Process Children
    if (NodeClass == UBehaviorTreeGraphNode_Composite::StaticClass())
    {
        const TArray<TSharedPtr<FJsonValue>>* ChildrenArray;
        if (NodeJson->TryGetArrayField(TEXT("children"), ChildrenArray))
        {
            for (auto& ChildVal : *ChildrenArray)
            {
                TSharedPtr<FJsonObject> ChildObj = ChildVal->AsObject();
                if (ChildObj.IsValid())
                {
                    ProcessIRNode(ChildObj, BTGraph, NewNode);
                }
            }
        }
    }
    
    // Automatic Graph Formatting
    // Simple basic layout offsets
    static int NodeX = 0;
    static int NodeY = 0;
    NewNode->NodePosX = NodeX;
    NewNode->NodePosY = NodeY;
    NodeX += 200;
    NodeY += 150;

    return NewNode;
#else
    return nullptr;
#endif
}
