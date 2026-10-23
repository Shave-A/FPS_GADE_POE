#include "BotCharacter.h"
#include "BotAIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "DrawDebugHelpers.h"
#include "AIController.h"
#include "TimerManager.h" 
#include "Components/CapsuleComponent.h"
    
void ABotCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Safety net: if the Blueprint lost its AI controller, create one
    if (!GetController())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s had no controller, spawning one."), *GetName());
        if (!AIControllerClass) { AIControllerClass = ABotAIController::StaticClass(); }
        SpawnDefaultController();
    }

    GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &ABotCharacter::OnCapsuleOverlap);
}

void ABotCharacter::OnCapsuleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    CollectPickup(OtherActor);
}

void ABotCharacter::CollectPickup(AActor* Pickup)
{
    if (bDead || !Pickup || Pickup->IsHidden() || !Pickup->ActorHasTag(TEXT("AmmoPickup")) || !NeedsAmmo())
    {
        return;
    }

    GiveAmmo(AmmoPickupAmount);

    // Hide it like a collected pickup, then bring it back after a delay
    Pickup->SetActorHiddenInGame(true);
    Pickup->SetActorEnableCollision(false);

    TWeakObjectPtr<AActor> Weak = Pickup;
    FTimerHandle Handle;
    GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Pickup, [Weak]()
        {
            if (AActor* P = Weak.Get())
            {
                P->SetActorHiddenInGame(false);
                P->SetActorEnableCollision(true);
            }
        }), PickupReappearTime, false);
}
ABotCharacter::ABotCharacter()
{
	AIControllerClass = ABotAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	bUseControllerRotationYaw = true;                       // body follows the AI's aim
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

float ABotCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead) return 0.f;

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Applied <= 0.f) return 0.f;

	Health = FMath::Clamp(Health - Applied, 0.f, MaxHealth);
	AActor* Attacker = EventInstigator ? EventInstigator->GetPawn() : DamageCauser;

	if (Health <= 0.f)
	{
		Die(Attacker);
		return Applied;
	}

	if (ABotAIController* AI = Cast<ABotAIController>(GetController()))
	{
        OnBotDamaged.Broadcast(Attacker);
	}
	return Applied;
}

void ABotCharacter::Die(AActor* Killer)
{
	bDead = true;

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	SetActorEnableCollision(false);   // can't be shot or block anyone
	SetActorHiddenInGame(true);       // gone from the arena

	OnBotDied.Broadcast(this, Killer);
}

bool ABotCharacter::FireAt(const FVector& AimPoint)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (bDead || Now < NextFireTime) return false;

	// Secondary firearm if it has ammo, otherwise the default firearm
	const bool bSecondary = HasUsableSecondary();
	if (bSecondary) { --SecondaryAmmo; }
	NextFireTime = Now + (bSecondary ? SecondaryFireInterval : DefaultFireInterval);
	const float Damage = bSecondary ? SecondaryDamage : DefaultDamage;

	FVector Start; FRotator Rot;
	GetActorEyesViewPoint(Start, Rot);
	FVector Dir = (AimPoint - Start).GetSafeNormal();
	Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(SpreadDegrees));
	const FVector End = Start + Dir * WeaponRange;

	// Object-type trace so shots hit pawn capsules whatever their Visibility response is
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BotFire), false, this);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params);

#if ENABLE_DRAW_DEBUG
	DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End,
		bSecondary ? FColor::Orange : FColor::Yellow, false, 0.08f, 0, 2.f);
#endif

	if (bHit && Hit.GetActor())
	{
		UGameplayStatics::ApplyDamage(Hit.GetActor(), Damage, GetController(), this, UDamageType::StaticClass());
	}
	return true;
}

float ABotCharacter::Heal(float Amount)
{
	if (bDead) return 0.f;
	const float Before = Health;
	Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
	return Health - Before;
}

bool ABotCharacter::GiveAmmo(int32 Amount)
{
	bHasSecondary = true;                 // picking up ammo also gives the secondary firearm
	SecondaryAmmo = FMath::Clamp(SecondaryAmmo + Amount, 0, MaxSecondaryAmmo);
	return true;
}

void ABotCharacter::ResetBot()
{
	bDead = false;
	Health = MaxHealth;
	bHasSecondary = false;
	SecondaryAmmo = 0;
	NextFireTime = 0.f;

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	SetRunning(false);
}

void ABotCharacter::SetRunning(bool bRunning)
{
	GetCharacterMovement()->MaxWalkSpeed = bRunning ? RunSpeed : WalkSpeed;
}

