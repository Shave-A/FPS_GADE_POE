#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BotAIController.h"      // EBotState
#include "BotStates.generated.h"

/**
 * State pattern: the base class every bot state follows.
 * The controller holds one active state and forwards Think (decisions) and TickState (per-frame work) to it.
 * Each state decides for itself when to hand over to another state by calling AI->ChangeState(...).
 */
UCLASS(Abstract)
class UBotState : public UObject
{
    GENERATED_BODY()

public:
    virtual EBotState GetStateId() const PURE_VIRTUAL(UBotState::GetStateId, return EBotState::Patrol;);

    virtual void Enter(ABotAIController* AI) {}                     // once, when the state becomes active
    virtual void Exit(ABotAIController* AI) {}                      // once, when the state is left
    virtual void Think(ABotAIController* AI) {}                     // every ThinkInterval: decide + act
    virtual void TickState(ABotAIController* AI, float DeltaTime) {} // every frame (only Attack needs it)

protected:
    /** Shared rule: if an enemy is visible, switch to Attack (or Evade when low on health). Returns true if it switched. */
    bool TryEnterCombat(ABotAIController* AI) const;

    /** Shared rule: which non-combat state the bot should be in right now (health > ammo > hunt > patrol). */
    EBotState ChooseIdleState(ABotAIController* AI) const;

    /** Shared rule: with no enemy in sight, face whoever just shot us for a moment. */
    void UpdateAwarenessFocus(ABotAIController* AI) const;
};

// ====================================================================== Patrol
/** Wanders to random reachable points at walking speed. */
UCLASS()
class UBotState_Patrol : public UBotState
{
    GENERATED_BODY()
public:
    virtual EBotState GetStateId() const override { return EBotState::Patrol; }
    virtual void Enter(ABotAIController* AI) override;
    virtual void Think(ABotAIController* AI) override;
private:
    float PatrolTimeout = 0.f;
};

// ====================================================================== Attack
/** Closes to preferred range, strafes and shoots the visible target. */
UCLASS()
class UBotState_Attack : public UBotState
{
    GENERATED_BODY()
public:
    virtual EBotState GetStateId() const override { return EBotState::Attack; }
    virtual void Enter(ABotAIController* AI) override;
    virtual void Exit(ABotAIController* AI) override;
    virtual void Think(ABotAIController* AI) override;
    virtual void TickState(ABotAIController* AI, float DeltaTime) override;
private:
    float FireAllowedTime = 0.f;   // reaction delay before the first shot at a new target
    float StrafeFlipTime = 0.f;
    float StrafeDir = 1.f;
};

// ====================================================================== Evade
/** Low on health while an enemy is visible: run to a health pickup, or away from the threat. */
UCLASS()
class UBotState_Evade : public UBotState
{
    GENERATED_BODY()
public:
    virtual EBotState GetStateId() const override { return EBotState::Evade; }
    virtual void Enter(ABotAIController* AI) override;
    virtual void Think(ABotAIController* AI) override;
private:
    float NextEvadeRepath = 0.f;
};

// ====================================================================== Seek pickup (shared base)
/** Walks to a visible pickup. Subclasses only say which tag they want. */
UCLASS(Abstract)
class UBotState_SeekPickup : public UBotState
{
    GENERATED_BODY()
public:
    virtual void Enter(ABotAIController* AI) override;
    virtual void Think(ABotAIController* AI) override;
protected:
    virtual FName GetPickupTag(ABotAIController* AI) const PURE_VIRTUAL(UBotState_SeekPickup::GetPickupTag, return NAME_None;);
};

UCLASS()
class UBotState_SeekHealth : public UBotState_SeekPickup
{
    GENERATED_BODY()
public:
    virtual EBotState GetStateId() const override { return EBotState::SeekHealth; }
protected:
    virtual FName GetPickupTag(ABotAIController* AI) const override { return AI->HealthPickupTag; }
};

UCLASS()
class UBotState_SeekAmmo : public UBotState_SeekPickup
{
    GENERATED_BODY()
public:
    virtual EBotState GetStateId() const override { return EBotState::SeekAmmo; }
protected:
    virtual FName GetPickupTag(ABotAIController* AI) const override { return AI->AmmoPickupTag; }
};

// ====================================================================== Hunt
/** Last bot alive: head towards the player. Attack takes over once it sees them. */
UCLASS()
class UBotState_Hunt : public UBotState
{
    GENERATED_BODY()
public:
    virtual EBotState GetStateId() const override { return EBotState::Hunt; }
    virtual void Enter(ABotAIController* AI) override;
    virtual void Think(ABotAIController* AI) override;
private:
    float NextHuntRepath = 0.f;
};

// ====================================================================== Search
/** Lost sight of its target: go to where it was last seen and look for a few seconds. */
UCLASS()
class UBotState_Search : public UBotState
{
    GENERATED_BODY()
public:
    virtual EBotState GetStateId() const override { return EBotState::Search; }
    virtual void Enter(ABotAIController* AI) override;
    virtual void Think(ABotAIController* AI) override;
private:
    float SearchEndTime = 0.f;
};

// ====================================================================== Dead
/** Does nothing until the bot respawns, then returns to Patrol. */
UCLASS()
class UBotState_Dead : public UBotState
{
    GENERATED_BODY()
public:
    virtual EBotState GetStateId() const override { return EBotState::Dead; }
    virtual void Enter(ABotAIController* AI) override;
    virtual void Think(ABotAIController* AI) override;
};