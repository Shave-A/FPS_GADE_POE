#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MatchManager.generated.h"

class UUserWidget;
class UMatchHudWidget;
class UMatchMenuWidget;
class UPauseMenuWidget;
class ABotCharacter;

USTRUCT(BlueprintType)
struct FLeaderboardEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) int32 Kills = 0;
    UPROPERTY(BlueprintReadOnly) bool bIsPlayer = false;

    TWeakObjectPtr<AActor> Actor;
};

// Observer pattern: the UI subscribes to these instead of the manager knowing about the UI's layout
DECLARE_MULTICAST_DELEGATE(FMatchEndedEvent);
DECLARE_MULTICAST_DELEGATE(FScoresChangedEvent);

/**
 * Owns the match: 5 minute timer, kill tracking, pause, restart and quit.
 * A world subsystem, so it needs no placing in the level.
 */
UCLASS()
class UMatchManager : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    void ReportKill(AActor* Killer, AActor* Victim);

    TArray<FLeaderboardEntry> GetLeaderboard();   // sorted, most kills first
    int32 GetPlayerKills();
    float GetTimeRemaining() const { return TimeRemaining; }
    bool IsMatchActive() const { return bMatchActive; }

    void PauseGame();
    void ResumeGame();
    void RestartMatch();
    void QuitToTitle();

    FMatchEndedEvent OnMatchEnded;
    FScoresChangedEvent OnScoresChanged;

protected:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
    void StartMatch();
    void EndMatch();
    void HandleBotDied(ABotCharacter* Bot, AActor* Killer);

    FLeaderboardEntry* FindEntry(AActor* Actor);
    void EnsurePlayerEntry();

    void ShowMenu(UUserWidget* Widget);
    void SetGameplayInput();

    UPROPERTY() TObjectPtr<UMatchHudWidget> HudWidget;
    UPROPERTY() TObjectPtr<UPauseMenuWidget> PauseWidget;
    UPROPERTY() TObjectPtr<UMatchMenuWidget> MatchOverWidget;

    TArray<FLeaderboardEntry> Entries;
    float TimeRemaining = 300.f;
    bool bMatchActive = false;
    bool bPaused = false;
};