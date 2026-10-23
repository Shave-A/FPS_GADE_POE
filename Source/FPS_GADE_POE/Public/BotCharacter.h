#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BotCharacter.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FBotDiedEvent, class ABotCharacter* /*Bot*/, AActor* /*Killer*/);

UCLASS()
class ABotCharacter : public ACharacter
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
	float Heal(float Amount);
	bool GiveAmmo(int32 Amount);

	void ResetBot();                 // full health, no secondary (call on respawn)
	void SetRunning(bool bRunning);

	bool IsDead() const { return bDead; }
	void Die(AActor* Killer);
	float GetHealth() const { return Health; }

	FBotDiedEvent OnBotDied;

protected:
	UPROPERTY(EditAnywhere, Category = "Bot|Health") float MaxHealth = 100.f;
	UPROPERTY(VisibleInstanceOnly, Category = "Bot|Health") float Health = 100.f;

	UPROPERTY(EditAnywhere, Category = "Bot|Weapons") float DefaultDamage = 8.f;
	UPROPERTY(EditAnywhere, Category = "Bot|Weapons") float DefaultFireInterval = 0.4f;
	UPROPERTY(EditAnywhere, Category = "Bot|Weapons") float SecondaryDamage = 14.f;
	UPROPERTY(EditAnywhere, Category = "Bot|Weapons") float SecondaryFireInterval = 0.12f;
	UPROPERTY(EditAnywhere, Category = "Bot|Weapons") float WeaponRange = 5000.f;
	UPROPERTY(EditAnywhere, Category = "Bot|Weapons") float SpreadDegrees = 3.f;
	UPROPERTY(EditAnywhere, Category = "Bot|Weapons") int32 MaxSecondaryAmmo = 60;

	UPROPERTY(EditAnywhere, Category = "Bot|Movement") float WalkSpeed = 300.f;
	UPROPERTY(EditAnywhere, Category = "Bot|Movement") float RunSpeed = 550.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Bot|Weapons") int32 SecondaryAmmo = 0;
	UPROPERTY(VisibleInstanceOnly, Category = "Bot|Weapons") bool bHasSecondary = false;

	bool bDead = false;
	float NextFireTime = 0.f;
};