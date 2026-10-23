#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BotAIController.generated.h"

class ABotCharacter;

UENUM()
enum class EBotState : uint8 { Patrol, Attack, Evade, SeekHealth, SeekAmmo };

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
	EBotState GetState() const { return State; }

protected:
	void Think();
	void SetState(EBotState NewState);

	AActor* FindVisibleTarget();
	AActor* FindVisiblePickup(const FName& Tag);
	bool IsValidTarget(const APawn* Candidate) const;
	bool CanSee(const AActor* Other, float Range, float HalfAngleDeg) const;
	bool IsMoving() const;

	void DoAttack(AActor* Target);
	void DoEvade(AActor* Threat, AActor* HealthPickup);
	void DoSeekPickup(AActor* Pickup, EBotState SeekState);
	void DoPatrol();
	void TickCombat();

	// Brief values
	UPROPERTY(EditAnywhere, Category = "Bot|Rules") float LowHealth = 25.f;
	UPROPERTY(EditAnywhere, Category = "Bot|Rules") float SeekHealthBelow = 75.f;

	UPROPERTY(EditAnywhere, Category = "Bot|Senses") float ThinkInterval = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Bot|Senses") float SightRange = 3500.f;
	UPROPERTY(EditAnywhere, Category = "Bot|Senses") float SightHalfAngle = 75.f;
	UPROPERTY(EditAnywhere, Category = "Bot|Senses") float PickupSightRange = 3000.f;

	UPROPERTY(EditAnywhere, Category = "Bot|Combat") float PreferredRange = 1500.f;
	UPROPERTY(EditAnywhere, Category = "Bot|Combat") float ReactionTime = 0.35f;
	UPROPERTY(EditAnywhere, Category = "Bot|Combat") float FireConeDeg = 20.f;

	UPROPERTY(EditAnywhere, Category = "Bot|Movement") float PatrolRadius = 3500.f;
	UPROPERTY(EditAnywhere, Category = "Bot|Movement") float EvadeDistance = 2200.f;

	UPROPERTY(EditAnywhere, Category = "Bot|Tags") FName HealthPickupTag = TEXT("HealthPickup");
	UPROPERTY(EditAnywhere, Category = "Bot|Tags") FName AmmoPickupTag = TEXT("AmmoPickup");

private:
	EBotState State = EBotState::Patrol;

	UPROPERTY() TObjectPtr<ABotCharacter> Bot;

	TWeakObjectPtr<AActor> CurrentTarget;
	TWeakObjectPtr<AActor> CurrentPickup;
	TWeakObjectPtr<AActor> AwarenessTarget;
    TMap<TWeakObjectPtr<AActor>, float> IgnoredPickups;   // pickups it could not path to, ignored for a while

	FTimerHandle ThinkTimer;
	float AwarenessExpire = 0.f;
	float PickupCommitUntil = 0.f;
	float FireAllowedTime = 0.f;
	float NextEvadeRepath = 0.f;
	float PatrolTimeout = 0.f;
	float StrafeFlipTime = 0.f;
	float StrafeDir = 1.f;
};