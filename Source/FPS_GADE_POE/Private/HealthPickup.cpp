#include "HealthPickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"
#include "HealableInterface.h"

namespace
{
	FString Normalise(FString S)
	{
		S.ReplaceInline(TEXT(" "), TEXT(""));
		S.ReplaceInline(TEXT("_"), TEXT(""));
		return S.ToLower();
	}

	// Finds a numeric Blueprint variable by name, ignoring spaces, underscores and case
	FNumericProperty* FindNumeric(UClass* Class, const FName& Wanted)
	{
		const FString Target = Normalise(Wanted.ToString());
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			if (FNumericProperty* Num = CastField<FNumericProperty>(*It))
			{
				if (Normalise(It->GetName()) == Target || Normalise(It->GetDisplayNameText().ToString()) == Target)
				{
					return Num;
				}
			}
		}
		return nullptr;
	}

	double ReadNumber(FNumericProperty* P, void* Ptr)
	{
		return P->IsFloatingPoint() ? P->GetFloatingPointPropertyValue(Ptr) : (double)P->GetSignedIntPropertyValue(Ptr);
	}

	void WriteNumber(FNumericProperty* P, void* Ptr, double V)
	{
		if (P->IsFloatingPoint()) { P->SetFloatingPointPropertyValue(Ptr, V); }
		else { P->SetIntPropertyValue(Ptr, (int64)FMath::RoundToInt(V)); }
	}
}

AHealthPickup::AHealthPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	Tags.Add(TEXT("HealthPickup"));          // bots find pickups by this tag

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(90.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	// Object type nothing traces for, so bullets and bot sight lines pass through the trigger
	Trigger->SetCollisionObjectType(ECC_Destructible);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	Mesh->SetRelativeScale3D(FVector(0.5f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Shape.Succeeded()) { Mesh->SetStaticMesh(Shape.Object); }

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Mat.Succeeded()) { Mesh->SetMaterial(0, Mat.Object); }
}

void AHealthPickup::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AHealthPickup::OnOverlap);

	if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), PickupColor);
	}
}

void AHealthPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bAvailable) { Mesh->AddLocalRotation(FRotator(0.f, SpinSpeed * DeltaSeconds, 0.f)); }
}

void AHealthPickup::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryPickup(OtherActor);
}

void AHealthPickup::TryPickup(AActor* Other)
{
	APawn* Pawn = Cast<APawn>(Other);
	if (!bAvailable || !Pawn) return;

	bool bUsed = false;

    if (IHealableInterface* Healable = Cast<IHealableInterface>(Pawn))
    {
        Healable->Heal(HealAmount);          // capped at max health inside Heal
        bUsed = true;
    }
	else if (Pawn->IsPlayerControlled())
	{
		bUsed = HealPlayerBlueprint(Pawn);
	}

	if (bUsed)
	{
		SetAvailable(false);
		GetWorldTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				SetAvailable(true);
			}), RespawnTime, false);
	}
}

bool AHealthPickup::HealPlayerBlueprint(AActor* Player)
{
	FNumericProperty* HealthProp = FindNumeric(Player->GetClass(), PlayerHealthVariable);
	if (!HealthProp)
	{
		UE_LOG(LogTemp, Warning, TEXT("HealthPickup: no variable matching '%s' on %s. Set 'Player Health Variable' on the pickup."),
			*PlayerHealthVariable.ToString(), *Player->GetName());
		return false;
	}

	float Max = FallbackMaxHealth;
	if (FNumericProperty* MaxProp = FindNumeric(Player->GetClass(), PlayerMaxHealthVariable))
	{
		Max = (float)ReadNumber(MaxProp, MaxProp->ContainerPtrToValuePtr<void>(Player));
	}

	void* Ptr = HealthProp->ContainerPtrToValuePtr<void>(Player);
	const double Current = ReadNumber(HealthProp, Ptr);
	WriteNumber(HealthProp, Ptr, FMath::Min(Current + HealAmount, (double)Max));   // never above max
	return true;
}

void AHealthPickup::SetAvailable(bool bNewAvailable)
{
	bAvailable = bNewAvailable;
	SetActorHiddenInGame(!bNewAvailable);    // bots treat a hidden pickup as already collected
	SetActorEnableCollision(bNewAvailable);

	if (bNewAvailable)
	{
		// Someone may already be standing on it when it reappears
		TArray<AActor*> Overlapping;
		GetOverlappingActors(Overlapping, APawn::StaticClass());
		for (AActor* A : Overlapping)
		{
			TryPickup(A);
			if (!bAvailable) break;
		}
	}
}