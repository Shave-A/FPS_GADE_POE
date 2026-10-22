#include "BotAIController.h"
#include "BotCharacter.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ABotAIController::ABotAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ABotAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Bot = Cast<ABotCharacter>(InPawn);
	GetWorldTimerManager().SetTimer(ThinkTimer, this, &ABotAIController::Think,
		ThinkInterval, true, FMath::FRandRange(0.05f, ThinkInterval));   // staggered per bot
}

void ABotAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	Super::OnUnPossess();
}

void ABotAIController::ReactToDamage(AActor* Attacker)
{
	if (Attacker && Attacker != GetPawn() && Cast<APawn>(Attacker))
	{
		AwarenessTarget = Attacker;
		AwarenessExpire = GetWorld()->GetTimeSeconds() + 3.f;
	}
}

bool ABotAIController::IsMoving() const
{
	return GetMoveStatus() == EPathFollowingStatus::Moving;
}

void ABotAIController::SetState(EBotState NewState)
{
	if (State != NewState) { State = NewState; StopMovement(); }
}

// ------------------------------------------------------------------ decisions
void ABotAIController::Think()
{
	if (!Bot) return;

	if (Bot->IsDead())
	{
		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		CurrentTarget.Reset(); CurrentPickup.Reset(); AwarenessTarget.Reset();
		State = EBotState::Patrol;
		return;
	}

	const float Health = Bot->GetHealth();
	AActor* Target = FindVisibleTarget();

	if (Target)
	{
		if (Health < LowHealth) { DoEvade(Target, FindVisiblePickup(HealthPickupTag)); }  // avoid the target
		else { DoAttack(Target); }          // shoot (secondary if ammo, else default)
		return;
	}

	CurrentTarget.Reset();
	ClearFocus(EAIFocusPriority::Gameplay);

	// Shot by something it can't see: turn to face the attacker for a moment
	if (AwarenessTarget.IsValid() && GetWorld()->GetTimeSeconds() < AwarenessExpire)
	{
		SetFocus(AwarenessTarget.Get());
	}

	if (Health < SeekHealthBelow)
	{
		if (AActor* P = FindVisiblePickup(HealthPickupTag)) { DoSeekPickup(P, EBotState::SeekHealth); return; }
	}
	if (Bot->NeedsAmmo())
	{
		if (AActor* P = FindVisiblePickup(AmmoPickupTag)) { DoSeekPickup(P, EBotState::SeekAmmo); return; }
	}
	DoPatrol();
}

// ------------------------------------------------------------------ perception
bool ABotAIController::IsValidTarget(const APawn* C) const
{
	if (!C || C == GetPawn() || C->IsHidden()) return false;
	if (const ABotCharacter* OtherBot = Cast<ABotCharacter>(C)) return !OtherBot->IsDead();
	return C->IsPlayerControlled();
}

bool ABotAIController::CanSee(const AActor* Other, float Range, float HalfAngleDeg) const
{
	const APawn* Me = GetPawn();
	if (!Me || !Other) return false;

	FVector Eye; FRotator EyeRot;
	Me->GetActorEyesViewPoint(Eye, EyeRot);

	const FVector To = Other->GetActorLocation() - Eye;
	const float Dist = To.Size();
	if (Dist > Range) return false;

	if (HalfAngleDeg < 179.f && Dist > KINDA_SMALL_NUMBER)
	{
		if (FVector::DotProduct(EyeRot.Vector(), To / Dist) < FMath::Cos(FMath::DegreesToRadians(HalfAngleDeg)))
			return false;
	}

	// Line of sight: level geometry blocks it
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BotSight), false, Me);
	Params.AddIgnoredActor(Other);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	return !GetWorld()->LineTraceTestByObjectType(Eye, Other->GetActorLocation(), Objects, Params);
}

AActor* ABotAIController::FindVisibleTarget()
{
	const float Now = GetWorld()->GetTimeSeconds();
	AActor* Best = nullptr;
	float BestSq = TNumericLimits<float>::Max();

	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* C = *It;
		if (!IsValidTarget(C)) continue;

		const bool bAware = AwarenessTarget.Get() == C && Now < AwarenessExpire;   // shot by it -> any direction
		if (!CanSee(C, SightRange, bAware ? 180.f : SightHalfAngle)) continue;

		const float Sq = FVector::DistSquared(C->GetActorLocation(), GetPawn()->GetActorLocation());
		if (Sq < BestSq) { BestSq = Sq; Best = C; }
	}
	return Best;
}

AActor* ABotAIController::FindVisiblePickup(const FName& Tag)
{
	const float Now = GetWorld()->GetTimeSeconds();

	// Stay committed to a pickup it already saw
	if (AActor* Committed = CurrentPickup.Get())
	{
		if (Now < PickupCommitUntil && Committed->ActorHasTag(Tag) && !Committed->IsHidden())
			return Committed;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(this, Tag, Found);

	AActor* Best = nullptr;
	float BestSq = TNumericLimits<float>::Max();
	for (AActor* P : Found)
	{
		if (!P || P->IsHidden()) continue;                       // hidden = already collected
		if (!CanSee(P, PickupSightRange, 85.f)) continue;
		const float Sq = FVector::DistSquared(P->GetActorLocation(), GetPawn()->GetActorLocation());
		if (Sq < BestSq) { BestSq = Sq; Best = P; }
	}
	return Best;
}

// ------------------------------------------------------------------ behaviours
void ABotAIController::DoAttack(AActor* Target)
{
	if (CurrentTarget.Get() != Target)
		FireAllowedTime = GetWorld()->GetTimeSeconds() + ReactionTime;   // reaction delay on new target
	CurrentTarget = Target;

	SetState(EBotState::Attack);
	Bot->SetRunning(true);
	SetFocus(Target);

	const float Dist = FVector::Dist(GetPawn()->GetActorLocation(), Target->GetActorLocation());
	if (Dist > PreferredRange) { if (!IsMoving()) MoveToActor(Target, PreferredRange * 0.7f, true, true, true); }
	else if (IsMoving()) { StopMovement(); }
}

void ABotAIController::DoEvade(AActor* Threat, AActor* HealthPickup)
{
	CurrentTarget = Threat;
	SetState(EBotState::Evade);
	Bot->SetRunning(true);
	ClearFocus(EAIFocusPriority::Gameplay);

	const float Now = GetWorld()->GetTimeSeconds();

	if (HealthPickup)       // flee towards a health pickup if it can see one
	{
		if (CurrentPickup.Get() != HealthPickup)
		{
			CurrentPickup = HealthPickup; PickupCommitUntil = Now + 10.f; StopMovement();
		}
		if (!IsMoving()) MoveToActor(HealthPickup, 20.f, false);
		return;
	}

	if (Now < NextEvadeRepath && IsMoving()) return;
	NextEvadeRepath = Now + 1.5f;

	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!Nav) return;

	const FVector MyLoc = GetPawn()->GetActorLocation();
	const FVector ThreatLoc = Threat->GetActorLocation();
	const FVector Away = (MyLoc - ThreatLoc).GetSafeNormal2D();
	const float CurDist = FVector::Dist2D(MyLoc, ThreatLoc);

	for (int32 i = 0; i < 6; ++i)    // reachable point on the far side, further from the threat
	{
		FNavLocation Out;
		if (Nav->GetRandomReachablePointInRadius(MyLoc + Away * EvadeDistance, 900.f, Out)
			&& FVector::Dist2D(Out.Location, ThreatLoc) > CurDist)
		{
			MoveToLocation(Out.Location, 60.f);
			return;
		}
	}
}

void ABotAIController::DoSeekPickup(AActor* Pickup, EBotState SeekState)
{
	SetState(SeekState);
	Bot->SetRunning(true);

	if (CurrentPickup.Get() != Pickup)
	{
		CurrentPickup = Pickup;
		PickupCommitUntil = GetWorld()->GetTimeSeconds() + 10.f;
		StopMovement();
	}
	if (!IsMoving()) MoveToActor(Pickup, 20.f, false);
}

void ABotAIController::DoPatrol()
{
	SetState(EBotState::Patrol);
	Bot->SetRunning(false);
	CurrentPickup.Reset();

	const float Now = GetWorld()->GetTimeSeconds();
	if (IsMoving() && Now < PatrolTimeout) return;

	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	FNavLocation Out;
	if (Nav && Nav->GetRandomReachablePointInRadius(GetPawn()->GetActorLocation(), PatrolRadius, Out))
	{
		MoveToLocation(Out.Location, 80.f);
		PatrolTimeout = Now + 15.f;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Bot has no patrol point - is there a NavMeshBoundsVolume in the level?"));
		PatrolTimeout = Now + 2.f;
	}
}

// ------------------------------------------------------------------ per-frame combat
void ABotAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Bot && !Bot->IsDead() && State == EBotState::Attack) TickCombat();
}

void ABotAIController::TickCombat()
{
	AActor* Target = CurrentTarget.Get();
	if (!Target) return;

	const float Now = GetWorld()->GetTimeSeconds();

	if (!IsMoving())    // strafe while holding position
	{
		if (Now >= StrafeFlipTime)
		{
			StrafeDir = FMath::RandBool() ? 1.f : -1.f;
			StrafeFlipTime = Now + FMath::FRandRange(0.8f, 2.2f);
		}
		Bot->AddMovementInput(Bot->GetActorRightVector(), StrafeDir * 0.6f);
	}

	const FVector ToTarget = (Target->GetActorLocation() - Bot->GetActorLocation()).GetSafeNormal2D();
	const float Facing = FVector::DotProduct(Bot->GetActorForwardVector().GetSafeNormal2D(), ToTarget);
	if (Now >= FireAllowedTime && Facing >= FMath::Cos(FMath::DegreesToRadians(FireConeDeg)))
	{
		Bot->FireAt(Target->GetActorLocation());
	}
}