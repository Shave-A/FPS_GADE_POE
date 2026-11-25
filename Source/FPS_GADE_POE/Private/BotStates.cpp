#include "BotStates.h"
#include "BotAIController.h"
#include "BotCharacter.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"

// ====================================================================== shared rules (base class)
bool UBotState::TryEnterCombat(ABotAIController* AI) const
{
    if (AI->FindVisibleTarget())
    {
        const bool bLowHealth = AI->GetBot()->GetHealth() < AI->LowHealth;
        AI->ChangeState(bLowHealth ? EBotState::Evade : EBotState::Attack);
        return true;
    }
    return false;
}

EBotState UBotState::ChooseIdleState(ABotAIController* AI) const
{
    ABotCharacter* Bot = AI->GetBot();

    if (Bot->GetHealth() < AI->SeekHealthBelow && AI->FindVisiblePickup(AI->HealthPickupTag))
        return EBotState::SeekHealth;

    if (Bot->NeedsAmmo() && AI->FindVisiblePickup(AI->AmmoPickupTag))
        return EBotState::SeekAmmo;

    if (AI->GetHuntTargetIfLastBot())
        return EBotState::Hunt;

    return EBotState::Patrol;
}

void UBotState::UpdateAwarenessFocus(ABotAIController* AI) const
{
    AI->CurrentTarget.Reset();
    AI->ClearFocus(EAIFocusPriority::Gameplay);

    // Shot by something it can't see: turn to face the attacker for a moment
    if (AI->IsAwareOfAttacker())
    {
        AI->SetFocus(AI->GetAwarenessTarget());
    }
}

// ====================================================================== Patrol
void UBotState_Patrol::Enter(ABotAIController* AI)
{
    AI->GetBot()->SetRunning(false);
    AI->CurrentPickup.Reset();
    PatrolTimeout = 0.f;   // pick a fresh point straight away
}

void UBotState_Patrol::Think(ABotAIController* AI)
{
    if (TryEnterCombat(AI)) return;
    UpdateAwarenessFocus(AI);

    const EBotState Wanted = ChooseIdleState(AI);
    if (Wanted != GetStateId()) { AI->ChangeState(Wanted); return; }

    const float Now = AI->GetWorld()->GetTimeSeconds();
    if (AI->IsMoving() && Now < PatrolTimeout) return;

    UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(AI->GetWorld());
    FNavLocation Out;
    if (Nav && Nav->GetRandomReachablePointInRadius(AI->GetPawn()->GetActorLocation(), AI->PatrolRadius, Out))
    {
        AI->MoveToLocation(Out.Location, 80.f);
        PatrolTimeout = Now + 15.f;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Bot has no patrol point - is there a NavMeshBoundsVolume in the level?"));
        PatrolTimeout = Now + 2.f;
    }
}

// ====================================================================== Attack
void UBotState_Attack::Enter(ABotAIController* AI)
{
    AI->GetBot()->SetRunning(true);
}

void UBotState_Attack::Exit(ABotAIController* AI)
{
    AI->ClearFocus(EAIFocusPriority::Gameplay);
}

void UBotState_Attack::Think(ABotAIController* AI)
{
    ABotCharacter* Bot = AI->GetBot();
    AActor* Target = AI->FindVisibleTarget();

    // Lost sight of everyone: go and look where the target was last seen
    if (!Target) { AI->ChangeState(EBotState::Search); return; }

    // Hurt badly: stop fighting and get away
    if (Bot->GetHealth() < AI->LowHealth) { AI->ChangeState(EBotState::Evade); return; }

    const float Now = AI->GetWorld()->GetTimeSeconds();
    if (AI->CurrentTarget.Get() != Target)
    {
        FireAllowedTime = Now + AI->ReactionTime;   // reaction delay on a new target
    }
    AI->CurrentTarget = Target;
    AI->LastKnownTargetLocation = Target->GetActorLocation();
    AI->SetFocus(Target);

    const float Dist = FVector::Dist(AI->GetPawn()->GetActorLocation(), Target->GetActorLocation());
    if (Dist > AI->PreferredRange)
    {
        if (!AI->IsMoving()) AI->MoveToActor(Target, AI->PreferredRange * 0.7f, true, true, true);
    }
    else if (AI->IsMoving())
    {
        AI->StopMovement();
    }
}

void UBotState_Attack::TickState(ABotAIController* AI, float DeltaTime)
{
    ABotCharacter* Bot = AI->GetBot();
    AActor* Target = AI->CurrentTarget.Get();
    if (!Bot || Bot->IsDead() || !Target) return;

    const float Now = AI->GetWorld()->GetTimeSeconds();

    if (!AI->IsMoving())    // strafe while holding position
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
    if (Now >= FireAllowedTime && Facing >= FMath::Cos(FMath::DegreesToRadians(AI->FireConeDeg)))
    {
        Bot->FireAt(Target->GetActorLocation());
    }
}

// ====================================================================== Evade
void UBotState_Evade::Enter(ABotAIController* AI)
{
    AI->GetBot()->SetRunning(true);
    AI->ClearFocus(EAIFocusPriority::Gameplay);
    NextEvadeRepath = 0.f;
}

void UBotState_Evade::Think(ABotAIController* AI)
{
    AActor* Threat = AI->FindVisibleTarget();

    // Threat out of sight: back to normal behaviour (probably SeekHealth)
    if (!Threat) { AI->ChangeState(ChooseIdleState(AI)); return; }

    // Healed enough to fight again
    if (AI->GetBot()->GetHealth() >= AI->LowHealth) { AI->ChangeState(EBotState::Attack); return; }

    AI->CurrentTarget = Threat;
    const float Now = AI->GetWorld()->GetTimeSeconds();

    // Flee towards a health pickup if it can see one
    if (AActor* HealthPickup = AI->FindVisiblePickup(AI->HealthPickupTag))
    {
        if (AI->CurrentPickup.Get() != HealthPickup)
        {
            AI->CurrentPickup = HealthPickup;
            AI->PickupCommitUntil = Now + 10.f;
            AI->StopMovement();
        }
        if (!AI->IsMoving()) AI->MoveToActor(HealthPickup, 20.f, false);
        return;
    }

    if (Now < NextEvadeRepath && AI->IsMoving()) return;
    NextEvadeRepath = Now + 1.5f;

    UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(AI->GetWorld());
    if (!Nav) return;

    const FVector MyLoc = AI->GetPawn()->GetActorLocation();
    const FVector ThreatLoc = Threat->GetActorLocation();
    const FVector Away = (MyLoc - ThreatLoc).GetSafeNormal2D();
    const float CurDist = FVector::Dist2D(MyLoc, ThreatLoc);

    for (int32 i = 0; i < 6; ++i)    // reachable point on the far side, further from the threat
    {
        FNavLocation Out;
        if (Nav->GetRandomReachablePointInRadius(MyLoc + Away * AI->EvadeDistance, 900.f, Out)
            && FVector::Dist2D(Out.Location, ThreatLoc) > CurDist)
        {
            AI->MoveToLocation(Out.Location, 60.f);
            return;
        }
    }
}

// ====================================================================== Seek pickup (health + ammo)
void UBotState_SeekPickup::Enter(ABotAIController* AI)
{
    AI->GetBot()->SetRunning(true);
}

void UBotState_SeekPickup::Think(ABotAIController* AI)
{
    if (TryEnterCombat(AI)) return;
    UpdateAwarenessFocus(AI);

    // Still the most important thing to do? (e.g. SeekAmmo gives way to SeekHealth)
    const EBotState Wanted = ChooseIdleState(AI);
    if (Wanted != GetStateId()) { AI->ChangeState(Wanted); return; }

    AActor* Pickup = AI->FindVisiblePickup(GetPickupTag(AI));
    if (!Pickup) { AI->ChangeState(EBotState::Patrol); return; }

    ABotCharacter* Bot = AI->GetBot();
    const float Now = AI->GetWorld()->GetTimeSeconds();
    if (AI->CurrentPickup.Get() != Pickup)
    {
        AI->CurrentPickup = Pickup;
        AI->PickupCommitUntil = Now + 10.f;
        AI->StopMovement();
    }

    // Ammo / weapon pickups are collected by the bot itself
    if (Pickup->ActorHasTag(AI->AmmoPickupTag)
        && FVector::Dist2D(Bot->GetActorLocation(), Pickup->GetActorLocation()) < 130.f)
    {
        Bot->CollectPickup(Pickup);
        return;
    }

    if (!AI->IsMoving())
    {
        if (AI->MoveToActor(Pickup, 20.f, false) == EPathFollowingRequestResult::Failed)
        {
            // Can't reach it (off the nav mesh?): ignore it for 10s instead of freezing
            AI->IgnoredPickups.Add(Pickup, Now + 10.f);
            AI->CurrentPickup.Reset();
        }
    }
}

// ====================================================================== Hunt
void UBotState_Hunt::Enter(ABotAIController* AI)
{
    AI->GetBot()->SetRunning(true);
    AI->CurrentPickup.Reset();
    NextHuntRepath = 0.f;
}

void UBotState_Hunt::Think(ABotAIController* AI)
{
    if (TryEnterCombat(AI)) return;
    UpdateAwarenessFocus(AI);

    const EBotState Wanted = ChooseIdleState(AI);
    if (Wanted != GetStateId()) { AI->ChangeState(Wanted); return; }

    APawn* Prey = AI->GetHuntTargetIfLastBot();
    if (!Prey) { AI->ChangeState(EBotState::Patrol); return; }

    const float Now = AI->GetWorld()->GetTimeSeconds();
    if (Now < NextHuntRepath && AI->IsMoving()) return;
    NextHuntRepath = Now + AI->HuntRepathInterval;

    AI->MoveToActor(Prey, 600.f, true, true, false);   // walks towards the player; Attack takes over once it sees them
}

// ====================================================================== Search
void UBotState_Search::Enter(ABotAIController* AI)
{
    AI->GetBot()->SetRunning(true);
    SearchEndTime = AI->GetWorld()->GetTimeSeconds() + AI->SearchDuration;
    AI->MoveToLocation(AI->LastKnownTargetLocation, 80.f);
}

void UBotState_Search::Think(ABotAIController* AI)
{
    // Found someone again
    if (TryEnterCombat(AI)) return;
    UpdateAwarenessFocus(AI);

    // Gave up looking: carry on with normal behaviour
    if (AI->GetWorld()->GetTimeSeconds() >= SearchEndTime)
    {
        AI->ChangeState(ChooseIdleState(AI));
    }
}

// ====================================================================== Dead
void UBotState_Dead::Enter(ABotAIController* AI)
{
    AI->StopMovement();
    AI->ClearFocus(EAIFocusPriority::Gameplay);
    AI->ClearMemory();
}

void UBotState_Dead::Think(ABotAIController* AI)
{
    // ABotCharacter::Respawn() brings the bot back; then start patrolling again
    if (!AI->GetBot()->IsDead())
    {
        AI->ChangeState(EBotState::Patrol);
    }
}