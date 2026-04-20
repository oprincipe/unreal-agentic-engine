#include "UnrealAgenticSimulationManager.h"
#include "Editor.h"
#include "EditorSubsystem.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "GameFramework/Character.h"
#include "Dom/JsonObject.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/App.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"

void UUnrealAgenticSimulationManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bIsRunning = false;
    bHasFinished = false;

    PIEStartedHandle = FEditorDelegates::PostPIEStarted.AddUObject(this, &UUnrealAgenticSimulationManager::OnPIEStarted);
    PIEEndedHandle = FEditorDelegates::EndPIE.AddUObject(this, &UUnrealAgenticSimulationManager::OnPIEEnded);
}

void UUnrealAgenticSimulationManager::Deinitialize()
{
    FEditorDelegates::PostPIEStarted.Remove(PIEStartedHandle);
    FEditorDelegates::EndPIE.Remove(PIEEndedHandle);
    Super::Deinitialize();
}

TStatId UUnrealAgenticSimulationManager::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UUnrealAgenticSimulationManager, STATGROUP_Tickables);
}

void UUnrealAgenticSimulationManager::StartSimulation(const FString& InAssetPath, float InMaxTimeLimit)
{
    AssetPath = InAssetPath;
    MaxTimeLimit = InMaxTimeLimit;
    bIsRunning = false;
    bHasFinished = false;
    FinalResults.Reset();

    // Fixed timestep for deterministic simulation
    FApp::SetFixedDeltaTime(0.0333f);

    FRequestPlaySessionParams PlayParams;
    GEditor->RequestPlaySession(PlayParams);
}

void UUnrealAgenticSimulationManager::OnPIEStarted(bool bIsSimulating)
{
    if (AssetPath.IsEmpty() || bHasFinished) return;

    UWorld* CurrentWorld = GEditor->PlayWorld;
    if (!CurrentWorld) return;

    // 1. Ensure NavMesh for Behavior Tree "Move To" compatibility
    ANavMeshBoundsVolume* NavVolume = CurrentWorld->SpawnActor<ANavMeshBoundsVolume>(ANavMeshBoundsVolume::StaticClass());
    if (NavVolume)
    {
        NavVolume->SetActorScale3D(FVector(100.f, 100.f, 10.f));
        UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(CurrentWorld);
        if (NavSys)
        {
            NavSys->Build();
        }
    }

    // 2. Spawn AI Floor (in case PIE is empty space)
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AStaticMeshActor* Floor = CurrentWorld->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    if (Floor && Floor->GetStaticMeshComponent())
    {
        Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube")));
        Floor->SetActorScale3D(FVector(50.f, 50.f, 0.1f));
    }

    // 3. Spawn Test Character
    ACharacter* Character = CurrentWorld->SpawnActor<ACharacter>(ACharacter::StaticClass(), FVector(0,0,100), FRotator::ZeroRotator, SpawnParams);
    if (Character)
    {
        TrackedActor = Character;
        LastKnownActorLocation = Character->GetActorLocation();
        
        AAIController* AIController = CurrentWorld->SpawnActor<AAIController>(AAIController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (AIController)
        {
            AIController->Possess(Character);
            
            UBehaviorTree* BTAsset = Cast<UBehaviorTree>(StaticLoadObject(UBehaviorTree::StaticClass(), nullptr, *AssetPath));
            if (BTAsset)
            {
                AIController->RunBehaviorTree(BTAsset);
            }
        }
    }

    bIsRunning = true;
    RunningTime = 0.0f;
    TotalDistanceTraveled = 0.0f;
    TimeStuck = 0.0f;
    
    UE_LOG(LogTemp, Display, TEXT("Agentic Simulation Started for %s"), *AssetPath);
}

void UUnrealAgenticSimulationManager::Tick(float DeltaTime)
{
    if (!bIsRunning || !GEditor || !GEditor->PlayWorld) return;

    RunningTime += DeltaTime;

    if (TrackedActor.IsValid())
    {
        FVector CurrentLocation = TrackedActor->GetActorLocation();
        float FrameDistance = FVector::Dist(CurrentLocation, LastKnownActorLocation);
        
        TotalDistanceTraveled += FrameDistance;
        
        if (FrameDistance < 0.1f)
        {
            TimeStuck += DeltaTime;
        }

        LastKnownActorLocation = CurrentLocation;
    }

    if (RunningTime >= MaxTimeLimit)
    {
        UE_LOG(LogTemp, Display, TEXT("Agentic Simulation Reached Time Limit (%.2fs). Stopping..."), RunningTime);
        bIsRunning = false;
        GEditor->RequestEndPlayMap();
    }
}

void UUnrealAgenticSimulationManager::OnPIEEnded(bool bIsSimulating)
{
    // The PIE Ended delegate fires when PIE map completely winds down.
    // Ensure we trigger our json report generation.
    if (!bHasFinished && !AssetPath.IsEmpty())
    {
        bIsRunning = false;
        bHasFinished = true;
        
        FApp::SetFixedDeltaTime(0.0); // Reset to dynamic framerate

        FinalResults = MakeShared<FJsonObject>();
        FinalResults->SetBoolField(TEXT("success"), true);
        
        TSharedPtr<FJsonObject> MetricsObj = MakeShared<FJsonObject>();
        MetricsObj->SetNumberField(TEXT("time_simulated"), RunningTime);
        MetricsObj->SetNumberField(TEXT("distance_traveled"), TotalDistanceTraveled);
        MetricsObj->SetNumberField(TEXT("time_stuck"), TimeStuck);
        
        FinalResults->SetObjectField(TEXT("metrics"), MetricsObj);

        UE_LOG(LogTemp, Display, TEXT("Agentic Simulation Processed Results. Distance: %f, Stuck: %f"), TotalDistanceTraveled, TimeStuck);

        AssetPath.Empty(); // clear for next run
    }
}

TSharedPtr<FJsonObject> UUnrealAgenticSimulationManager::GetSimulationResults() const
{
    return FinalResults;
}
