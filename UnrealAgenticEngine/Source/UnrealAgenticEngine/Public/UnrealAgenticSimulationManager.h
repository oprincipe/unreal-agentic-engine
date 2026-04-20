#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "TickableEditorObject.h"
#include "Dom/JsonObject.h"
#include "UnrealAgenticSimulationManager.generated.h"

UCLASS()
class UNREALAGENTICENGINE_API UUnrealAgenticSimulationManager : public UEditorSubsystem, public FTickableEditorObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // FTickableEditorObject interface
    virtual void Tick(float DeltaTime) override;
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
    virtual TStatId GetStatId() const override;

    void StartSimulation(const FString& InAssetPath, float InMaxTimeLimit);
    bool IsSimulationRunning() const { return bIsRunning; }
    TSharedPtr<FJsonObject> GetSimulationResults() const;

private:
    void OnPIEStarted(bool bIsSimulating);
    void OnPIEEnded(bool bIsSimulating);

    bool bIsRunning;
    bool bHasFinished;
    float RunningTime;
    float MaxTimeLimit;

    FString AssetPath;
    
    // Metrics
    FVector LastKnownActorLocation;
    float TotalDistanceTraveled;
    float TimeStuck;

    TWeakObjectPtr<class AActor> TrackedActor;
    TSharedPtr<FJsonObject> FinalResults;
    
    FDelegateHandle PIEStartedHandle;
    FDelegateHandle PIEEndedHandle;
};
