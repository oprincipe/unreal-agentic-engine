#include "Commands/UnrealAgenticBlueprintQueryCommands.h"
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

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("search_blueprint_nodes")) return HandleSearchBlueprintNodes(Params);
    else if (CommandType == TEXT("read_blueprint_functions")) return HandleReadBlueprintFunctions(Params);
    else if (CommandType == TEXT("read_blueprint_macros")) return HandleReadBlueprintMacros(Params);
    else if (CommandType == TEXT("read_blueprint_enum")) return HandleReadBlueprintEnum(Params);
    else if (CommandType == TEXT("read_blueprint_struct")) return HandleReadBlueprintStruct(Params);
    else if (CommandType == TEXT("read_data_asset")) return HandleReadDataAsset(Params);
    else if (CommandType == TEXT("read_input_action")) return HandleReadInputAction(Params);
    else if (CommandType == TEXT("read_input_mapping_context")) return HandleReadInputMappingContext(Params);
    else if (CommandType == TEXT("read_widget_variables")) return HandleReadWidgetVariables(Params);
    else if (CommandType == TEXT("read_save_game_blueprint")) return HandleReadSaveGameBlueprint(Params);
    else if (CommandType == TEXT("read_material")) return HandleReadMaterial(Params);
    else if (CommandType == TEXT("read_sound_class")) return HandleReadSoundClass(Params);
    else if (CommandType == TEXT("read_blueprint_graph")) return HandleReadBlueprintGraph(Params);
    
    return nullptr;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleSearchBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString SearchTerm;
    if (!Params->TryGetStringField(TEXT("search_term"), SearchTerm))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'search_term' parameter"));
    }

    FString TargetClassStr;
    Params->TryGetStringField(TEXT("target_class"), TargetClassStr);

    UClass* TargetClass = nullptr;
    if (!TargetClassStr.IsEmpty())
    {
        FString SearchName = TargetClassStr;
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

        if (!TargetClass)
        {
            return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target class not found: %s"), *TargetClassStr));
        }
    }

    TArray<TSharedPtr<FJsonValue>> MatchesArray;
    int32 MatchCount = 0;
    const int32 MaxMatches = 30;

    auto ProcessFunction = [&](UFunction* Function)
    {
        if (MatchCount >= MaxMatches) return;

        if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_BlueprintEvent))
        {
            return;
        }

        FString FuncName = Function->GetName();
        if (!FuncName.Contains(SearchTerm, ESearchCase::IgnoreCase))
        {
            return;
        }

        TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
        FuncObj->SetStringField(TEXT("function_name"), FuncName);
        
        if (UClass* OuterClass = Function->GetOwnerClass())
        {
            FuncObj->SetStringField(TEXT("class_name"), OuterClass->GetName());
        }
        FuncObj->SetStringField(TEXT("tooltip"), Function->GetToolTipText().ToString());

        TArray<TSharedPtr<FJsonValue>> InputsArray;
        TArray<TSharedPtr<FJsonValue>> OutputsArray;

        for (TFieldIterator<FProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
        {
            FProperty* Param = *PropIt;
            FString ParamName = Param->GetName();
            FString ParamType = Param->GetCPPType();

            if (Param->IsA<FObjectProperty>())
            {
                FObjectProperty* ObjProp = CastField<FObjectProperty>(Param);
                if (ObjProp && ObjProp->PropertyClass)
                {
                    ParamType = ObjProp->PropertyClass->GetName();
                }
            }

            TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
            PinObj->SetStringField(TEXT("name"), ParamName);
            PinObj->SetStringField(TEXT("type"), ParamType);
            PinObj->SetStringField(TEXT("tooltip"), Param->GetToolTipText().ToString());

            bool bIsOutput = Param->HasAnyPropertyFlags(CPF_ReturnParm) || (Param->HasAnyPropertyFlags(CPF_OutParm) && !Param->HasAnyPropertyFlags(CPF_ConstParm));
            
            if (bIsOutput)
            {
                OutputsArray.Add(MakeShared<FJsonValueObject>(PinObj));
            }
            else
            {
                InputsArray.Add(MakeShared<FJsonValueObject>(PinObj));
            }
        }

        FuncObj->SetArrayField(TEXT("input_pins"), InputsArray);
        FuncObj->SetArrayField(TEXT("output_pins"), OutputsArray);

        MatchesArray.Add(MakeShared<FJsonValueObject>(FuncObj));
        MatchCount++;
    };

    if (TargetClass)
    {
        for (TFieldIterator<UFunction> FunIt(TargetClass, EFieldIteratorFlags::IncludeSuper); FunIt; ++FunIt)
        {
            ProcessFunction(*FunIt);
            if (MatchCount >= MaxMatches) break;
        }
    }
    else
    {
        for (TObjectIterator<UFunction> It; It; ++It)
        {
            ProcessFunction(*It);
            if (MatchCount >= MaxMatches) break;
        }
    }
    // Phase 2: Process UK2Node subclasses (Macro/Custom Blueprint Nodes)
    UPackage* TransientPackage = GetTransientPackage();
    
    auto ProcessK2NodeClass = [&](UClass* K2Class)
    {
        if (MatchCount >= MaxMatches) return;
        
        if (K2Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
            return;

        FString ClassName = K2Class->GetName();
        if (!ClassName.Contains(SearchTerm, ESearchCase::IgnoreCase))
            return;

        UEdGraph* DummyGraph = NewObject<UEdGraph>(TransientPackage);
        UK2Node* DummyNode = NewObject<UK2Node>(DummyGraph, K2Class);
        if (!DummyNode) return;

        // Required to prevent crashes on some node types when allocating pins
        DummyNode->CreateNewGuid();
        DummyNode->AllocateDefaultPins();

        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetStringField(TEXT("function_name"), ClassName);
        NodeObj->SetStringField(TEXT("class_name"), TEXT("UK2Node (Macro/Special Node)"));

        TArray<TSharedPtr<FJsonValue>> InputsArray;
        TArray<TSharedPtr<FJsonValue>> OutputsArray;

        for (UEdGraphPin* Pin : DummyNode->Pins)
        {
            if (!Pin) continue;

            TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
            PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
            PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
            PinObj->SetStringField(TEXT("tooltip"), Pin->PinToolTip);

            if (Pin->Direction == EGPD_Output)
            {
                OutputsArray.Add(MakeShared<FJsonValueObject>(PinObj));
            }
            else
            {
                InputsArray.Add(MakeShared<FJsonValueObject>(PinObj));
            }
        }

        NodeObj->SetArrayField(TEXT("input_pins"), InputsArray);
        NodeObj->SetArrayField(TEXT("output_pins"), OutputsArray);

        MatchesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
        MatchCount++;
    };

    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (It->IsChildOf(UK2Node::StaticClass()))
        {
            ProcessK2NodeClass(*It);
            if (MatchCount >= MaxMatches) break;
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("matches"), MatchesArray);
    ResultObj->SetNumberField(TEXT("count"), MatchesArray.Num());
    ResultObj->SetBoolField(TEXT("success"), true);
    
    if (MatchCount >= MaxMatches)
    {
        ResultObj->SetStringField(TEXT("warning"), TEXT("Search results were truncated to 30 matches. Please use target_class or a more specific search_term to narrow your results."));
    }

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleReadBlueprintFunctions(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    const UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintName));
    if (!Blueprint)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load Blueprint at path: %s"), *BlueprintName));
    }

    const UClass* GenClass = Blueprint->GeneratedClass;
    if (!GenClass)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Blueprint does not have a GeneratedClass (might need compilation)"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());

    TArray<TSharedPtr<FJsonValue>> FunctionsArray;

    for (TFieldIterator<UFunction> FuncIt(GenClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
    {
        const UFunction* Function = *FuncIt;
        FString FuncName = Function->GetName();

        // Skip internal/compiler-generated functions
        if (FuncName.StartsWith(TEXT("ExecuteUbergraph_")) || FuncName.StartsWith(TEXT("BndEvt__")) || FuncName.StartsWith(TEXT("UserConstructionScript")))
        {
            continue;
        }

        TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
        FuncObj->SetStringField(TEXT("name"), FuncName);

        TArray<TSharedPtr<FJsonValue>> InputsArray;
        TArray<TSharedPtr<FJsonValue>> OutputsArray;

        for (TFieldIterator<FProperty> PropIt(Function); PropIt; ++PropIt)
        {
            const FProperty* Param = *PropIt;
            if (!Param->HasAnyPropertyFlags(CPF_Parm)) continue;

            const bool bIsReturn = Param->HasAnyPropertyFlags(CPF_ReturnParm);
            const bool bIsOut = Param->HasAnyPropertyFlags(CPF_OutParm);
            
            // If it's an out param, it's typically an output (unless it's a pass-by-reference input, but blueprint usually surfaces this as both or reference)
            const bool bIsOutput = bIsReturn || (bIsOut && !Param->HasAnyPropertyFlags(CPF_ReferenceParm));

            TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
            ParamObj->SetStringField(TEXT("name"), Param->GetAuthoredName());
            ParamObj->SetStringField(TEXT("cpp_type"), Param->GetCPPType());
            ParamObj->SetStringField(TEXT("class_type"), Param->GetClass()->GetName());

            if (bIsOutput)
            {
                OutputsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
            }
            else
            {
                InputsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
            }
        }

        FuncObj->SetArrayField(TEXT("inputs"), InputsArray);
        FuncObj->SetArrayField(TEXT("outputs"), OutputsArray);
        FunctionsArray.Add(MakeShared<FJsonValueObject>(FuncObj));
    }

    ResultObj->SetArrayField(TEXT("functions"), FunctionsArray);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleReadBlueprintMacros(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));

    UBlueprint* BP = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintName));
    if (!BP)
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load Blueprint at: %s"), *BlueprintName));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("blueprint_name"), BP->GetName());

    TArray<TSharedPtr<FJsonValue>> MacrosArray;
    for (UEdGraph* MacroGraph : BP->MacroGraphs)
    {
        if (!MacroGraph) continue;
        TSharedPtr<FJsonObject> MacroObj = MakeShared<FJsonObject>();
        MacroObj->SetStringField(TEXT("name"), MacroGraph->GetName());
        MacroObj->SetNumberField(TEXT("node_count"), MacroGraph->Nodes.Num());

        // Collect tunnel input/output pins from macro tunnel nodes
        TArray<TSharedPtr<FJsonValue>> InputPins, OutputPins;
        for (UEdGraphNode* Node : MacroGraph->Nodes)
        {
            if (!Node) continue;
            FString NodeClass = Node->GetClass()->GetName();
            if (NodeClass.Contains(TEXT("Tunnel")))
            {
                for (const UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin->Direction == EGPD_Output && Pin->PinName != TEXT("then"))
                    {
                        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
                        P->SetStringField(TEXT("name"), Pin->PinName.ToString());
                        P->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
                        InputPins.Add(MakeShared<FJsonValueObject>(P));
                    }
                    else if (Pin->Direction == EGPD_Input && Pin->PinName != TEXT("execute"))
                    {
                        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
                        P->SetStringField(TEXT("name"), Pin->PinName.ToString());
                        P->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
                        OutputPins.Add(MakeShared<FJsonValueObject>(P));
                    }
                }
            }
        }
        MacroObj->SetArrayField(TEXT("inputs"), InputPins);
        MacroObj->SetArrayField(TEXT("outputs"), OutputPins);
        MacrosArray.Add(MakeShared<FJsonValueObject>(MacroObj));
    }

    ResultObj->SetArrayField(TEXT("macros"), MacrosArray);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleReadBlueprintEnum(const TSharedPtr<FJsonObject>& Params)
{
    FString EnumPath;
    if (!Params->TryGetStringField(TEXT("enum_path"), EnumPath))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'enum_path' parameter"));
    }

    const UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(UEditorAssetLibrary::LoadAsset(EnumPath));
    if (!Enum)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UserDefinedEnum at path: %s"), *EnumPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("enum_path"), EnumPath);
    ResultObj->SetStringField(TEXT("enum_name"), Enum->GetName());

    const int32 NumEnums = Enum->NumEnums();
    TArray<TSharedPtr<FJsonValue>> EnumValuesArray;

    // Generally, Enum->NumEnums() includes the _MAX value.
    // We iterate through all of them and exclude anything ending in _MAX.
    for (int32 i = 0; i < NumEnums; i++)
    {
        FString EnumNameStr = Enum->GetNameStringByIndex(i);
        if (EnumNameStr.EndsWith(TEXT("_MAX")))
            continue;

        FString DisplayNameStr = Enum->GetDisplayNameTextByIndex(i).ToString();

        TSharedPtr<FJsonObject> EnumValueObj = MakeShared<FJsonObject>();
        EnumValueObj->SetNumberField(TEXT("index"), i);
        EnumValueObj->SetStringField(TEXT("name"), EnumNameStr);
        EnumValueObj->SetStringField(TEXT("display_name"), DisplayNameStr);

        EnumValuesArray.Add(MakeShared<FJsonValueObject>(EnumValueObj));
    }

    ResultObj->SetArrayField(TEXT("values"), EnumValuesArray);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleReadBlueprintStruct(const TSharedPtr<FJsonObject>& Params)
{
    FString StructPath;
    if (!Params->TryGetStringField(TEXT("struct_path"), StructPath))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'struct_path' parameter"));
    }

    const UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(UEditorAssetLibrary::LoadAsset(StructPath));
    if (!Struct)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UserDefinedStruct at path: %s"), *StructPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("struct_path"), StructPath);
    ResultObj->SetStringField(TEXT("struct_name"), Struct->GetName());

    TArray<TSharedPtr<FJsonValue>> VariablesArray;

    for (TFieldIterator<FProperty> It(Struct); It; ++It)
    {
        const FProperty* Property = *It;
        
        TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
        // Get user-friendly name or variable name
        VarObj->SetStringField(TEXT("name"), Property->GetAuthoredName());
        VarObj->SetStringField(TEXT("cpp_type"), Property->GetCPPType());
        VarObj->SetStringField(TEXT("class_type"), Property->GetClass()->GetName());

        VariablesArray.Add(MakeShared<FJsonValueObject>(VarObj));
    }

    ResultObj->SetArrayField(TEXT("variables"), VariablesArray);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleReadDataAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    UDataAsset* DataAsset = Cast<UDataAsset>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!DataAsset)
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load DataAsset at: %s"), *AssetPath));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetStringField(TEXT("asset_name"), DataAsset->GetName());
    ResultObj->SetStringField(TEXT("class_name"), DataAsset->GetClass()->GetName());

    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (TFieldIterator<FProperty> It(DataAsset->GetClass()); It; ++It)
    {
        const FProperty* Prop = *It;
        if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_EditorOnly)) continue;

        FString ValueStr;
        Prop->ContainerPtrToValuePtr<void>(DataAsset);
        Prop->ExportTextItem_InContainer(ValueStr, DataAsset, nullptr, nullptr, PPF_None);

        TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
        PropObj->SetStringField(TEXT("name"), Prop->GetAuthoredName());
        PropObj->SetStringField(TEXT("cpp_type"), Prop->GetCPPType());
        PropObj->SetStringField(TEXT("value"), ValueStr);
        PropsArray.Add(MakeShared<FJsonValueObject>(PropObj));
    }

    ResultObj->SetArrayField(TEXT("properties"), PropsArray);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleReadInputAction(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    UInputAction* Action = Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!Action)
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load InputAction at: %s"), *AssetPath));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetStringField(TEXT("action_name"), Action->GetName());

    // ValueType enum: Digital=0, Axis1D=1, Axis2D=2, Axis3D=3
    const UEnum* ValueTypeEnum = StaticEnum<EInputActionValueType>();
    const FString ValueTypeStr = ValueTypeEnum ? ValueTypeEnum->GetNameStringByValue(static_cast<int64>(Action->ValueType)) : TEXT("Unknown");
    ResultObj->SetStringField(TEXT("value_type"), ValueTypeStr);
    ResultObj->SetBoolField(TEXT("consume_input"), Action->bConsumeInput);

    // Triggers
    TArray<TSharedPtr<FJsonValue>> TriggersArray;
    for (const UInputTrigger* Trigger : Action->Triggers)
    {
        if (Trigger)
            TriggersArray.Add(MakeShared<FJsonValueString>(Trigger->GetClass()->GetName()));
    }
    ResultObj->SetArrayField(TEXT("triggers"), TriggersArray);

    // Modifiers
    TArray<TSharedPtr<FJsonValue>> ModifiersArray;
    for (const UInputModifier* Modifier : Action->Modifiers)
    {
        if (Modifier)
            ModifiersArray.Add(MakeShared<FJsonValueString>(Modifier->GetClass()->GetName()));
    }
    ResultObj->SetArrayField(TEXT("modifiers"), ModifiersArray);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleReadInputMappingContext(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    const UInputMappingContext* IMC = Cast<UInputMappingContext>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!IMC)
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load InputMappingContext at: %s"), *AssetPath));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetStringField(TEXT("imc_name"), IMC->GetName());

    TArray<TSharedPtr<FJsonValue>> MappingsArray;
    for (const FEnhancedActionKeyMapping& Mapping : IMC->GetMappings())
    {
        TSharedPtr<FJsonObject> MappingObj = MakeShared<FJsonObject>();
        FString ActionName = Mapping.Action.Get() ? Mapping.Action->GetName() : FString(TEXT("None"));
        MappingObj->SetStringField(TEXT("action"), ActionName);
        MappingObj->SetStringField(TEXT("key"), Mapping.Key.GetDisplayName().ToString());

        TArray<TSharedPtr<FJsonValue>> TrigArr;
        for (const UInputTrigger* T : Mapping.Triggers)
            if (T) TrigArr.Add(MakeShared<FJsonValueString>(T->GetClass()->GetName()));
        MappingObj->SetArrayField(TEXT("triggers"), TrigArr);

        TArray<TSharedPtr<FJsonValue>> ModArr;
        for (const UInputModifier* M : Mapping.Modifiers)
            if (M) ModArr.Add(MakeShared<FJsonValueString>(M->GetClass()->GetName()));
        MappingObj->SetArrayField(TEXT("modifiers"), ModArr);

        MappingsArray.Add(MakeShared<FJsonValueObject>(MappingObj));
    }

    ResultObj->SetArrayField(TEXT("mappings"), MappingsArray);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleReadWidgetVariables(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));

    const UBlueprint* WidgetBP = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintName));
    if (!WidgetBP)
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load WidgetBlueprint at: %s"), *BlueprintName));

    const UClass* GenClass = WidgetBP->GeneratedClass;
    if (!GenClass)
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("WidgetBlueprint has no GeneratedClass"));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("widget_name"), WidgetBP->GetName());

    TArray<TSharedPtr<FJsonValue>> VarsArray;
    for (TFieldIterator<FProperty> It(GenClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        const FProperty* Prop = *It;
        TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
        VarObj->SetStringField(TEXT("name"), Prop->GetAuthoredName());
        VarObj->SetStringField(TEXT("cpp_type"), Prop->GetCPPType());
        VarObj->SetStringField(TEXT("class_type"), Prop->GetClass()->GetName());
        VarsArray.Add(MakeShared<FJsonValueObject>(VarObj));
    }

    ResultObj->SetArrayField(TEXT("variables"), VarsArray);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleReadSaveGameBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));

    const UBlueprint* BP = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintName));
    if (!BP)
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load Blueprint at: %s"), *BlueprintName));

    const UClass* GenClass = BP->GeneratedClass;
    if (!GenClass)
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Blueprint has no GeneratedClass"));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("blueprint_name"), BP->GetName());

    TArray<TSharedPtr<FJsonValue>> VarsArray;
    for (TFieldIterator<FProperty> It(GenClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        const FProperty* Prop = *It;
        // For SaveGame, we only care about SaveGame-tagged properties
        const bool bSaveGame = Prop->HasAnyPropertyFlags(CPF_SaveGame);
        TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
        VarObj->SetStringField(TEXT("name"), Prop->GetAuthoredName());
        VarObj->SetStringField(TEXT("cpp_type"), Prop->GetCPPType());
        VarObj->SetStringField(TEXT("class_type"), Prop->GetClass()->GetName());
        VarObj->SetBoolField(TEXT("is_save_game"), bSaveGame);
        VarsArray.Add(MakeShared<FJsonValueObject>(VarObj));
    }

    ResultObj->SetArrayField(TEXT("variables"), VarsArray);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleReadMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    const UMaterialInterface* MatInterface = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!MatInterface)
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load Material at: %s"), *AssetPath));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetStringField(TEXT("material_name"), MatInterface->GetName());
    ResultObj->SetStringField(TEXT("class_name"), MatInterface->GetClass()->GetName());

    // Scalar parameters
    TArray<FMaterialParameterInfo> ScalarInfos;
    TArray<FGuid> ScalarGuids;
    MatInterface->GetAllScalarParameterInfo(ScalarInfos, ScalarGuids);
    TArray<TSharedPtr<FJsonValue>> ScalarsArray;
    for (const FMaterialParameterInfo& Info : ScalarInfos)
    {
        float Val = 0.f;
        MatInterface->GetScalarParameterValue(Info, Val);
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("name"), Info.Name.ToString());
        P->SetNumberField(TEXT("value"), Val);
        ScalarsArray.Add(MakeShared<FJsonValueObject>(P));
    }
    ResultObj->SetArrayField(TEXT("scalar_parameters"), ScalarsArray);

    // Vector parameters
    TArray<FMaterialParameterInfo> VectorInfos;
    TArray<FGuid> VectorGuids;
    MatInterface->GetAllVectorParameterInfo(VectorInfos, VectorGuids);
    TArray<TSharedPtr<FJsonValue>> VectorsArray;
    for (const FMaterialParameterInfo& Info : VectorInfos)
    {
        FLinearColor Val;
        MatInterface->GetVectorParameterValue(Info, Val);
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("name"), Info.Name.ToString());
        P->SetNumberField(TEXT("r"), Val.R);
        P->SetNumberField(TEXT("g"), Val.G);
        P->SetNumberField(TEXT("b"), Val.B);
        P->SetNumberField(TEXT("a"), Val.A);
        VectorsArray.Add(MakeShared<FJsonValueObject>(P));
    }
    ResultObj->SetArrayField(TEXT("vector_parameters"), VectorsArray);

    // Texture parameters
    TArray<FMaterialParameterInfo> TexInfos;
    TArray<FGuid> TexGuids;
    MatInterface->GetAllTextureParameterInfo(TexInfos, TexGuids);
    TArray<TSharedPtr<FJsonValue>> TexturesArray;
    for (const FMaterialParameterInfo& Info : TexInfos)
    {
        UTexture* Tex = nullptr;
        MatInterface->GetTextureParameterValue(Info, Tex);
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("name"), Info.Name.ToString());
        P->SetStringField(TEXT("texture"), Tex ? Tex->GetPathName() : TEXT("None"));
        TexturesArray.Add(MakeShared<FJsonValueObject>(P));
    }
    ResultObj->SetArrayField(TEXT("texture_parameters"), TexturesArray);

    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleReadSoundClass(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    const UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load audio asset at: %s"), *AssetPath));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetStringField(TEXT("asset_name"), Asset->GetName());
    ResultObj->SetStringField(TEXT("class_name"), Asset->GetClass()->GetName());

    // Generically export all editable properties via text
    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (TFieldIterator<FProperty> It(Asset->GetClass()); It; ++It)
    {
        const FProperty* Prop = *It;
        if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;

        FString ValueStr;
        Prop->ExportTextItem_InContainer(ValueStr, Asset, nullptr, nullptr, PPF_None);

        TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
        PropObj->SetStringField(TEXT("name"), Prop->GetAuthoredName());
        PropObj->SetStringField(TEXT("cpp_type"), Prop->GetCPPType());
        PropObj->SetStringField(TEXT("value"), ValueStr);
        PropsArray.Add(MakeShared<FJsonValueObject>(PropObj));
    }

    ResultObj->SetArrayField(TEXT("properties"), PropsArray);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticBlueprintQueryCommands::HandleReadBlueprintGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    FString GraphName;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }
    
    if (!Params->TryGetStringField(TEXT("graph_name"), GraphName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'graph_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* TargetGraph = nullptr;
    
    // Search all graphs
    TArray<UEdGraph*> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);
    
    for (UEdGraph* Graph : AllGraphs)
    {
        if (Graph && Graph->GetName() == GraphName)
        {
            TargetGraph = Graph;
            break;
        }
    }
    
    if (!TargetGraph)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph '%s' not found in Blueprint: %s"), *GraphName, *BlueprintName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("graph_name"), TargetGraph->GetName());
    ResultObj->SetStringField(TEXT("graph_type"), TargetGraph->GetClass()->GetName());

    // Extract all nodes
    TArray<TSharedPtr<FJsonValue>> NodesArray;
    
    for (UEdGraphNode* Node : TargetGraph->Nodes)
    {
        if (!Node) continue;
        
        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
        NodeObj->SetStringField(TEXT("node_name"), Node->GetName());
        NodeObj->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
        NodeObj->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
        NodeObj->SetNumberField(TEXT("position_x"), Node->NodePosX);
        NodeObj->SetNumberField(TEXT("position_y"), Node->NodePosY);
        
        // Detailed Pins with connections
        TArray<TSharedPtr<FJsonValue>> PinsArray;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin) continue;
            
            TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
            PinObj->SetStringField(TEXT("pin_id"), Pin->PinId.ToString());
            PinObj->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
            PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
            PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
            PinObj->SetStringField(TEXT("sub_category"), Pin->PinType.PinSubCategory.ToString());
            
            UObject* SubCategoryObject = Pin->PinType.PinSubCategoryObject.Get();
            if (SubCategoryObject)
            {
                PinObj->SetStringField(TEXT("sub_category_object"), SubCategoryObject->GetName());
            }

            PinObj->SetStringField(TEXT("tooltip"), Pin->PinToolTip);
            PinObj->SetStringField(TEXT("default_value"), Pin->GetDefaultAsString());
            
            // Extract Connections
            TArray<TSharedPtr<FJsonValue>> LinkedToArray;
            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (LinkedPin && LinkedPin->GetOwningNode())
                {
                    TSharedPtr<FJsonObject> LinkedObj = MakeShared<FJsonObject>();
                    LinkedObj->SetStringField(TEXT("target_node_id"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
                    LinkedObj->SetStringField(TEXT("target_pin_name"), LinkedPin->PinName.ToString());
                    LinkedToArray.Add(MakeShared<FJsonValueObject>(LinkedObj));
                }
            }
            PinObj->SetArrayField(TEXT("linked_to"), LinkedToArray);
            
            PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
        }
        
        NodeObj->SetArrayField(TEXT("pins"), PinsArray);
        NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
    }

    ResultObj->SetArrayField(TEXT("nodes"), NodesArray);
    ResultObj->SetNumberField(TEXT("node_count"), NodesArray.Num());
    ResultObj->SetBoolField(TEXT("success"), true);
    
    return ResultObj;
}

