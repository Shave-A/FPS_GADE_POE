#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HealableInterface.h"
#include "BotCharacter.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FBotDiedEvent, class ABotCharacter* /*Bot*/, AActor* /*Killer*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FBotDamagedEvent, AActor* /*Attacker*/);

UCLASS()
class ABotCharacter : public ACharacter, public IHealableInterface
{
    GENERATED_BODY()

public:
    ABotCharacter();

    virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

    // Weapons: default firearm = unlimited ammo, secondary = limited ammo
    bool FireAt(const FVector& AimPoint);
    bool HasUsableSecondary() const { return bHasSecondary && SecondaryAmmo > 0; }
    bool NeedsAmmo() const { return !bHasSecondary || SecondaryAmmo < MaxSecondaryAmmo / 2; }

    // Called by pickups
    virtual float Heal(float Amount) override;      // IHealableInterface
    bool GiveAmmo(int32 Amount);

    /** Collects an actor tagged "AmmoPickup" (e.g. BP_WeaponPickup): gives the secondary firearm + ammo. */
    void CollectPickup(AActor* Pickup);

    void Die(AActor* Killer);
    void Respawn();
    void ResetBot();                 // full health, no secondary (called on respawn)
    void SetRunning(bool bRunning);

    bool IsDead() const { return bDead; }
    float GetHealth() const { return Health; }

    // Observer events: other classes subscribe, the bot knows nothing about them
    FBotDiedEvent OnBotDied;
    FBotDamagedEvent OnBotDamaged;

    /** Name shown on the leaderboard. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bot")
    FString DisplayName;

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnCapsuleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    bool PickRespawnTransform(FVector& OutLocation, FRotator& OutRotation) const;

    // ---- Health ----
    UPROPERTY(EditAnywhere, Category = "Bot|Health") float MaxHealth = 100.f;
    UPROPERTY(VisibleInstanceOnly, Category = "Bot|Health") float Health = 100.f;

    // ---- Weapons ----
    UPROPERTY(EditAnywhere, Category = "Bot|Weapons") float DefaultDamage = 6.f;
    UPROPERTY(EditAnywhere, Category = "Bot|Weapons") float DefaultFireInterval = 0.6f;
    UPROPERTY(EditAnywhere, Category = "Bot|Weapons") float SecondaryDamage = 8.f;
    UPROPERTY(EditAnywhere, Category = "Bot|Weapons") float SecondaryFireInterval = 0.25f;
    UPROPERTY(EditAnywhere, Category = "Bot|Weapons") float WeaponRange = 5000.f;
    UPROPERTY(EditAnywhere, Category = "Bot|Weapons") float SpreadDegrees = 5.f;
    UPROPERTY(EditAnywhere, Category = "Bot|Weapons") int32 BurstSize = 3;
    UPROPERTY(EditAnywhere, Category = "Bot|Weapons") float BurstPause = 0.9f;
    UPROPERTY(EditAnywhere, Category = "Bot|Weapons") int32 MaxSecondaryAmmo = 60;

    UPROPERTY(VisibleInstanceOnly, Category = "Bot|Weapons") int32 SecondaryAmmo = 0;
    UPROPERTY(VisibleInstanceOnly, Category = "Bot|Weapons") bool bHasSecondary = false;

    // ---- Movement ----
    UPROPERTY(EditAnywhere, Category = "Bot|Movement") float WalkSpeed = 300.f;
    UPROPERTY(EditAnywhere, Category = "Bot|Movement") float RunSpeed = 550.f;

    // ---- Pickups ----
    UPROPERTY(EditAnywhere, Category = "Bot|Pickups") int32 AmmoPickupAmount = 30;
    UPROPERTY(EditAnywhere, Category = "Bot|Pickups") float PickupReappearTime = 15.f;

    // ---- Respawn ----
    UPROPERTY(EditAnywhere, Category = "Bot|Respawn") float RespawnDelay = 20.f;
    UPROPERTY(EditAnywhere, Category = "Bot|Respawn") FName RespawnTag = TEXT("RespawnPoint");

    FTimerHandle RespawnTimer;
    FVector StartLocation = FVector::ZeroVector;
    FRotator StartRotation = FRotator::ZeroRotator;

    bool bDead = false;
    float NextFireTime = 0.f;
    int32 ShotsInBurst = 0;
};