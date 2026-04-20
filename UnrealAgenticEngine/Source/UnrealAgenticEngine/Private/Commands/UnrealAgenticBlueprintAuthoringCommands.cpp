#include "Commands/UnrealAgenticBlueprintAuthoringCommands.h"
#include "Commands/UnrealAgenticCommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node.h"
#include "EdGraph/EdGraphPin.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
// New tool includes
#include "Engine/DataAsset.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Sound/SoundBase.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EngineUtils.h"
#include "UnrealAgenticModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Materials/MaterialInstanceConstant.h"

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintAuthoringCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_blueprint")) return HandleCreateBlueprint(Params);
    else if (CommandType == TEXT("compile_blueprint")) return HandleCompileBlueprint(Params);
    else if (CommandType == TEXT("add_component_to_blueprint")) return HandleAddComponentToBlueprint(Params);
    else if (CommandType == TEXT("add_blueprint_event_node")) return HandleAddBlueprintEventNode(Params);
    else if (CommandType == TEXT("add_blueprint_function_node")) return HandleAddBlueprintFunctionNode(Params);
    else if (CommandType == TEXT("add_blueprint_branch_node")) return HandleAddBlueprintBranchNode(Params);
    else if (CommandType == TEXT("create_blueprint_custom_event")) return HandleCreateBlueprintCustomEvent(Params);
    else if (CommandType == TEXT("connect_blueprint_nodes")) return HandleConnectBlueprintNodes(Params);
    else if (CommandType == TEXT("create_blueprint_variable")) return HandleCreateBlueprintVariable(Params);
    else if (CommandType == TEXT("delete_blueprint_node")) return HandleDeleteBlueprintNode(Params);
    else if (CommandType == TEXT("break_blueprint_link")) return HandleBreakBlueprintLink(Params);
    else if (CommandType == TEXT("delete_blueprint_component")) return HandleDeleteBlueprintComponent(Params);
    
    return nullptr;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintAuthoringCommands::HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/Blueprints/");
    FString AssetName = BlueprintName;
    
    if (BlueprintName.StartsWith(TEXT("/Game/")))
    {
        int32 LastSlash;
        if (BlueprintName.FindLastChar('/', LastSlash))
        {
            PackagePath = BlueprintName.Left(LastSlash + 1);
            AssetName = BlueprintName.RightChop(LastSlash + 1);
        }
    }
    else
    {
        int32 LastSlash;
        if (BlueprintName.FindLastChar('/', LastSlash))
        {
            PackagePath += BlueprintName.Left(LastSlash + 1);
            AssetName = BlueprintName.RightChop(LastSlash + 1);
        }
    }

    if (UEditorAssetLibrary::DoesAssetExist(PackagePath + AssetName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint already exists: %s"), *BlueprintName));
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();

    FString ParentClass;
    Params->TryGetStringField(TEXT("parent_class"), ParentClass);

    UClass* SelectedParentClass = AActor::StaticClass();

    if (!ParentClass.IsEmpty())
    {
        // Try exact matches first
        if (ParentClass == TEXT("Actor") || ParentClass == TEXT("AActor"))
        {
            SelectedParentClass = AActor::StaticClass();
        }
        else if (ParentClass == TEXT("Pawn") || ParentClass == TEXT("APawn"))
        {
            SelectedParentClass = APawn::StaticClass();
        }
        else if (ParentClass == TEXT("Character") || ParentClass == TEXT("ACharacter"))
        {
            SelectedParentClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.Character"));
        }
        else if (ParentClass == TEXT("AIController") || ParentClass == TEXT("AAIController"))
        {
            SelectedParentClass = LoadClass<AActor>(nullptr, TEXT("/Script/AIModule.AIController"));
        }
        else
        {
            // For general lookup, we should use the name WITHOUT the 'A' or 'U' prefix
            FString ClassNameNoPrefix = ParentClass;
            if (ParentClass.StartsWith(TEXT("A"), ESearchCase::IgnoreCase) || ParentClass.StartsWith(TEXT("U"), ESearchCase::IgnoreCase))
            {
                if (ParentClass.Len() > 1 && FChar::IsUpper(ParentClass[1])) 
                {
                    ClassNameNoPrefix = ParentClass.RightChop(1);
                }
            }

            UClass* FoundClass = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassNameNoPrefix));
            if (!FoundClass)
                FoundClass = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Script/Game.%s"), *ClassNameNoPrefix));
            if (!FoundClass)
                FoundClass = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Script/AIModule.%s"), *ClassNameNoPrefix));

            if (FoundClass)
            {
                SelectedParentClass = FoundClass;
            }
        }

        if (SelectedParentClass)
        {
            UE_LOG(LogTemp, Log, TEXT("Successfully set parent class to '%s'"), *ParentClass);
        }
        else
        {
            SelectedParentClass = AActor::StaticClass();
            UE_LOG(LogTemp, Warning, TEXT("Could not find specified parent class '%s', defaulting to AActor"), *ParentClass);
        }
    }

    Factory->ParentClass = SelectedParentClass;

    UPackage* Package = CreatePackage(*(PackagePath + AssetName));
    UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *AssetName, RF_Standalone | RF_Public, nullptr, GWarn));

    if (NewBlueprint)
    {
        FAssetRegistryModule::AssetCreated(NewBlueprint);
        if (!Package->MarkPackageDirty())
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to mark package '%s' as dirty"), *Package->GetName());
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("name"), AssetName);
        ResultObj->SetStringField(TEXT("path"), PackagePath + AssetName);
        return ResultObj;
    }

    return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to create blueprint"));
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintAuthoringCommands::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), BlueprintName);
    
    // Evaluate Compilation Status
    bool bSuccess = false;
    FString StatusMsg = TEXT("Unknown");
    
    if (Blueprint->Status == BS_UpToDate)
    {
        bSuccess = true;
        StatusMsg = TEXT("Success");
    }
    else if (Blueprint->Status == BS_UpToDateWithWarnings)
    {
        bSuccess = true;
        StatusMsg = TEXT("Success with Warnings");
    }
    else if (Blueprint->Status == BS_Error)
    {
        bSuccess = false;
        StatusMsg = TEXT("Error");
    }
    else
    {
        bSuccess = false;
        StatusMsg = TEXT("Unknown/Dirty");
    }

    ResultObj->SetBoolField(TEXT("compiled"), bSuccess);
    ResultObj->SetStringField(TEXT("status"), StatusMsg);
    ResultObj->SetBoolField(TEXT("success"), bSuccess);
    
    if (!bSuccess)
    {
        ResultObj->SetStringField(TEXT("message"), TEXT("Compilation failed. Check the Blueprint Message Log for detailed errors."));
    }
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintAuthoringCommands::HandleAddComponentToBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentType;
    if (!Params->TryGetStringField(TEXT("component_type"), ComponentType))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UClass* ComponentClass = nullptr;
    
    FString SearchName = ComponentType;
    if (SearchName.StartsWith(TEXT("U")))
    {
        SearchName = SearchName.Mid(1);
    }
    
    FString SearchNameWithComp = SearchName.EndsWith(TEXT("Component")) ? SearchName : SearchName + TEXT("Component");

    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Cls = *It;
        FString ClsName = Cls->GetName();
        
        if (ClsName == SearchName || ClsName == SearchNameWithComp)
        {
            if (Cls->IsChildOf(UActorComponent::StaticClass()))
            {
                ComponentClass = Cls;
                break;
            }
        }
    }

    if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown component type: %s"), *ComponentType));
    }

    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, *ComponentName);
    if (NewNode)
    {
        USceneComponent* SceneComponent = Cast<USceneComponent>(NewNode->ComponentTemplate);
        if (SceneComponent)
        {
            if (Params->HasField(TEXT("location")))
            {
                SceneComponent->SetRelativeLocation(FUnrealAgenticCommonUtils::GetVectorFromJson(Params, TEXT("location")));
            }
            if (Params->HasField(TEXT("rotation")))
            {
                SceneComponent->SetRelativeRotation(FUnrealAgenticCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")));
            }
            if (Params->HasField(TEXT("scale")))
            {
                SceneComponent->SetRelativeScale3D(FUnrealAgenticCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
            }
        }

        Blueprint->SimpleConstructionScript->AddNode(NewNode);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("component_name"), ComponentName);
        ResultObj->SetStringField(TEXT("component_type"), ComponentType);
        return ResultObj;
    }

    return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to add component to blueprint"));
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintAuthoringCommands::HandleAddBlueprintEventNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString EventType;
    if (!Params->TryGetStringField(TEXT("event_type"), EventType))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'event_type' parameter"));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealAgenticCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to find or create event graph"));
    }

    FVector2D NodePosition(0, 0);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealAgenticCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    const UK2Node_Event* EventNode;

    if (EventType == TEXT("BeginPlay"))
    {
        EventNode = FUnrealAgenticCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveBeginPlay"), NodePosition);
    }
    else if (EventType == TEXT("Tick"))
    {
        EventNode = FUnrealAgenticCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveTick"), NodePosition);
    }
    else if (EventType == TEXT("BeginOverlap"))
    {
        EventNode = FUnrealAgenticCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveActorBeginOverlap"), NodePosition);
    }
    else if (EventType == TEXT("EndOverlap"))
    {
        EventNode = FUnrealAgenticCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveActorEndOverlap"), NodePosition);
    }
    else if (EventType == TEXT("Hit"))
    {
        EventNode = FUnrealAgenticCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveHit"), NodePosition);
    }
    else if (EventType == TEXT("AnyDamage"))
    {
        EventNode = FUnrealAgenticCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveAnyDamage"), NodePosition);
    }
    else if (EventType == TEXT("Destroyed"))
    {
        EventNode = FUnrealAgenticCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveDestroyed"), NodePosition);
    }
    else if (EventType == TEXT("Custom"))
    {
        FString CustomEventName = TEXT("CustomEvent");
        Params->TryGetStringField(TEXT("custom_event_name"), CustomEventName);

        UK2Node_CustomEvent* CustomEventNode = NewObject<UK2Node_CustomEvent>(EventGraph);
        CustomEventNode->CustomFunctionName = *CustomEventName;
        CustomEventNode->NodePosX = NodePosition.X;
        CustomEventNode->NodePosY = NodePosition.Y;
        EventGraph->AddNode(CustomEventNode, true);
        CustomEventNode->PostPlacedNewNode();
        CustomEventNode->AllocateDefaultPins();
        CustomEventNode->ReconstructNode();

        EventNode = CustomEventNode;
    }
    else
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown event type: %s"), *EventType));
    }

    if (!EventNode)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to create event node"));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("event_type"), EventType);
    ResultObj->SetStringField(TEXT("node_id"), EventNode->NodeGuid.ToString());
    ResultObj->SetNumberField(TEXT("position_x"), NodePosition.X);
    ResultObj->SetNumberField(TEXT("position_y"), NodePosition.Y);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintAuthoringCommands::HandleAddBlueprintFunctionNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealAgenticCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to find or create event graph"));
    }

    FVector2D NodePosition(300, 0);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealAgenticCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    FString FunctionClass;
    Params->TryGetStringField(TEXT("function_class"), FunctionClass);

    UFunction* Function = nullptr;

    if (!FunctionClass.IsEmpty())
    {
        UClass* TargetClass = nullptr;
        FString SearchName = FunctionClass;
        if (SearchName.StartsWith(TEXT("U")) || SearchName.StartsWith(TEXT("A")))
        {
            SearchName = SearchName.Mid(1);
        }

        for (TObjectIterator<UClass> It; It; ++It)
        {
            if (It->GetName() == SearchName)
            {
                TargetClass = *It;
                break;
            }
        }

        if (TargetClass)
        {
            Function = TargetClass->FindFunctionByName(*FunctionName);
            
            // Fallback for function names without 'K2_' prefix if not found
            if (!Function && !FunctionName.StartsWith(TEXT("K2_")))
            {
                Function = TargetClass->FindFunctionByName(*(TEXT("K2_") + FunctionName));
            }
        }
    }
    else
    {
        if (Blueprint->ParentClass)
        {
            Function = Blueprint->ParentClass->FindFunctionByName(*FunctionName);
            
            if (!Function && !FunctionName.StartsWith(TEXT("K2_")))
            {
                Function = Blueprint->ParentClass->FindFunctionByName(*(TEXT("K2_") + FunctionName));
            }
        }
    }

    if (!Function)
    {
        const UClass* GameplayStatics = UGameplayStatics::StaticClass();
        Function = GameplayStatics->FindFunctionByName(*FunctionName);
    }

    if (!Function)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
    }

    const UK2Node_CallFunction* FunctionNode = FUnrealAgenticCommonUtils::CreateFunctionCallNode(EventGraph, Function, NodePosition);
    if (!FunctionNode)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to create function call node"));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("function_name"), FunctionName);
    ResultObj->SetStringField(TEXT("node_id"), FunctionNode->NodeGuid.ToString());
    ResultObj->SetNumberField(TEXT("position_x"), NodePosition.X);
    ResultObj->SetNumberField(TEXT("position_y"), NodePosition.Y);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintAuthoringCommands::HandleAddBlueprintBranchNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealAgenticCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to find or create event graph"));
    }

    FVector2D NodePosition(600, 0);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealAgenticCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(EventGraph);
    BranchNode->NodePosX = NodePosition.X;
    BranchNode->NodePosY = NodePosition.Y;
    EventGraph->AddNode(BranchNode, true);
    BranchNode->PostPlacedNewNode();
    BranchNode->AllocateDefaultPins();
    BranchNode->ReconstructNode();

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_type"), TEXT("Branch"));
    ResultObj->SetStringField(TEXT("node_id"), BranchNode->NodeGuid.ToString());
    ResultObj->SetNumberField(TEXT("position_x"), NodePosition.X);
    ResultObj->SetNumberField(TEXT("position_y"), NodePosition.Y);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintAuthoringCommands::HandleCreateBlueprintCustomEvent(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString EventName;
    if (!Params->TryGetStringField(TEXT("event_name"), EventName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealAgenticCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to find or create event graph"));
    }

    UK2Node_CustomEvent* CustomEventNode = NewObject<UK2Node_CustomEvent>(EventGraph);
    CustomEventNode->CustomFunctionName = *EventName;
    CustomEventNode->NodePosX = 0;
    CustomEventNode->NodePosY = 0;
    EventGraph->AddNode(CustomEventNode, true);
    CustomEventNode->PostPlacedNewNode();
    CustomEventNode->AllocateDefaultPins();

    if (Params->HasField(TEXT("input_params")))
    {
        const TArray<TSharedPtr<FJsonValue>>* ParamsArray;
        if (Params->TryGetArrayField(TEXT("input_params"), ParamsArray))
        {
            for (const TSharedPtr<FJsonValue>& ParamValue : *ParamsArray)
            {
                const TSharedPtr<FJsonObject>* ParamObj;
                if (ParamValue->TryGetObject(ParamObj))
                {
                    FString ParamName, ParamType;
                    if ((*ParamObj)->TryGetStringField(TEXT("name"), ParamName) &&
                        (*ParamObj)->TryGetStringField(TEXT("type"), ParamType))
                    {
                        FBPVariableDescription NewParam;
                        NewParam.VarName = *ParamName;
                        NewParam.VarGuid = FGuid::NewGuid();
                        NewParam.FriendlyName = ParamName;

                        if (ParamType == TEXT("bool"))
                        {
                            NewParam.VarType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
                        }
                        else if (ParamType == TEXT("int"))
                        {
                            NewParam.VarType.PinCategory = UEdGraphSchema_K2::PC_Int;
                        }
                        else if (ParamType == TEXT("float"))
                        {
                            NewParam.VarType.PinCategory = UEdGraphSchema_K2::PC_Real;
                            NewParam.VarType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
                        }
                        else if (ParamType == TEXT("string"))
                        {
                            NewParam.VarType.PinCategory = UEdGraphSchema_K2::PC_String;
                        }
                        else
                        {
                            NewParam.VarType.PinCategory = UEdGraphSchema_K2::PC_Object;
                            UClass* FoundClass = FindObject<UClass>(nullptr, *ParamType);
                            if (FoundClass)
                            {
                                NewParam.VarType.PinSubCategoryObject = FoundClass;
                            }
                        }

                        TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
                        NewPin->PinName = NewParam.VarName;
                        NewPin->PinType = NewParam.VarType;
                        CustomEventNode->UserDefinedPins.Add(NewPin);
                    }
                }
            }
        }
    }

    CustomEventNode->ReconstructNode();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("event_name"), EventName);
    ResultObj->SetStringField(TEXT("node_id"), CustomEventNode->NodeGuid.ToString());
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintAuthoringCommands::HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString SourceNodeId, SourcePin, TargetNodeId, TargetPin;
    if (!Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId) ||
        !Params->TryGetStringField(TEXT("source_pin"), SourcePin) ||
        !Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId) ||
        !Params->TryGetStringField(TEXT("target_pin"), TargetPin))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing connection parameters"));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealAgenticCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to find event graph"));
    }

    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;

    FGuid SourceGuid, TargetGuid;
    if (FGuid::Parse(SourceNodeId, SourceGuid) && FGuid::Parse(TargetNodeId, TargetGuid))
    {
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            if (Node->NodeGuid == SourceGuid)
            {
                SourceNode = Node;
            }
            if (Node->NodeGuid == TargetGuid)
            {
                TargetNode = Node;
            }
        }
    }

    if (!SourceNode || !TargetNode)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Could not find source or target nodes"));
    }

    const bool bConnected = FUnrealAgenticCommonUtils::ConnectGraphNodes(EventGraph, SourceNode, SourcePin, TargetNode, TargetPin);
    if (!bConnected)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to connect nodes"));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("source_node"), SourceNodeId);
    ResultObj->SetStringField(TEXT("target_node"), TargetNodeId);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintAuthoringCommands::HandleCreateBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    FString VariableType;
    if (!Params->TryGetStringField(TEXT("variable_type"), VariableType))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_type' parameter"));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    FEdGraphPinType PinType;

    if (VariableType == TEXT("bool"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    }
    else if (VariableType == TEXT("int"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    }
    else if (VariableType == TEXT("float"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    }
    else if (VariableType == TEXT("string"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    else if (VariableType == TEXT("vector"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else if (VariableType == TEXT("rotator"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
    }
    else if (VariableType == TEXT("transform"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
    }
    else
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        UClass* FoundClass = FindObject<UClass>(nullptr, *VariableType);
        if (FoundClass)
        {
            PinType.PinSubCategoryObject = FoundClass;
        }
    }

    FBPVariableDescription NewVariable;
    NewVariable.VarName = *VariableName;
    NewVariable.VarGuid = FGuid::NewGuid();
    NewVariable.FriendlyName = VariableName;
    NewVariable.Category = NSLOCTEXT("BlueprintEditor", "UserVariables", "Variables");
    NewVariable.PropertyFlags = CPF_Edit | CPF_BlueprintVisible;
    NewVariable.VarType = PinType;

    if (Params->HasField(TEXT("default_value")))
    {
        const TSharedPtr<FJsonValue> DefaultValue = Params->TryGetField(TEXT("default_value"));

        if (VariableType == TEXT("bool"))
        {
            NewVariable.DefaultValue = DefaultValue->AsBool() ? TEXT("true") : TEXT("false");
        }
        else if (VariableType == TEXT("int"))
        {
            NewVariable.DefaultValue = FString::FromInt(static_cast<int32>(DefaultValue->AsNumber()));
        }
        else if (VariableType == TEXT("float"))
        {
            NewVariable.DefaultValue = FString::SanitizeFloat(DefaultValue->AsNumber());
        }
        else if (VariableType == TEXT("string"))
        {
            NewVariable.DefaultValue = DefaultValue->AsString();
        }
        else if (VariableType == TEXT("vector"))
        {
            const TArray<TSharedPtr<FJsonValue>>* VectorArray;
            if (DefaultValue->TryGetArray(VectorArray) && VectorArray->Num() == 3)
            {
                float X = (*VectorArray)[0]->AsNumber();
                float Y = (*VectorArray)[1]->AsNumber();
                float Z = (*VectorArray)[2]->AsNumber();
                NewVariable.DefaultValue = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), X, Y, Z);
            }
        }
    }

    Blueprint->NewVariables.Add(NewVariable);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("variable_name"), VariableName);
    ResultObj->SetStringField(TEXT("variable_type"), VariableType);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintAuthoringCommands::HandleDeleteBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    FString NodeIdStr;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }
    
    if (!Params->TryGetStringField(TEXT("node_id"), NodeIdStr))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    FGuid NodeGuid;
    if (!FGuid::Parse(NodeIdStr, NodeGuid))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Invalid 'node_id' format. Must be a valid GUID."));
    }

    UEdGraphNode* NodeToDelete = nullptr;
    TArray<UEdGraph*> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);

    for (UEdGraph* Graph : AllGraphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node->NodeGuid == NodeGuid)
            {
                NodeToDelete = Node;
                break;
            }
        }
        if (NodeToDelete) break;
    }

    if (!NodeToDelete)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node %s not found in Blueprint."), *NodeIdStr));
    }

    // Safely break links and remove
    UEdGraph* Graph = NodeToDelete->GetGraph();
    const UEdGraphSchema* Schema = Graph->GetSchema();
    if (Schema)
    {
        Schema->BreakNodeLinks(*NodeToDelete);
    }
    else
    {
        NodeToDelete->BreakAllNodeLinks();
    }
    
    Graph->RemoveNode(NodeToDelete);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Node deleted successfully."));
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintAuthoringCommands::HandleBreakBlueprintLink(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    FString SourceNodeId, SourcePinName;
    FString TargetNodeId, TargetPinName;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) ||
        !Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId) ||
        !Params->TryGetStringField(TEXT("source_pin_name"), SourcePinName) ||
        !Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId) ||
        !Params->TryGetStringField(TEXT("target_pin_name"), TargetPinName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing required parameters for break_blueprint_link."));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraphPin* SourcePin = nullptr;
    UEdGraphPin* TargetPin = nullptr;
    
    FGuid SourceGuid, TargetGuid;
    FGuid::Parse(SourceNodeId, SourceGuid);
    FGuid::Parse(TargetNodeId, TargetGuid);

    TArray<UEdGraph*> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);

    for (UEdGraph* Graph : AllGraphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node->NodeGuid == SourceGuid)
            {
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin->PinName.ToString() == SourcePinName) SourcePin = Pin;
                }
            }
            if (Node->NodeGuid == TargetGuid)
            {
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin->PinName.ToString() == TargetPinName) TargetPin = Pin;
                }
            }
        }
    }

    if (!SourcePin || !TargetPin)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Pin not found!"));
    }

    SourcePin->BreakLinkTo(TargetPin);
    
    const UEdGraphSchema* Schema = SourcePin->GetSchema();
    if (Schema)
    {
        Schema->ReconstructNode(*SourcePin->GetOwningNode());
        Schema->ReconstructNode(*TargetPin->GetOwningNode());
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Link broken successfully."));
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintAuthoringCommands::HandleDeleteBlueprintComponent(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    FString ComponentName;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) ||
        !Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing parameters for delete_blueprint_component."));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Blueprint not found."));
    }

    if (!Blueprint->SimpleConstructionScript)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript."));
    }

    USCS_Node* NodeToRemove = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node->GetVariableName().ToString() == ComponentName)
        {
            NodeToRemove = Node;
            break;
        }
    }

    if (!NodeToRemove)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Component not found."));
    }

    Blueprint->SimpleConstructionScript->RemoveNode(NodeToRemove);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("message"), TEXT("Component deleted successfully."));
    return ResultObj;
}


