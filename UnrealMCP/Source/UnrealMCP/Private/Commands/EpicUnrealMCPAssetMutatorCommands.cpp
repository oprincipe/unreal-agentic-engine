#include "Commands/EpicUnrealMCPAssetMutatorCommands.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
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
#include "EpicUnrealMCPModule.h"
#include "IAssetTools.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Sound/SoundBase.h"


TSharedPtr<FJsonObject> FEpicUnrealMCPAssetMutatorCommands::HandleCommand(
    const FString &CommandType, const TSharedPtr<FJsonObject> &Params) {
  if (CommandType == TEXT("set_physics_properties"))
    return HandleSetPhysicsProperties(Params);
  if (CommandType == TEXT("set_static_mesh_properties"))
    return HandleSetStaticMeshProperties(Params);
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
FEpicUnrealMCPAssetMutatorCommands::HandleSetPhysicsProperties(
    const TSharedPtr<FJsonObject> &Params) {
  FString BlueprintName;
  if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName)) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Missing 'blueprint_name' parameter"));
  }

  FString ComponentName;
  if (!Params->TryGetStringField(TEXT("component_name"), ComponentName)) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Missing 'component_name' parameter"));
  }

  UBlueprint *Blueprint =
      FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
  if (!Blueprint) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
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
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Component not found: %s"), *ComponentName));
  }

  UPrimitiveComponent *PrimComponent =
      Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
  if (!PrimComponent) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
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
FEpicUnrealMCPAssetMutatorCommands::HandleSetStaticMeshProperties(
    const TSharedPtr<FJsonObject> &Params) {
  FString BlueprintName;
  if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName)) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Missing 'blueprint_name' parameter"));
  }

  FString ComponentName;
  if (!Params->TryGetStringField(TEXT("component_name"), ComponentName)) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Missing 'component_name' parameter"));
  }

  UBlueprint *Blueprint =
      FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
  if (!Blueprint) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
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
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Component not found: %s"), *ComponentName));
  }

  UStaticMeshComponent *MeshComponent =
      Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
  if (!MeshComponent) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
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
        FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
  }
  if (Params->HasField(TEXT("rotation"))) {
    MeshComponent->SetRelativeRotation(
        FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params,
                                                      TEXT("rotation")));
  }
  if (Params->HasField(TEXT("scale"))) {
    MeshComponent->SetRelativeScale3D(
        FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
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
FEpicUnrealMCPAssetMutatorCommands::HandleSetMeshMaterialColor(
    const TSharedPtr<FJsonObject> &Params) {
  FString BlueprintName;
  if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName)) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Missing 'blueprint_name' parameter"));
  }

  FString ComponentName;
  if (!Params->TryGetStringField(TEXT("component_name"), ComponentName)) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Missing 'component_name' parameter"));
  }

  UBlueprint *Blueprint =
      FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
  if (!Blueprint) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
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
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Component not found: %s"), *ComponentName));
  }

  UPrimitiveComponent *PrimComponent =
      Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
  if (!PrimComponent) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
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
      return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
          FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
    }
  } else {
    Material = PrimComponent->GetMaterial(MaterialSlot);
    if (!Material) {
      Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(
          TEXT("/Engine/BasicShapes/BasicShapeMaterial")));
      if (!Material) {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("No material found on component and failed to load default "
                 "material"));
      }
    }
  }

  if (bHasColor) {
    UMaterialInstanceDynamic *DynMaterial =
        UMaterialInstanceDynamic::Create(Material, PrimComponent);
    if (!DynMaterial) {
      return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
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
FEpicUnrealMCPAssetMutatorCommands::HandleDuplicateAsset(
    const TSharedPtr<FJsonObject> &Params) {
  FString SourcePath, DestPath;
  if (!Params->TryGetStringField(TEXT("source_path"), SourcePath) ||
      !Params->TryGetStringField(TEXT("dest_path"), DestPath))
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Missing 'source_path' or 'dest_path'"));

  const UObject *Duplicated =
      UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);
  if (!Duplicated)
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("DuplicateAsset failed"));

  TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
  ResultObj->SetStringField(TEXT("asset_path"), Duplicated->GetPathName());
  ResultObj->SetBoolField(TEXT("success"), true);
  return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAssetMutatorCommands::HandleDestroyActor(
    const TSharedPtr<FJsonObject> &Params) {
  FString ActorName;
  if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Missing 'actor_name' parameter"));

  const UWorld *World =
      GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!World)
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
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
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Actor not found"));

  TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
  ResultObj->SetBoolField(TEXT("success"), true);
  return ResultObj;
}

TSharedPtr<FJsonObject>
FEpicUnrealMCPAssetMutatorCommands::HandleStartPlayInEditor(
    const TSharedPtr<FJsonObject> &Params) {
  if (!GEditor)
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("GEditor not valid"));

  if (GEditor->PlayWorld)
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Already in PIE"));

  const FRequestPlaySessionParams PlayParams;
  GEditor->RequestPlaySession(PlayParams);

  TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
  ResultObj->SetBoolField(TEXT("success"), true);
  return ResultObj;
}

TSharedPtr<FJsonObject>
FEpicUnrealMCPAssetMutatorCommands::HandleStopPlayInEditor(
    const TSharedPtr<FJsonObject> &Params) {
  if (!GEditor)
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("GEditor not valid"));

  if (!GEditor->PlayWorld)
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Not currently in PIE"));

  GEditor->RequestEndPlayMap();

  TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
  ResultObj->SetBoolField(TEXT("success"), true);
  return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAssetMutatorCommands::HandleGetEditorLogs(
    const TSharedPtr<FJsonObject> &Params) {
  TArray<FString> Logs = FEpicUnrealMCPModule::Get().GetRecentLogs();

  bool bClear = false;
  Params->TryGetBoolField(TEXT("clear_after_read"), bClear);
  if (bClear)
    FEpicUnrealMCPModule::Get().ClearLogs();

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
FEpicUnrealMCPAssetMutatorCommands::HandleCreateMaterialInstance(
    const TSharedPtr<FJsonObject> &Params) {
  FString ParentPath, DestPath;
  if (!Params->TryGetStringField(TEXT("parent_path"), ParentPath) ||
      !Params->TryGetStringField(TEXT("dest_path"), DestPath))
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Missing 'parent_path' or 'dest_path'"));

  UMaterialInterface *ParentMat =
      Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(ParentPath));
  if (!ParentMat)
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
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
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
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
FEpicUnrealMCPAssetMutatorCommands::HandleSpawnBlueprintActor(
    const TSharedPtr<FJsonObject> &Params) {
  FString BlueprintName;
  if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName)) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Missing 'blueprint_name' parameter"));
  }

  FString ActorName;
  if (!Params->TryGetStringField(TEXT("actor_name"), ActorName)) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        TEXT("Missing 'actor_name' parameter"));
  }

  const UBlueprint *Blueprint =
      FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
  if (!Blueprint) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
  }

  FVector Location(0.0f, 0.0f, 0.0f);
  FRotator Rotation(0.0f, 0.0f, 0.0f);

  if (Params->HasField(TEXT("location"))) {
    Location =
        FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
  }
  if (Params->HasField(TEXT("rotation"))) {
    Rotation =
        FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (!World) {
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
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
    return FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
  }

  return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
      TEXT("Failed to spawn blueprint actor"));
}
