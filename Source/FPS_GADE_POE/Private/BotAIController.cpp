#include "BotAIController.h"
#include "BotCharacter.h"
#include "BotStates.h"
#include "Navigation/PathFollowingComponent.h"
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
    if (Bot) { Bot->OnBotDamaged.RemoveAll(this); }
    Super::OnPossess(InPawn);
    Bot = Cast<ABotCharacter>(InPawn);
    if (Bot) { Bot->OnBotDamaged.AddUObject(this, &ABotAIController::ReactToDamage); }

    CreateStates();
    CurrentState = nullptr;
    ChangeState(EBotState::Patrol);

    GetWorldTimerManager().SetTimer(ThinkTimer, this, &ABotAIController::Think,
        ThinkInterval, true, FMath::FRandRange(0.05f, ThinkInterval));   // staggered per bot
}

void ABotAIController::OnUnPossess()
{
    GetWorldTimerManager().ClearTimer(ThinkTimer);
    if (CurrentState) { CurrentState->Exit(this); }
    CurrentState = nullptr;
    Super::OnUnPossess();
}

// ------------------------------------------------------------------ state machine
void ABotAIController::CreateStates()
{
    if (States.Num() > 0) return;   // only once per controller

    States.Add(EBotState::Patrol, NewObject<UBotState_Patrol>(this));
    States.Add(EBotState::Attack, NewObject<UBotState_Attack>(this));
    States.Add(EBotState::Evade, NewObject<UBotState_Evade>(this));
    States.Add(EBotState::SeekHealth, NewObject<UBotState_SeekHealth>(this));
    States.Add(EBotState::SeekAmmo, NewObject<UBotState_SeekAmmo>(this));
    States.Add(EBotState::Hunt, NewObject<UBotState_Hunt>(this));
    States.Add(EBotState::Search, NewObject<UBotState_Search>(this));
    States.Add(EBotState::Dead, NewObject<UBotState_Dead>(this));
}

void ABotAIController::ChangeState(EBotState NewState)
{
    if (!Bot) return;

    TObjectPtr<UBotState>* Found = States.Find(NewState);
    if (!Found || !*Found || *Found == CurrentState) return;

    if (CurrentState) { CurrentState->Exit(this); }
    StopMovement();                      // every state starts with a clean move order
    CurrentState = *Found;
    CurrentState->Enter(this);
}

EBotState ABotAIController::GetState() const
{
    return CurrentState ? CurrentState->GetStateId() : EBotState::Patrol;
}

void ABotAIController::Think()
{
    if (!Bot || !CurrentState) return;

    // Dying can happen in any state, so the controller checks it before asking the state
    if (Bot->IsDead()) { ChangeState(EBotState::Dead); }

    // Let the active state decide. If it hands over to a new state, give the new
    // state one go straight away so the bot reacts this think instead of the next one.
    // Capped so two states can never bounce back and forth forever.
    for (int32 i = 0; i < 3; ++i)
    {
        UBotState* Before = CurrentState;
        CurrentState->Think(this);
        if (CurrentState == Before) break;
    }
}

void ABotAIController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Bot && CurrentState) { CurrentState->TickState(this, DeltaSeconds); }
}

// ------------------------------------------------------------------ memory
void ABotAIController::ReactToDamage(AActor* Attacker)
{
    if (Attacker && Attacker != GetPawn() && Cast<APawn>(Attacker))
    {
        AwarenessTarget = Attacker;
        AwarenessExpire = GetWorld()->GetTimeSeconds() + 3.f;
    }
}

bool ABotAIController::IsAwareOfAttacker() const
{
    return AwarenessTarget.IsValid() && GetWorld()->GetTimeSeconds() < AwarenessExpire;
}

void ABotAIController::ClearMemory()
{
    CurrentTarget.Reset();
    CurrentPickup.Reset();
    AwarenessTarget.Reset();
}

bool ABotAIController::IsMoving() const
{
    return GetMoveStatus() == EPathFollowingStatus::Moving;
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

        const float DistSq = FVector::DistSquared(C->GetActorLocation(), GetPawn()->GetActorLocation());
        const bool bAware = AwarenessTarget.Get() == C && Now < AwarenessExpire;   // shot by it -> any direction
        const bool bClose = DistSq < FMath::Square(CloseSenseRange);               // very close -> any direction
        if (!CanSee(C, SightRange, (bAware || bClose) ? 180.f : SightHalfAngle)) continue;

        if (DistSq < BestSq) { BestSq = DistSq; Best = C; }
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
        if (const float* Until = IgnoredPickups.Find(P)) { if (Now < *Until) continue; }
        if (!CanSee(P, PickupSightRange, 85.f)) continue;
        const float Sq = FVector::DistSquared(P->GetActorLocation(), GetPawn()->GetActorLocation());
        if (Sq < BestSq) { BestSq = Sq; Best = P; }
    }
    return Best;
}

APawn* ABotAIController::GetHuntTargetIfLastBot() const
{
    APawn* Prey = nullptr;
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        APawn* P = *It;
        if (!IsValidTarget(P)) continue;
        if (Cast<ABotCharacter>(P)) return nullptr;   // another bot is alive -> normal patrol
        if (!Prey) Prey = P;
    }
    return Prey;
}