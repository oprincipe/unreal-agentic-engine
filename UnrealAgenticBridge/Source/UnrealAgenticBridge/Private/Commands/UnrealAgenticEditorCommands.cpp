#include "Commands/UnrealAgenticEditorCommands.h"
#include "Commands/UnrealAgenticCommonUtils.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "Commands/UnrealAgenticAssetMutatorCommands.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EditorAssetLibrary.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

FUnrealAgenticEditorCommands::FUnrealAgenticEditorCommands()
{
}

TSharedPtr<FJsonObject> FUnrealAgenticEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation commands
    if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("find_actors_by_name"))
    {
        return HandleFindActorsByName(Params);
    }
    else if (CommandType == TEXT("spawn_actor"))
    {
        return HandleSpawnActor(Params);
    }
    else if (CommandType == TEXT("delete_actor"))
    {
        return HandleDeleteActor(Params);
    }
    else if (CommandType == TEXT("set_actor_color"))
    {
        return HandleSetActorColor(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    else if (CommandType == TEXT("set_actor_material"))
    {
        return HandleSetActorMaterial(Params);
    }
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    
    return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealAgenticEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FUnrealAgenticCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FUnrealAgenticCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealAgenticEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealAgenticCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealAgenticCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealAgenticCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    // IMPORTANT: Do NOT force SpawnParams.Name. If a recently deleted actor with the same Name 
    // is pending GC, forcing the Title/Name here causes a Fatal Engine Crash.
    // Instead, let Unreal generate a unique internal name, then set the user-facing Editor Label.

    if (ActorType == TEXT("StaticMeshActor"))
    {
        AStaticMeshActor* NewMeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
        if (NewMeshActor)
        {
            // Check for an optional static_mesh parameter to assign a mesh
            FString MeshPath;
            if (Params->TryGetStringField(TEXT("static_mesh"), MeshPath))
            {
                UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
                if (Mesh)
                {
                    NewMeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Could not find static mesh at path: %s"), *MeshPath);
                }
            }
        }
        NewActor = NewMeshActor;
    }
    else if (ActorType == TEXT("PointLight"))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("Blueprint"))
    {
        FString BlueprintPath;
        if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
        {
            return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter for Blueprint actor type"));
        }
        
        UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(BlueprintPath);
        if (!LoadedObj)
        {
            return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load blueprint at path: %s"), *BlueprintPath));
        }

        UBlueprint* LoadedBP = Cast<UBlueprint>(LoadedObj);
        if (!LoadedBP || !LoadedBP->GeneratedClass)
        {
            return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Loaded asset is not a valid Blueprints class"));
        }

        NewActor = World->SpawnActor<AActor>(LoadedBP->GeneratedClass, Location, Rotation, SpawnParams);
    }
    else
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
    }

    if (NewActor)
    {
        // Safely set the Editor-facing name (Outliner label) without risking GC naming crashes
#if WITH_EDITOR
        NewActor->SetActorLabel(ActorName);
#endif

        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Return the created actor's details
        return FUnrealAgenticCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FUnrealAgenticEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FUnrealAgenticCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
            Actor->Destroy();
            
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }
    
    return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FUnrealAgenticEditorCommands::HandleSetActorColor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName, ColorHex;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("color_hex"), ColorHex))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'color_hex' parameter"));
    }

    // Convert hex string (e.g., "#FF0000" or "FF0000") to FColor
    FColor Color = FColor::FromHex(ColorHex);
    FLinearColor LinearColor = FLinearColor(Color);

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            // Find any MeshComponent (StaticMeshComponent etc)
            TArray<UMeshComponent*> MeshComponents;
            Actor->GetComponents<UMeshComponent>(MeshComponents);

            if (MeshComponents.Num() == 0)
            {
                return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Actor does not have any mesh components to color"));
            }

            int32 PaintedCount = 0;
            for (UMeshComponent* MeshComp : MeshComponents)
            {
                if (!MeshComp) continue;

                // Color all materials on the mesh
                const int32 NumMaterials = MeshComp->GetNumMaterials();
                for (int32 MatIndex = 0; MatIndex < NumMaterials; ++MatIndex)
                {
                    UMaterialInterface* BaseMaterial = MeshComp->GetMaterial(MatIndex);
                    if (!BaseMaterial) continue;

                    UMaterialInstanceDynamic* DynamicMat = Cast<UMaterialInstanceDynamic>(BaseMaterial);
                    bool bWasCreated = false;
                    
                    if (!DynamicMat)
                    {
                        // Create dynamic material instance
                        DynamicMat = UMaterialInstanceDynamic::Create(BaseMaterial, MeshComp);
                        bWasCreated = true;
                    }
                    
                    if (DynamicMat)
                    {
                        // Some basic shape materials use "Color" or "BaseColor"
                        DynamicMat->SetVectorParameterValue(TEXT("Color"), LinearColor);
                        DynamicMat->SetVectorParameterValue(TEXT("BaseColor"), LinearColor);
                        
                        if (bWasCreated)
                        {
                            MeshComp->SetMaterial(MatIndex, DynamicMat);
                        }
                        PaintedCount++;
                    }
                }
            }

            if (PaintedCount > 0)
            {
                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetStringField(TEXT("status"), TEXT("success"));
                ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Applied color %s to %d materials on %s"), *ColorHex, PaintedCount, *ActorName));
                return ResultObj;
            }
            else
            {
                return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("No compatible materials found to apply color"));
            }
        }
    }

    return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FUnrealAgenticEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FUnrealAgenticCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FUnrealAgenticCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FUnrealAgenticCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FUnrealAgenticCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealAgenticEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // Forward to the AssetMutator subsystem
    FUnrealAgenticAssetMutatorCommands AssetMutatorCommands;
    return AssetMutatorCommands.HandleCommand(TEXT("spawn_blueprint_actor"), Params);
}

TSharedPtr<FJsonObject> FUnrealAgenticEditorCommands::HandleSetActorMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName, MaterialPath;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
        return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));

    UMaterialInterface* MaterialAsset = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!MaterialAsset)
        return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load material asset at: %s"), *MaterialPath));

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            TArray<UMeshComponent*> MeshComponents;
            Actor->GetComponents<UMeshComponent>(MeshComponents);

            if (MeshComponents.Num() == 0)
                return FUnrealAgenticCommonUtils::CreateErrorResponse(TEXT("Actor does not have any mesh components to apply material to"));

            int32 PaintedCount = 0;
            for (UMeshComponent* MeshComp : MeshComponents)
            {
                if (!MeshComp) continue;

                const int32 NumMaterials = MeshComp->GetNumMaterials();
                for (int32 MatIndex = 0; MatIndex < NumMaterials; ++MatIndex)
                {
                    MeshComp->SetMaterial(MatIndex, MaterialAsset);
                    PaintedCount++;
                }
            }

            if (PaintedCount > 0)
            {
                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetStringField(TEXT("status"), TEXT("success"));
                ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Applied material %s to %d slots on %s"), *MaterialPath, PaintedCount, *ActorName));
                return ResultObj;
            }
        }
    }
    
    return FUnrealAgenticCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}
