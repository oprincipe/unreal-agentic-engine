#include "Commands/UnrealAgenticAssetMutatorCommands.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Commands/UnrealAgenticCommonUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimBlueprint.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/UserDefinedEnum.h"
#include "Factories/BlueprintFactory.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"

// New tool includes
#include "AssetToolsModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/DataAsset.h"
#include "EngineUtils.h"
#include "UnrealAgenticModule.h"
#include "IAssetTools.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Sound/SoundBase.h"


TSharedPtr<FJsonObject> FUnrealAgenticAssetMutatorCommands::HandleCommand(
    const FString &CommandType, const TSharedPtr<FJsonObject> &Params) {
  if (CommandType == TEXT("set_physics_properties"))
    return HandleSetPhysicsProperties(Params);
  if (CommandType == TEXT("set_static_mesh_properties"))
    return HandleSetStaticMeshProperties(Params);
  if (CommandType == TEXT("set_skeletal_mesh_properties"))
    return HandleSetSkeletalMeshProperties(Params);
  if (CommandType == TEXT("set_blueprint_class_defaults"))
    return HandleSetBlueprintClassDefaults(Params);
  if (CommandType == TEXT("set_mesh_material_color"))
    return HandleSetMeshMaterialColor(Params);
  if (CommandType == TEXT("duplicate_asset"))
    return HandleDuplicateAsset(Params);
  if (CommandType == TEXT("destroy_actor"))
    return HandleDestroyActor(Params);
  if (CommandType == TEXT("start_play_in_editor"))
    return HandleStartPlayInEditor(Params);
  if (CommandType == TEXT("stop_play_in_editor"))
    return HandleStopPlayInEditor(Params);
  if (CommandType == TEXT("get_editor_logs"))
    return HandleGetEditorLogs(Params);
  if (CommandType == TEXT("create_material_instance"))
    return HandleCreateMaterialInstance(Params);
  if (CommandType == TEXT("spawn_blueprint_actor"))
    return HandleSpawnBlueprintActor(Params);

  return nullptr;
}

TSharedPtr<FJsonObject>
FUnrealAgenticAssetMutatorCommands::HandleSetPhysicsProperties(
    const TSharedPtr<FJsonObject> &Params) {
  FString BlueprintName;
  if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName)) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Missing 'blueprint_name' parameter"));
  }

  FString ComponentName;
  if (!Params->TryGetStringField(TEXT("component_name"), ComponentName)) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Missing 'component_name' parameter"));
  }

  UBlueprint *Blueprint =
      FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
  if (!Blueprint) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
  }

  const USCS_Node *ComponentNode = nullptr;
  for (const USCS_Node *Node :
       Blueprint->SimpleConstructionScript->GetAllNodes()) {
    if (Node && Node->GetVariableName().ToString() == ComponentName) {
      ComponentNode = Node;
      break;
    }
  }

  if (!ComponentNode) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Component not found: %s"), *ComponentName));
  }

  UPrimitiveComponent *PrimComponent =
      Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
  if (!PrimComponent) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Component is not a primitive component"));
  }

  if (Params->HasField(TEXT("simulate_physics"))) {
    PrimComponent->SetSimulatePhysics(
        Params->GetBoolField(TEXT("simulate_physics")));
  }

  if (Params->HasField(TEXT("mass"))) {
    const float Mass = Params->GetNumberField(TEXT("mass"));
    // UE5.5 requires SetMassOverrideInKg for proper mass control
    PrimComponent->SetMassOverrideInKg(NAME_None, Mass);
    UE_LOG(LogTemp, Display, TEXT("Set mass for component %s to %f kg"),
           *ComponentName, Mass);
  }

  if (Params->HasField(TEXT("linear_damping"))) {
    PrimComponent->SetLinearDamping(
        Params->GetNumberField(TEXT("linear_damping")));
  }

  if (Params->HasField(TEXT("angular_damping"))) {
    PrimComponent->SetAngularDamping(
        Params->GetNumberField(TEXT("angular_damping")));
  }

  FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

  TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
  ResultObj->SetStringField(TEXT("component"), ComponentName);
  return ResultObj;
}

TSharedPtr<FJsonObject>
FUnrealAgenticAssetMutatorCommands::HandleSetStaticMeshProperties(
    const TSharedPtr<FJsonObject> &Params) {
  FString BlueprintName;
  if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName)) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Missing 'blueprint_name' parameter"));
  }

  FString ComponentName;
  if (!Params->TryGetStringField(TEXT("component_name"), ComponentName)) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Missing 'component_name' parameter"));
  }

  UBlueprint *Blueprint =
      FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
  if (!Blueprint) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
  }

  const USCS_Node *ComponentNode = nullptr;
  for (const USCS_Node *Node :
       Blueprint->SimpleConstructionScript->GetAllNodes()) {
    if (Node && Node->GetVariableName().ToString() == ComponentName) {
      ComponentNode = Node;
      break;
    }
  }

  if (!ComponentNode) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Component not found: %s"), *ComponentName));
  }

  UStaticMeshComponent *MeshComponent =
      Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
  if (!MeshComponent) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Component is not a static mesh component"));
  }

  if (Params->HasField(TEXT("static_mesh"))) {
    const FString MeshPath = Params->GetStringField(TEXT("static_mesh"));
    if (UStaticMesh *Mesh =
            Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath))) {
      MeshComponent->SetStaticMesh(Mesh);
    }
  }

  if (Params->HasField(TEXT("material"))) {
    const FString MaterialPath = Params->GetStringField(TEXT("material"));
    if (UMaterialInterface *Material = Cast<UMaterialInterface>(
            UEditorAssetLibrary::LoadAsset(MaterialPath))) {
      MeshComponent->SetMaterial(0, Material);
    }
  }

  if (Params->HasField(TEXT("location"))) {
    MeshComponent->SetRelativeLocation(
        FUnrealAgenticCommonUtils::GetVectorFromJson(Params, TEXT("location")));
  }
  if (Params->HasField(TEXT("rotation"))) {
    MeshComponent->SetRelativeRotation(
        FUnrealAgenticCommonUtils::GetRotatorFromJson(Params,
                                                      TEXT("rotation")));
  }
  if (Params->HasField(TEXT("scale"))) {
    MeshComponent->SetRelativeScale3D(
        FUnrealAgenticCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
  }

  if (Params->HasField(TEXT("collision_setup"))) {
    FName ProfileName(*Params->GetStringField(TEXT("collision_setup")));
    MeshComponent->SetCollisionProfileName(ProfileName);
  }

  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

  TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
  ResultObj->SetStringField(TEXT("component"), ComponentName);
  return ResultObj;
}

TSharedPtr<FJsonObject>
FUnrealAgenticAssetMutatorCommands::HandleSetMeshMaterialColor(
    const TSharedPtr<FJsonObject> &Params) {
  FString BlueprintName;
  if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName)) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Missing 'blueprint_name' parameter"));
  }

  FString ComponentName;
  if (!Params->TryGetStringField(TEXT("component_name"), ComponentName)) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Missing 'component_name' parameter"));
  }

  UBlueprint *Blueprint =
      FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
  if (!Blueprint) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
  }

  const USCS_Node *ComponentNode = nullptr;
  for (const USCS_Node *Node :
       Blueprint->SimpleConstructionScript->GetAllNodes()) {
    if (Node && Node->GetVariableName().ToString() == ComponentName) {
      ComponentNode = Node;
      break;
    }
  }

  if (!ComponentNode) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Component not found: %s"), *ComponentName));
  }

  UPrimitiveComponent *PrimComponent =
      Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
  if (!PrimComponent) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Component is not a primitive component"));
  }

  FLinearColor Color(1.0f, 1.0f, 1.0f, 1.0f);
  bool bHasColor = false;

  if (Params->HasField(TEXT("color_hex"))) {
    FString HexStr = Params->GetStringField(TEXT("color_hex"));
    Color = FLinearColor(FColor::FromHex(HexStr));
    bHasColor = true;
  } else if (const TArray<TSharedPtr<FJsonValue>> *ColorJsonArray;
             Params->TryGetArrayField(TEXT("color"), ColorJsonArray) &&
             ColorJsonArray->Num() == 4) {
    Color.R = FMath::Clamp((*ColorJsonArray)[0]->AsNumber(), 0.0, 1.0);
    Color.G = FMath::Clamp((*ColorJsonArray)[1]->AsNumber(), 0.0, 1.0);
    Color.B = FMath::Clamp((*ColorJsonArray)[2]->AsNumber(), 0.0, 1.0);
    Color.A = FMath::Clamp((*ColorJsonArray)[3]->AsNumber(), 0.0, 1.0);
    bHasColor = true;
  }

  int32 MaterialSlot = 0;
  if (Params->HasField(TEXT("material_slot"))) {
    MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
  }

  FString ParameterName = TEXT("BaseColor");
  Params->TryGetStringField(TEXT("parameter_name"), ParameterName);

  UMaterialInterface *Material;

  if (FString MaterialPath;
      Params->TryGetStringField(TEXT("material_path"), MaterialPath)) {
    Material =
        Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material) {
      return FUnrealAgenticCommonUtils::CreateErrorResponse(
          FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
    }
  } else {
    Material = PrimComponent->GetMaterial(MaterialSlot);
    if (!Material) {
      Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(
          TEXT("/Engine/BasicShapes/BasicShapeMaterial")));
      if (!Material) {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(
            TEXT("No material found on component and failed to load default "
                 "material"));
      }
    }
  }

  if (bHasColor) {
    UMaterialInstanceDynamic *DynMaterial =
        UMaterialInstanceDynamic::Create(Material, PrimComponent);
    if (!DynMaterial) {
      return FUnrealAgenticCommonUtils::CreateErrorResponse(
          TEXT("Failed to create dynamic material instance"));
    }

    DynMaterial->SetVectorParameterValue(*ParameterName, Color);
    PrimComponent->SetMaterial(MaterialSlot, DynMaterial);
  } else {
    PrimComponent->SetMaterial(MaterialSlot, Material);
  }

  FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

  UE_LOG(LogTemp, Log,
         TEXT("Set material color on component %s: R=%f, G=%f, B=%f, A=%f"),
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

TSharedPtr<FJsonObject>
FUnrealAgenticAssetMutatorCommands::HandleDuplicateAsset(
    const TSharedPtr<FJsonObject> &Params) {
  FString SourcePath, DestPath;
  if (!Params->TryGetStringField(TEXT("source_path"), SourcePath) ||
      !Params->TryGetStringField(TEXT("dest_path"), DestPath))
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Missing 'source_path' or 'dest_path'"));

  const UObject *Duplicated =
      UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);
  if (!Duplicated)
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("DuplicateAsset failed"));

  TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
  ResultObj->SetStringField(TEXT("asset_path"), Duplicated->GetPathName());
  ResultObj->SetBoolField(TEXT("success"), true);
  return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticAssetMutatorCommands::HandleDestroyActor(
    const TSharedPtr<FJsonObject> &Params) {
  FString ActorName;
  if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Missing 'actor_name' parameter"));

  const UWorld *World =
      GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!World)
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("No Editor World available."));

  bool bFound = false;
  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Actor = *It;
    if (Actor->GetActorNameOrLabel() == ActorName) {
      Actor->Destroy();
      bFound = true;
      break;
    }
  }

  if (!bFound)
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Actor not found"));

  TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
  ResultObj->SetBoolField(TEXT("success"), true);
  return ResultObj;
}

TSharedPtr<FJsonObject>
FUnrealAgenticAssetMutatorCommands::HandleStartPlayInEditor(
    const TSharedPtr<FJsonObject> &Params) {
  if (!GEditor)
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("GEditor not valid"));

  if (GEditor->PlayWorld)
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Already in PIE"));

  const FRequestPlaySessionParams PlayParams;
  GEditor->RequestPlaySession(PlayParams);

  TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
  ResultObj->SetBoolField(TEXT("success"), true);
  return ResultObj;
}

TSharedPtr<FJsonObject>
FUnrealAgenticAssetMutatorCommands::HandleStopPlayInEditor(
    const TSharedPtr<FJsonObject> &Params) {
  if (!GEditor)
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("GEditor not valid"));

  if (!GEditor->PlayWorld)
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Not currently in PIE"));

  GEditor->RequestEndPlayMap();

  TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
  ResultObj->SetBoolField(TEXT("success"), true);
  return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticAssetMutatorCommands::HandleGetEditorLogs(
    const TSharedPtr<FJsonObject> &Params) {
  TArray<FString> Logs = FUnrealAgenticModule::Get().GetRecentLogs();

  bool bClear = false;
  Params->TryGetBoolField(TEXT("clear_after_read"), bClear);
  if (bClear)
    FUnrealAgenticModule::Get().ClearLogs();

  TArray<TSharedPtr<FJsonValue>> LogArray;
  for (const FString &S : Logs) {
    LogArray.Add(MakeShared<FJsonValueString>(S));
  }

  TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
  ResultObj->SetArrayField(TEXT("logs"), LogArray);
  ResultObj->SetBoolField(TEXT("success"), true);
  return ResultObj;
}

TSharedPtr<FJsonObject>
FUnrealAgenticAssetMutatorCommands::HandleCreateMaterialInstance(
    const TSharedPtr<FJsonObject> &Params) {
  FString ParentPath, DestPath;
  if (!Params->TryGetStringField(TEXT("parent_path"), ParentPath) ||
      !Params->TryGetStringField(TEXT("dest_path"), DestPath))
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Missing 'parent_path' or 'dest_path'"));

  UMaterialInterface *ParentMat =
      Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(ParentPath));
  if (!ParentMat)
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Parent material not found"));

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

  FString PackageName, AssetName;
  AssetTools.CreateUniqueAssetName(DestPath, TEXT(""), PackageName, AssetName);

  const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

  UObject *NewAsset =
      AssetTools.CreateAsset(AssetName, PackagePath,
                             UMaterialInstanceConstant::StaticClass(), nullptr);
  UMaterialInstanceConstant *MIC = Cast<UMaterialInstanceConstant>(NewAsset);
  if (!MIC)
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Failed to create MIC"));

  MIC->SetParentEditorOnly(ParentMat);

  // Parse vector parameters
  const TSharedPtr<FJsonObject> *VectorsObj;
  if (Params->TryGetObjectField(TEXT("vector_parameters"), VectorsObj)) {
    for (const auto &Elem : (*VectorsObj)->Values) {
      if (Elem.Value->Type == EJson::String) // Hex color
      {
        FColor HexColor = FColor::FromHex(Elem.Value->AsString());
        MIC->SetVectorParameterValueEditorOnly(
            FMaterialParameterInfo(*Elem.Key), FLinearColor(HexColor));
      }
    }
  }

  // Parse scalar parameters
  const TSharedPtr<FJsonObject> *ScalarsObj;
  if (Params->TryGetObjectField(TEXT("scalar_parameters"), ScalarsObj)) {
    for (const auto &Elem : (*ScalarsObj)->Values) {
      if (Elem.Value->Type == EJson::Number) {
        MIC->SetScalarParameterValueEditorOnly(
            FMaterialParameterInfo(*Elem.Key), Elem.Value->AsNumber());
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

TSharedPtr<FJsonObject>
FUnrealAgenticAssetMutatorCommands::HandleSpawnBlueprintActor(
    const TSharedPtr<FJsonObject> &Params) {
  FString BlueprintName;
  if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName)) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Missing 'blueprint_name' parameter"));
  }

  FString ActorName;
  if (!Params->TryGetStringField(TEXT("actor_name"), ActorName)) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Missing 'actor_name' parameter"));
  }

  const UBlueprint *Blueprint =
      FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
  if (!Blueprint) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
  }

  FVector Location(0.0f, 0.0f, 0.0f);
  FRotator Rotation(0.0f, 0.0f, 0.0f);

  if (Params->HasField(TEXT("location"))) {
    Location =
        FUnrealAgenticCommonUtils::GetVectorFromJson(Params, TEXT("location"));
  }
  if (Params->HasField(TEXT("rotation"))) {
    Rotation =
        FUnrealAgenticCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (!World) {
    return FUnrealAgenticCommonUtils::CreateErrorResponse(
        TEXT("Failed to get editor world"));
  }

  FTransform SpawnTransform;
  SpawnTransform.SetLocation(Location);
  SpawnTransform.SetRotation(FQuat(Rotation));

  // Small delay to let the engine process newly compiled classes
  FPlatformProcess::Sleep(0.2f);

  AActor *NewActor =
      World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform);

  if (NewActor) {
    NewActor->SetActorLabel(*ActorName);
    return FUnrealAgenticCommonUtils::ActorToJsonObject(NewActor, true);
  }

  return FUnrealAgenticCommonUtils::CreateErrorResponse(
      TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FUnrealAgenticAssetMutatorCommands::HandleSetSkeletalMeshProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName)) {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName)) {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint) {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Blueprint not found"));
    }

    const USCS_Node* ComponentNode = nullptr;
    for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes()) {
        if (Node && Node->GetVariableName().ToString() == ComponentName) {
            ComponentNode = Node;
            break;
        }
    }

    USkeletalMeshComponent* MeshComponent = nullptr;
    AActor* CDO = nullptr;

    if (!ComponentNode) {
        if (UClass* ParentClass = Blueprint->ParentClass) {
            UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
            if (BPGC) {
                CDO = Cast<AActor>(BPGC->GetDefaultObject(true));
                if (CDO) {
                    for (UActorComponent* Comp : CDO->GetComponents()) {
                        if (Comp && Comp->GetFName().ToString() == ComponentName) {
                            MeshComponent = Cast<USkeletalMeshComponent>(Comp);
                            break;
                        }
                    }
                }
            }
        }
    } else {
        MeshComponent = Cast<USkeletalMeshComponent>(ComponentNode->ComponentTemplate);
    }

    if (!MeshComponent) {
        UE_LOG(LogTemp, Error, TEXT("SetSkeletalMeshProperties: Component '%s' not found or is not a SkeletalMeshComponent!"), *ComponentName);
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Component not found or is not a SkeletalMeshComponent"));
    }

    Blueprint->Modify();
    MeshComponent->Modify();
    if (CDO) CDO->Modify();
    
    MeshComponent->PreEditChange(nullptr);

    bool bChanged = false;

    if (Params->HasField(TEXT("skeletal_mesh"))) {
        FString MeshPath = Params->GetStringField(TEXT("skeletal_mesh"));
        if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(UEditorAssetLibrary::LoadAsset(MeshPath))) {
            MeshComponent->SetSkeletalMeshAsset(Mesh);
            bChanged = true;
            UE_LOG(LogTemp, Log, TEXT("SetSkeletalMeshProperties: Successfully set skeletal mesh to %s"), *MeshPath);
        } else {
            UE_LOG(LogTemp, Error, TEXT("SetSkeletalMeshProperties: Failed to load SkeletalMesh at %s"), *MeshPath);
            return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load SkeletalMesh at %s"), *MeshPath));
        }
    }

    if (Params->HasField(TEXT("anim_class"))) {
        FString AnimPath = Params->GetStringField(TEXT("anim_class"));
        UClass* AnimClass = nullptr;
        if (AnimPath.EndsWith(TEXT("_C"))) {
            AnimClass = LoadObject<UClass>(nullptr, *AnimPath);
        } else {
            if (UBlueprint* AnimBP = FUnrealAgenticCommonUtils::FindBlueprintByName(AnimPath)) {
                AnimClass = AnimBP->GeneratedClass;
            } else {
                FString ClassPath = AnimPath + TEXT("_C");
                AnimClass = LoadObject<UClass>(nullptr, *ClassPath);
            }
        }
        
        if (AnimClass) {
            MeshComponent->SetAnimInstanceClass(AnimClass);
            bChanged = true;
            UE_LOG(LogTemp, Log, TEXT("SetSkeletalMeshProperties: Successfully set anim class to %s"), *AnimClass->GetName());
        } else {
            UE_LOG(LogTemp, Error, TEXT("SetSkeletalMeshProperties: Failed to load AnimClass %s"), *AnimPath);
            return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load AnimClass at %s"), *AnimPath));
        }
    }

    if (Params->HasField(TEXT("location"))) {
        MeshComponent->SetRelativeLocation(FUnrealAgenticCommonUtils::GetVectorFromJson(Params, TEXT("location")));
        bChanged = true;
    }
    if (Params->HasField(TEXT("rotation"))) {
        MeshComponent->SetRelativeRotation(FUnrealAgenticCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")));
        bChanged = true;
    }
    if (Params->HasField(TEXT("scale"))) {
        MeshComponent->SetRelativeScale3D(FUnrealAgenticCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
        bChanged = true;
    }

    MeshComponent->PostEditChange();

    if (bChanged) {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        // Force the CDO properties to broadcast as updated
        if (CDO) {
            CDO->PostEditChange();
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticAssetMutatorCommands::HandleSetBlueprintClassDefaults(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName)) {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealAgenticCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint || !Blueprint->GeneratedClass) {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Blueprint not found or has no GeneratedClass"));
    }

    UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject(true);
    if (!CDO) {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Could not get ClassDefaultObject"));
    }

    const TSharedPtr<FJsonObject>* PropertiesObj;
    if (Params->TryGetObjectField(TEXT("properties"), PropertiesObj)) {
        for (const auto& Elem : (*PropertiesObj)->Values) {
            FString PropName = Elem.Key;
            FProperty* Property = CDO->GetClass()->FindPropertyByName(*PropName);
            if (Property) {
                if (Elem.Value->Type == EJson::String) {
                    FString ValStr = Elem.Value->AsString();
                    if (FClassProperty* ClassProp = CastField<FClassProperty>(Property)) {
                        UClass* LoadedClass = nullptr;
                        if (ValStr.EndsWith(TEXT("_C"))) {
                            LoadedClass = Cast<UClass>(UEditorAssetLibrary::LoadAsset(ValStr));
                            if (!LoadedClass) LoadedClass = LoadObject<UClass>(nullptr, *ValStr);
                        } else {
                            if (UBlueprint* FoundBP = FUnrealAgenticCommonUtils::FindBlueprintByName(ValStr)) {
                                LoadedClass = FoundBP->GeneratedClass;
                            } else {
                                FString ClassPath = ValStr + TEXT("_C");
                                LoadedClass = Cast<UClass>(UEditorAssetLibrary::LoadAsset(ClassPath));
                            }
                        }
                        if (LoadedClass) {
                            ClassProp->SetObjectPropertyValue_InContainer(CDO, LoadedClass);
                        } else {
                            UE_LOG(LogTemp, Warning, TEXT("Failed to load class %s for property %s"), *ValStr, *PropName);
                        }
                    } else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property)) {
                        UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(ValStr);
                        if (LoadedObj) {
                            ObjProp->SetObjectPropertyValue_InContainer(CDO, LoadedObj);
                        }
                    } else {
                        Property->ImportText_Direct(*ValStr, Property->ContainerPtrToValuePtr<void>(CDO), CDO, 0);
                    }
                } else if (Elem.Value->Type == EJson::Boolean) {
                    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property)) {
                        BoolProp->SetPropertyValue_InContainer(CDO, Elem.Value->AsBool());
                    } else {
                        FString ValStr = Elem.Value->AsBool() ? TEXT("True") : TEXT("False");
                        Property->ImportText_Direct(*ValStr, Property->ContainerPtrToValuePtr<void>(CDO), CDO, 0);
                    }
                } else if (Elem.Value->Type == EJson::Number) {
                    FString ValStr = FString::SanitizeFloat(Elem.Value->AsNumber());
                    Property->ImportText_Direct(*ValStr, Property->ContainerPtrToValuePtr<void>(CDO), CDO, 0);
                }
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}
