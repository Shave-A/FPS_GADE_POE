#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HealthPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class AHealthPickup : public AActor
{
	GENERATED_BODY()

public:
	AHealthPickup();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void TryPickup(AActor* Other);
	bool HealPlayerBlueprint(AActor* Player);
	void SetAvailable(bool bNewAvailable);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Trigger;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, Category = "Pickup") float HealAmount = 25.f;
	UPROPERTY(EditAnywhere, Category = "Pickup", meta = (ClampMin = "0.5")) float RespawnTime = 15.f;
	UPROPERTY(EditAnywhere, Category = "Pickup") float SpinSpeed = 90.f;
	UPROPERTY(EditAnywhere, Category = "Pickup") FLinearColor PickupColor = FLinearColor(0.1f, 1.f, 0.2f);

	// Names of the variables in your PlayerBP (spaces/underscores/case are ignored)
	UPROPERTY(EditAnywhere, Category = "Pickup|Player") FName PlayerHealthVariable = TEXT("CurrentHealth");
	UPROPERTY(EditAnywhere, Category = "Pickup|Player") FName PlayerMaxHealthVariable = TEXT("MaxHealth");
	UPROPERTY(EditAnywhere, Category = "Pickup|Player") float FallbackMaxHealth = 100.f;

	bool bAvailable = true;
	FTimerHandle RespawnTimer;
};