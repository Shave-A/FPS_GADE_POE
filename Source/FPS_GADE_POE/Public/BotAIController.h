#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BotAIController.generated.h"

class ABotCharacter;
class UBotState;

/** Identifies each bot state. The behaviour itself lives in the state classes (BotStates.h). */
UENUM()
enum class EBotState : uint8 { Patrol, Attack, Evade, SeekHealth, SeekAmmo, Hunt, Search, Dead };

/**
 * Bot brain using the State pattern.
 * The controller owns one object per state and forwards Think/Tick to whichever state is active.
 * It also provides the shared senses (sight, pickups) and memory that the states read.
 */
UCLASS()
class ABotAIController : public AAIController
{
    GENERATED_BODY()

public:
    ABotAIController();

    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void Tick(float DeltaSeconds) override;

    void ReactToDamage(AActor* Attacker);

    // ---------------------------------------------------------------- state machine
    /** Exit the current state, enter the new one. Does nothing if already in that state. */
    void ChangeState(EBotState NewState);
    EBotState GetState() const;

    // ---------------------------------------------------------------- shared senses (used by the states)
    ABotCharacter* GetBot() const { return Bot; }
    AActor* FindVisibleTarget();
    AActor* FindVisiblePickup(const FName& Tag);
    APawn* GetHuntTargetIfLastBot() const;
    bool IsMoving() const;
    bool IsAwareOfAttacker() const;
    AActor* GetAwarenessTarget() const { return AwarenessTarget.Get(); }
    void ClearMemory();

    // ---------------------------------------------------------------- shared memory (used by the states)
    TWeakObjectPtr<AActor> CurrentTarget;
    TWeakObjectPtr<AActor> CurrentPickup;
    FVector LastKnownTargetLocation = FVector::ZeroVector;
    float PickupCommitUntil = 0.f;
    TMap<TWeakObjectPtr<AActor>, float> IgnoredPickups;   // pickups it could not path to, ignored for a while

    // ---------------------------------------------------------------- tuning (read by the states)
    UPROPERTY(EditAnywhere, Category = "Bot|Rules") float LowHealth = 25.f;
    UPROPERTY(EditAnywhere, Category = "Bot|Rules") float SeekHealthBelow = 75.f;

    UPROPERTY(EditAnywhere, Category = "Bot|Combat") float PreferredRange = 1500.f;
    UPROPERTY(EditAnywhere, Category = "Bot|Combat") float ReactionTime = 0.6f;
    UPROPERTY(EditAnywhere, Category = "Bot|Combat") float FireConeDeg = 12.f;

    UPROPERTY(EditAnywhere, Category = "Bot|Movement") float PatrolRadius = 3500.f;
    UPROPERTY(EditAnywhere, Category = "Bot|Movement") float EvadeDistance = 2200.f;
    UPROPERTY(EditAnywhere, Category = "Bot|Movement") float HuntRepathInterval = 1.5f;
    UPROPERTY(EditAnywhere, Category = "Bot|Movement") float SearchDuration = 6.f;

    UPROPERTY(EditAnywhere, Category = "Bot|Tags") FName HealthPickupTag = TEXT("HealthPickup");
    UPROPERTY(EditAnywhere, Category = "Bot|Tags") FName AmmoPickupTag = TEXT("AmmoPickup");

protected:
    UPROPERTY(EditAnywhere, Category = "Bot|Senses") float ThinkInterval = 0.2f;
    UPROPERTY(EditAnywhere, Category = "Bot|Senses") float SightRange = 3500.f;
    UPROPERTY(EditAnywhere, Category = "Bot|Senses") float SightHalfAngle = 75.f;
    UPROPERTY(EditAnywhere, Category = "Bot|Senses") float PickupSightRange = 3000.f;
    UPROPERTY(EditAnywhere, Category = "Bot|Senses") float CloseSenseRange = 900.f;

private:
    void Think();
    void CreateStates();
    bool IsValidTarget(const APawn* Candidate) const;
    bool CanSee(const AActor* Other, float Range, float HalfAngleDeg) const;

    UPROPERTY() TObjectPtr<ABotCharacter> Bot;

    /** One object per state, created once on possess. */
    UPROPERTY() TMap<EBotState, TObjectPtr<UBotState>> States;

    /** The state currently in control. */
    UPROPERTY() TObjectPtr<UBotState> CurrentState;

    TWeakObjectPtr<AActor> AwarenessTarget;
    FTimerHandle ThinkTimer;
    float AwarenessExpire = 0.f;
};