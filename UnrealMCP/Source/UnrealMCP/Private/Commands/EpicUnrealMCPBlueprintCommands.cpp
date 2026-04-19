#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
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
#include "EpicUnrealMCPModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Materials/MaterialInstanceConstant.h"

FEpicUnrealMCPBlueprintCommands::FEpicUnrealMCPBlueprintCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_blueprint"))
    {
        return HandleCreateBlueprint(Params);
    }
    else if (CommandType == TEXT("add_component_to_blueprint"))
    {
        return HandleAddComponentToBlueprint(Params);
    }
    else if (CommandType == TEXT("set_physics_properties"))
    {
        return HandleSetPhysicsProperties(Params);
    }
    else if (CommandType == TEXT("compile_blueprint"))
    {
        return HandleCompileBlueprint(Params);
    }
    else if (CommandType == TEXT("set_static_mesh_properties"))
    {
        return HandleSetStaticMeshProperties(Params);
    }
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    else if (CommandType == TEXT("set_mesh_material_color"))
    {
        return HandleSetMeshMaterialColor(Params);
    }
    else if (CommandType == TEXT("create_blueprint_variable"))
    {
        return HandleCreateBlueprintVariable(Params);
    }
    else if (CommandType == TEXT("add_blueprint_event_node"))
    {
        return HandleAddBlueprintEventNode(Params);
    }
    else if (CommandType == TEXT("add_blueprint_function_node"))
    {
        return HandleAddBlueprintFunctionNode(Params);
    }
    else if (CommandType == TEXT("connect_blueprint_nodes"))
    {
        return HandleConnectBlueprintNodes(Params);
    }
    else if (CommandType == TEXT("add_blueprint_branch_node"))
    {
        return HandleAddBlueprintBranchNode(Params);
    }
    else if (CommandType == TEXT("create_blueprint_custom_event"))
    {
        return HandleCreateBlueprintCustomEvent(Params);
    }
    else if (CommandType == TEXT("read_blueprint_enum"))
    {
        return HandleReadBlueprintEnum(Params);
    }
    else if (CommandType == TEXT("read_blueprint_struct"))
    {
        return HandleReadBlueprintStruct(Params);
    }
    else if (CommandType == TEXT("read_blueprint_functions"))
    {
        return HandleReadBlueprintFunctions(Params);
    }
    else if (CommandType == TEXT("read_data_asset"))
    {
        return HandleReadDataAsset(Params);
    }
    else if (CommandType == TEXT("read_input_action"))
    {
        return HandleReadInputAction(Params);
    }
    else if (CommandType == TEXT("read_input_mapping_context"))
    {
        return HandleReadInputMappingContext(Params);
    }
    else if (CommandType == TEXT("read_widget_variables"))
    {
        return HandleReadWidgetVariables(Params);
    }
    else if (CommandType == TEXT("read_savegame_blueprint"))
    {
        return HandleReadSaveGameBlueprint(Params);
    }
    else if (CommandType == TEXT("read_material"))
    {
        return HandleReadMaterial(Params);
    }
    else if (CommandType == TEXT("read_blueprint_macros"))
    {
        return HandleReadBlueprintMacros(Params);
    }
    else if (CommandType == TEXT("read_sound_class"))
    {
        return HandleReadSoundClass(Params);
    }
    else if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("spawn_actor"))
    {
        return HandleSpawnActor(Params);
    }
    else if (CommandType == TEXT("destroy_actor"))
    {
        return HandleDestroyActor(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    else if (CommandType == TEXT("start_play_in_editor"))
    {
        return HandleStartPlayInEditor(Params);
    }
    else if (CommandType == TEXT("stop_play_in_editor"))
    {
        return HandleStopPlayInEditor(Params);
    }
    else if (CommandType == TEXT("get_editor_logs"))
    {
        return HandleGetEditorLogs(Params);
    }
    else if (CommandType == TEXT("create_material_instance"))
    {
        return HandleCreateMaterialInstance(Params);
    }
    else if (CommandType == TEXT("duplicate_asset"))
    {
        return HandleDuplicateAsset(Params);
    }
    
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    const FString PackagePath = TEXT("/Game/Blueprints/");
    const FString AssetName = BlueprintName;
    if (UEditorAssetLibrary::DoesAssetExist(PackagePath + AssetName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint already exists: %s"), *BlueprintName));
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();

    FString ParentClass;
    Params->TryGetStringField(TEXT("parent_class"), ParentClass);

    UClass* SelectedParentClass = AActor::StaticClass();

    if (!ParentClass.IsEmpty())
    {
        FString ClassName = ParentClass;
        if (!ClassName.StartsWith(TEXT("A")))
        {
            ClassName = TEXT("A") + ClassName;
        }

        UClass* FoundClass;
        if (ClassName == TEXT("APawn"))
        {
            FoundClass = APawn::StaticClass();
        }
        else if (ClassName == TEXT("AActor"))
        {
            FoundClass = AActor::StaticClass();
        }
        else
        {
            // Try loading the class - LoadClass is more reliable than FindObject for runtime lookups
            const FString ClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
            FoundClass = LoadClass<AActor>(nullptr, *ClassPath);

            if (!FoundClass)
            {
                const FString GameClassPath = FString::Printf(TEXT("/Script/Game.%s"), *ClassName);
                FoundClass = LoadClass<AActor>(nullptr, *GameClassPath);
            }
        }

        if (FoundClass)
        {
            SelectedParentClass = FoundClass;
            UE_LOG(LogTemp, Log, TEXT("Successfully set parent class to '%s'"), *ClassName);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not find specified parent class '%s' at paths: /Script/Engine.%s or /Script/Game.%s, defaulting to AActor"),
                *ClassName, *ClassName, *ClassName);
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

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create blueprint"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAddComponentToBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentType;
    if (!Params->TryGetStringField(TEXT("component_type"), ComponentType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
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
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown component type: %s"), *ComponentType));
    }

    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, *ComponentName);
    if (NewNode)
    {
        USceneComponent* SceneComponent = Cast<USceneComponent>(NewNode->ComponentTemplate);
        if (SceneComponent)
        {
            if (Params->HasField(TEXT("location")))
            {
                SceneComponent->SetRelativeLocation(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
            }
            if (Params->HasField(TEXT("rotation")))
            {
                SceneComponent->SetRelativeRotation(FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")));
            }
            if (Params->HasField(TEXT("scale")))
            {
                SceneComponent->SetRelativeScale3D(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
            }
        }

        Blueprint->SimpleConstructionScript->AddNode(NewNode);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("component_name"), ComponentName);
        ResultObj->SetStringField(TEXT("component_type"), ComponentType);
        return ResultObj;
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add component to blueprint"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    const USCS_Node* ComponentNode = nullptr;
    for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    if (Params->HasField(TEXT("simulate_physics")))
    {
        PrimComponent->SetSimulatePhysics(Params->GetBoolField(TEXT("simulate_physics")));
    }

    if (Params->HasField(TEXT("mass")))
    {
        const float Mass = Params->GetNumberField(TEXT("mass"));
        // UE5.5 requires SetMassOverrideInKg for proper mass control
        PrimComponent->SetMassOverrideInKg(NAME_None, Mass);
        UE_LOG(LogTemp, Display, TEXT("Set mass for component %s to %f kg"), *ComponentName, Mass);
    }

    if (Params->HasField(TEXT("linear_damping")))
    {
        PrimComponent->SetLinearDamping(Params->GetNumberField(TEXT("linear_damping")));
    }

    if (Params->HasField(TEXT("angular_damping")))
    {
        PrimComponent->SetAngularDamping(Params->GetNumberField(TEXT("angular_damping")));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), BlueprintName);
    ResultObj->SetBoolField(TEXT("compiled"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    const UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));

    // Small delay to let the engine process newly compiled classes
    FPlatformProcess::Sleep(0.2f);

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform);

    if (NewActor)
    {
        NewActor->SetActorLabel(*ActorName);
        return FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    const USCS_Node* ComponentNode = nullptr;
    for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
    if (!MeshComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a static mesh component"));
    }

    if (Params->HasField(TEXT("static_mesh")))
    {
        const FString MeshPath = Params->GetStringField(TEXT("static_mesh"));
        if (UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath)))
        {
            MeshComponent->SetStaticMesh(Mesh);
        }
    }

    if (Params->HasField(TEXT("material")))
    {
        const FString MaterialPath = Params->GetStringField(TEXT("material"));
        if (UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath)))
        {
            MeshComponent->SetMaterial(0, Material);
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetMeshMaterialColor(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    const USCS_Node* ComponentNode = nullptr;
    for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    TArray<float> ColorArray;
    const TArray<TSharedPtr<FJsonValue>>* ColorJsonArray;
    if (!Params->TryGetArrayField(TEXT("color"), ColorJsonArray) || ColorJsonArray->Num() != 4)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'color' must be an array of 4 float values [R, G, B, A]"));
    }

    for (const TSharedPtr<FJsonValue>& Value : *ColorJsonArray)
    {
        ColorArray.Add(FMath::Clamp(Value->AsNumber(), 0.0f, 1.0f));
    }

    FLinearColor Color(ColorArray[0], ColorArray[1], ColorArray[2], ColorArray[3]);

    int32 MaterialSlot = 0;
    if (Params->HasField(TEXT("material_slot")))
    {
        MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
    }

    FString ParameterName = TEXT("BaseColor");
    Params->TryGetStringField(TEXT("parameter_name"), ParameterName);

    UMaterialInterface* Material;

    if (FString MaterialPath; Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (!Material)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
        }
    }
    else
    {
        Material = PrimComponent->GetMaterial(MaterialSlot);
        if (!Material)
        {
            Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial")));
            if (!Material)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No material found on component and failed to load default material"));
            }
        }
    }

    UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(Material, PrimComponent);
    if (!DynMaterial)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create dynamic material instance"));
    }

    DynMaterial->SetVectorParameterValue(*ParameterName, Color);
    PrimComponent->SetMaterial(MaterialSlot, DynMaterial);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    UE_LOG(LogTemp, Log, TEXT("Set material color on component %s: R=%f, G=%f, B=%f, A=%f"),
        *ComponentName, Color.R, Color.G, Color.B, Color.A);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    ResultObj->SetNumberField(TEXT("material_slot"), MaterialSlot);
    ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);

    TArray<TSharedPtr<FJsonValue>> ColorResultArray;
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.R));
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.G));
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.B));
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.A));
    ResultObj->SetArrayField(TEXT("color"), ColorResultArray);

    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    FString VariableType;
    if (!Params->TryGetStringField(TEXT("variable_type"), VariableType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_type' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
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
            NewVariable.DefaultValue = FString::FromInt((int32)DefaultValue->AsNumber());
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

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAddBlueprintEventNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString EventType;
    if (!Params->TryGetStringField(TEXT("event_type"), EventType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'event_type' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find or create event graph"));
    }

    FVector2D NodePosition(0, 0);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FEpicUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    const UK2Node_Event* EventNode;

    if (EventType == TEXT("BeginPlay"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveBeginPlay"), NodePosition);
    }
    else if (EventType == TEXT("Tick"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveTick"), NodePosition);
    }
    else if (EventType == TEXT("BeginOverlap"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveActorBeginOverlap"), NodePosition);
    }
    else if (EventType == TEXT("EndOverlap"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveActorEndOverlap"), NodePosition);
    }
    else if (EventType == TEXT("Hit"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveHit"), NodePosition);
    }
    else if (EventType == TEXT("AnyDamage"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveAnyDamage"), NodePosition);
    }
    else if (EventType == TEXT("Destroyed"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveDestroyed"), NodePosition);
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
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown event type: %s"), *EventType));
    }

    if (!EventNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create event node"));
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

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAddBlueprintFunctionNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find or create event graph"));
    }

    FVector2D NodePosition(300, 0);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FEpicUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    FString FunctionClass;
    Params->TryGetStringField(TEXT("function_class"), FunctionClass);

    UFunction* Function = nullptr;

    if (!FunctionClass.IsEmpty())
    {
        if (const UClass* TargetClass = FindObject<UClass>(nullptr, *FunctionClass))
        {
            Function = TargetClass->FindFunctionByName(*FunctionName);
        }
    }
    else
    {
        if (Blueprint->ParentClass)
        {
            Function = Blueprint->ParentClass->FindFunctionByName(*FunctionName);
        }
    }

    if (!Function)
    {
        const UClass* GameplayStatics = UGameplayStatics::StaticClass();
        Function = GameplayStatics->FindFunctionByName(*FunctionName);
    }

    if (!Function)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
    }

    const UK2Node_CallFunction* FunctionNode = FEpicUnrealMCPCommonUtils::CreateFunctionCallNode(EventGraph, Function, NodePosition);
    if (!FunctionNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create function call node"));
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

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString SourceNodeId, SourcePin, TargetNodeId, TargetPin;
    if (!Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId) ||
        !Params->TryGetStringField(TEXT("source_pin"), SourcePin) ||
        !Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId) ||
        !Params->TryGetStringField(TEXT("target_pin"), TargetPin))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing connection parameters"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find event graph"));
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
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not find source or target nodes"));
    }

    const bool bConnected = FEpicUnrealMCPCommonUtils::ConnectGraphNodes(EventGraph, SourceNode, SourcePin, TargetNode, TargetPin);
    if (!bConnected)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to connect nodes"));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("source_node"), SourceNodeId);
    ResultObj->SetStringField(TEXT("target_node"), TargetNodeId);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAddBlueprintBranchNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find or create event graph"));
    }

    FVector2D NodePosition(600, 0);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FEpicUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
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

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateBlueprintCustomEvent(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString EventName;
    if (!Params->TryGetStringField(TEXT("event_name"), EventName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find or create event graph"));
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

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadBlueprintEnum(const TSharedPtr<FJsonObject>& Params)
{
    FString EnumPath;
    if (!Params->TryGetStringField(TEXT("enum_path"), EnumPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'enum_path' parameter"));
    }

    const UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(UEditorAssetLibrary::LoadAsset(EnumPath));
    if (!Enum)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UserDefinedEnum at path: %s"), *EnumPath));
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

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadBlueprintStruct(const TSharedPtr<FJsonObject>& Params)
{
    FString StructPath;
    if (!Params->TryGetStringField(TEXT("struct_path"), StructPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'struct_path' parameter"));
    }

    const UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(UEditorAssetLibrary::LoadAsset(StructPath));
    if (!Struct)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UserDefinedStruct at path: %s"), *StructPath));
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

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadBlueprintFunctions(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    const UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load Blueprint at path: %s"), *BlueprintPath));
    }

    const UClass* GenClass = Blueprint->GeneratedClass;
    if (!GenClass)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint does not have a GeneratedClass (might need compilation)"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
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

// ---- HandleReadDataAsset ----
TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadDataAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    UDataAsset* DataAsset = Cast<UDataAsset>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!DataAsset)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load DataAsset at: %s"), *AssetPath));

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

// ---- HandleReadInputAction ----
TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadInputAction(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    UInputAction* Action = Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!Action)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load InputAction at: %s"), *AssetPath));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetStringField(TEXT("action_name"), Action->GetName());

    // ValueType enum: Digital=0, Axis1D=1, Axis2D=2, Axis3D=3
    const UEnum* ValueTypeEnum = StaticEnum<EInputActionValueType>();
    const FString ValueTypeStr = ValueTypeEnum ? ValueTypeEnum->GetNameStringByValue((int64)Action->ValueType) : TEXT("Unknown");
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

// ---- HandleReadInputMappingContext ----
TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadInputMappingContext(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    const UInputMappingContext* IMC = Cast<UInputMappingContext>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!IMC)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load InputMappingContext at: %s"), *AssetPath));

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

// ---- HandleReadWidgetVariables ----
TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadWidgetVariables(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));

    const UBlueprint* WidgetBP = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    if (!WidgetBP)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load WidgetBlueprint at: %s"), *BlueprintPath));

    const UClass* GenClass = WidgetBP->GeneratedClass;
    if (!GenClass)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WidgetBlueprint has no GeneratedClass"));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
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

// ---- HandleReadSaveGameBlueprint ----
TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadSaveGameBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));

    const UBlueprint* BP = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    if (!BP)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load Blueprint at: %s"), *BlueprintPath));

    const UClass* GenClass = BP->GeneratedClass;
    if (!GenClass)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no GeneratedClass"));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
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

// ---- HandleReadMaterial ----
TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    const UMaterialInterface* MatInterface = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(AssetPath));
    if (!MatInterface)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load Material at: %s"), *AssetPath));

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

// ---- HandleReadBlueprintMacros ----
TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadBlueprintMacros(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));

    UBlueprint* BP = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    if (!BP)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load Blueprint at: %s"), *BlueprintPath));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
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

// ---- HandleReadSoundClass ----
TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadSoundClass(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));

    const UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load audio asset at: %s"), *AssetPath));

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

// ==============================================================================
// WORLD & LEVEL
// ==============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    const UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No Editor World available."));

    TArray<TSharedPtr<FJsonValue>> ActorsArray;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        const AActor* Actor = *It;
        TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
        ActorObj->SetStringField(TEXT("name"), Actor->GetActorNameOrLabel());
        ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
        
        TSharedPtr<FJsonObject> TransformObj = MakeShared<FJsonObject>();
        FTransform T = Actor->GetTransform();
        const FVector Loc = T.GetLocation();
        const FRotator Rot = T.GetRotation().Rotator();
        const FVector Scale = T.GetScale3D();
        
        TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
        LocObj->SetNumberField(TEXT("x"), Loc.X); LocObj->SetNumberField(TEXT("y"), Loc.Y); LocObj->SetNumberField(TEXT("z"), Loc.Z);
        TransformObj->SetObjectField(TEXT("location"), LocObj);
        
        TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
        RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch); RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw); RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
        TransformObj->SetObjectField(TEXT("rotation"), RotObj);

        TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
        ScaleObj->SetNumberField(TEXT("x"), Scale.X); ScaleObj->SetNumberField(TEXT("y"), Scale.Y); ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
        TransformObj->SetObjectField(TEXT("scale"), ScaleObj);

        ActorObj->SetObjectField(TEXT("transform"), TransformObj);
        ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorsArray);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ClassPath;
    if (!Params->TryGetStringField(TEXT("class_path"), ClassPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'class_path' parameter"));
    UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(ClassPath);
    UClass* SpawnClass = Cast<UClass>(LoadedAsset);
    UStaticMesh* LoadedMesh = nullptr;

    if (!SpawnClass) // Might be a Blueprint or a StaticMesh
    {
        if (const UBlueprint* BP = Cast<UBlueprint>(LoadedAsset)) 
        {
            SpawnClass = BP->GeneratedClass;
        }
        else 
        {
            LoadedMesh = Cast<UStaticMesh>(LoadedAsset);
            if (LoadedMesh)
            {
                SpawnClass = AStaticMeshActor::StaticClass();
            }
        }
    }

    if (!SpawnClass || !SpawnClass->IsChildOf(AActor::StaticClass()))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid class or not an AActor derivative"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No Editor World available."));

    double X = 0, Y = 0, Z = 0;
    const TSharedPtr<FJsonObject>* LocObj;
    if (Params->TryGetObjectField(TEXT("location"), LocObj))
    {
        (*LocObj)->TryGetNumberField(TEXT("x"), X);
        (*LocObj)->TryGetNumberField(TEXT("y"), Y);
        (*LocObj)->TryGetNumberField(TEXT("z"), Z);
    }

    const FVector Location(X, Y, Z);

    double Pitch = 0, Yaw = 0, Roll = 0;
    const TSharedPtr<FJsonObject>* RotObj;
    if (Params->TryGetObjectField(TEXT("rotation"), RotObj))
    {
        (*RotObj)->TryGetNumberField(TEXT("pitch"), Pitch);
        (*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw);
        (*RotObj)->TryGetNumberField(TEXT("roll"), Roll);
    }
    const FRotator Rotation(Pitch, Yaw, Roll);

    AActor* Spawned = World->SpawnActor<AActor>(SpawnClass, Location, Rotation);
    if (!Spawned) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn actor"));

    double ScaleX = 1.0, ScaleY = 1.0, ScaleZ = 1.0;
    const TSharedPtr<FJsonObject>* ScaleObj;
    if (Params->TryGetObjectField(TEXT("scale"), ScaleObj))
    {
        (*ScaleObj)->TryGetNumberField(TEXT("x"), ScaleX);
        (*ScaleObj)->TryGetNumberField(TEXT("y"), ScaleY);
        (*ScaleObj)->TryGetNumberField(TEXT("z"), ScaleZ);
        
        FTransform T = Spawned->GetTransform();
        T.SetScale3D(FVector(ScaleX, ScaleY, ScaleZ));
        Spawned->SetActorTransform(T);
    }

    if (LoadedMesh)
    {
        const AStaticMeshActor* SMA = Cast<AStaticMeshActor>(Spawned);
        if (SMA && SMA->GetStaticMeshComponent())
        {
            SMA->GetStaticMeshComponent()->SetStaticMesh(LoadedMesh);
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    // Return full actor info matching the new Python parsing code
    ResultObj->SetStringField(TEXT("name"), Spawned->GetActorNameOrLabel());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleDestroyActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));

    const UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No Editor World available."));

    bool bFound = false;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor->GetActorNameOrLabel() == ActorName)
        {
            Actor->Destroy();
            bFound = true;
            break;
        }
    }

    if (!bFound) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Actor not found"));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));

    const UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No Editor World available."));

    AActor* Target = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor->GetActorNameOrLabel() == ActorName)
        {
            Target = Actor;
            break;
        }
    }

    if (!Target) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Actor not found"));

    FTransform T = Target->GetTransform();

    const TSharedPtr<FJsonObject>* LocObj;
    if (Params->TryGetObjectField(TEXT("location"), LocObj))
    {
        double X = T.GetLocation().X, Y = T.GetLocation().Y, Z = T.GetLocation().Z;
        (*LocObj)->TryGetNumberField(TEXT("x"), X); (*LocObj)->TryGetNumberField(TEXT("y"), Y); (*LocObj)->TryGetNumberField(TEXT("z"), Z);
        T.SetLocation(FVector(X, Y, Z));
    }

    const TSharedPtr<FJsonObject>* RotObj;
    if (Params->TryGetObjectField(TEXT("rotation"), RotObj))
    {
        double P = T.GetRotation().Rotator().Pitch, YW = T.GetRotation().Rotator().Yaw, R = T.GetRotation().Rotator().Roll;
        (*RotObj)->TryGetNumberField(TEXT("pitch"), P); (*RotObj)->TryGetNumberField(TEXT("yaw"), YW); (*RotObj)->TryGetNumberField(TEXT("roll"), R);
        T.SetRotation(FRotator(P, YW, R).Quaternion());
    }

    const TSharedPtr<FJsonObject>* ScaleObj;
    if (Params->TryGetObjectField(TEXT("scale"), ScaleObj))
    {
        double X = T.GetScale3D().X, Y = T.GetScale3D().Y, Z = T.GetScale3D().Z;
        (*ScaleObj)->TryGetNumberField(TEXT("x"), X); (*ScaleObj)->TryGetNumberField(TEXT("y"), Y); (*ScaleObj)->TryGetNumberField(TEXT("z"), Z);
        T.SetScale3D(FVector(X, Y, Z));
    }
    
    Target->SetActorTransform(T);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// ==============================================================================
// PLAY IN EDITOR & LOGS
// ==============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleStartPlayInEditor(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not valid"));
    
    if (GEditor->PlayWorld) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Already in PIE"));

    const FRequestPlaySessionParams PlayParams;
    GEditor->RequestPlaySession(PlayParams);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleStopPlayInEditor(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not valid"));
    
    if (!GEditor->PlayWorld) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Not currently in PIE"));

    GEditor->RequestEndPlayMap();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetEditorLogs(const TSharedPtr<FJsonObject>& Params)
{
    TArray<FString> Logs = FEpicUnrealMCPModule::Get().GetRecentLogs();
    
    bool bClear = false;
    Params->TryGetBoolField(TEXT("clear_after_read"), bClear);
    if (bClear) FEpicUnrealMCPModule::Get().ClearLogs();

    TArray<TSharedPtr<FJsonValue>> LogArray;
    for (const FString& S : Logs)
    {
        LogArray.Add(MakeShared<FJsonValueString>(S));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("logs"), LogArray);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// ==============================================================================
// ASSET MANAGEMENT
// ==============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
    FString ParentPath, DestPath;
    if (!Params->TryGetStringField(TEXT("parent_path"), ParentPath) || !Params->TryGetStringField(TEXT("dest_path"), DestPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_path' or 'dest_path'"));

    UMaterialInterface* ParentMat = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(ParentPath));
    if (!ParentMat) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Parent material not found"));

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    FString PackageName, AssetName;
    AssetTools.CreateUniqueAssetName(DestPath, TEXT(""), PackageName, AssetName);

    const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

    UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMaterialInstanceConstant::StaticClass(), nullptr);
    UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(NewAsset);
    if (!MIC) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create MIC"));

    MIC->SetParentEditorOnly(ParentMat);
    
    // Parse vector parameters
    const TSharedPtr<FJsonObject>* VectorsObj;
    if (Params->TryGetObjectField(TEXT("vector_parameters"), VectorsObj))
    {
        for (const auto& Elem : (*VectorsObj)->Values)
        {
            if (Elem.Value->Type == EJson::String) // Hex color
            {
                FColor HexColor = FColor::FromHex(Elem.Value->AsString());
                MIC->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(*Elem.Key), FLinearColor(HexColor));
            }
        }
    }

    // Parse scalar parameters
    const TSharedPtr<FJsonObject>* ScalarsObj;
    if (Params->TryGetObjectField(TEXT("scalar_parameters"), ScalarsObj))
    {
        for (const auto& Elem : (*ScalarsObj)->Values)
        {
            if (Elem.Value->Type == EJson::Number)
            {
                MIC->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(*Elem.Key), Elem.Value->AsNumber());
            }
        }
    }

    MIC->PostEditChange();
    
    UEditorAssetLibrary::SaveLoadedAsset(MIC);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("asset_path"), MIC->GetPathName());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath, DestPath;
    if (!Params->TryGetStringField(TEXT("source_path"), SourcePath) || !Params->TryGetStringField(TEXT("dest_path"), DestPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' or 'dest_path'"));

    const UObject* Duplicated = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);
    if (!Duplicated) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("DuplicateAsset failed"));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("asset_path"), Duplicated->GetPathName());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}
