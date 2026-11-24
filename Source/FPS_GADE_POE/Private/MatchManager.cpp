#include "MatchManager.h"
#include "MatchSettings.h"
#include "MatchHudWidget.h"
#include "MatchMenuWidget.h"
#include "PauseMenuWidget.h"
#include "BotCharacter.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"

bool UMatchManager::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UMatchManager::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UMatchManager, STATGROUP_Tickables);
}

void UMatchManager::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    const FString MapName = UGameplayStatics::GetCurrentLevelName(&InWorld, true);
    if (MapName == TEXT("FiringRange") || MapName == TEXT("MainMenu"))
    {
        return; // no match timer or match HUD outside the arena
    }

    StartMatch();
}

// ---------------------------------------------------------------- match flow
void UMatchManager::StartMatch()
{
    const UMatchSettings* Settings = GetDefault<UMatchSettings>();

    Entries.Reset();                                   // scores cleared for every new match
    TimeRemaining = Settings->MatchLengthSeconds;
    bPaused = false;
    bMatchActive = true;

    static const TCHAR* BotNames[] = { TEXT("Ruby"), TEXT("Sapphire"), TEXT("Emerald"), TEXT("Gold") };

    // Sort by internal name so each bot gets the same leaderboard name every match
    TArray<ABotCharacter*> Bots;
    for (TActorIterator<ABotCharacter> It(GetWorld()); It; ++It)
    {
        Bots.Add(*It);
    }
    Bots.Sort([](const ABotCharacter& A, const ABotCharacter& B) { return A.GetName() < B.GetName(); });

    for (int32 i = 0; i < Bots.Num(); ++i)
    {
        ABotCharacter* Bot = Bots[i];
        Bot->DisplayName = (i < UE_ARRAY_COUNT(BotNames)) ? FString(BotNames[i]) : FString::Printf(TEXT("Bot %d"), i + 1);

        FLeaderboardEntry Entry;
        Entry.Name = Bot->DisplayName;
        Entry.Actor = Bot;
        Entries.Add(Entry);
        Bot->OnBotDied.AddUObject(this, &UMatchManager::HandleBotDied);
    }
    EnsurePlayerEntry();

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC && Settings->HudWidgetClass)
    {
        HudWidget = CreateWidget<UMatchHudWidget>(PC, Settings->HudWidgetClass);
        if (HudWidget) { HudWidget->AddToViewport(0); }
    }
    SetGameplayInput();
}

void UMatchManager::Tick(float DeltaTime)
{
    if (!bMatchActive) return;

    // Not called while the game is paused, so the timer stops during the pause menu
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        if (PC->WasInputKeyJustPressed(EKeys::Escape))
        {
            PauseGame();
            return;
        }
    }

    TimeRemaining = FMath::Max(0.f, TimeRemaining - DeltaTime);
    if (TimeRemaining <= 0.f)
    {
        EndMatch();
    }
}

void UMatchManager::EndMatch()
{
    bMatchActive = false;

    const UMatchSettings* Settings = GetDefault<UMatchSettings>();
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC && Settings->MatchOverWidgetClass)
    {
        MatchOverWidget = CreateWidget<UMatchMenuWidget>(PC, Settings->MatchOverWidgetClass);
        ShowMenu(MatchOverWidget);
    }
    UGameplayStatics::SetGamePaused(GetWorld(), true);

    OnMatchEnded.Broadcast();
}

// ---------------------------------------------------------------- scoring
void UMatchManager::HandleBotDied(ABotCharacter* Bot, AActor* Killer)
{
    ReportKill(Killer, Bot);
}

void UMatchManager::ReportKill(AActor* Killer, AActor* Victim)
{
    if (!bMatchActive || !Killer || Killer == Victim) return;

    if (FLeaderboardEntry* Entry = FindEntry(Killer))
    {
        ++Entry->Kills;
        OnScoresChanged.Broadcast();
    }
}

void UMatchManager::EnsurePlayerEntry()
{
    for (const FLeaderboardEntry& E : Entries)
    {
        if (E.bIsPlayer) return;
    }
    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
    {
        FLeaderboardEntry Entry;
        Entry.Name = GetDefault<UMatchSettings>()->PlayerDisplayName;
        Entry.bIsPlayer = true;
        Entry.Actor = Pawn;
        Entries.Add(Entry);
    }
}

FLeaderboardEntry* UMatchManager::FindEntry(AActor* Actor)
{
    if (!Actor) return nullptr;

    const APawn* Pawn = Cast<APawn>(Actor);
    const bool bPlayer = Pawn && Pawn->IsPlayerControlled();
    if (bPlayer) { EnsurePlayerEntry(); }

    for (FLeaderboardEntry& E : Entries)
    {
        if (bPlayer ? E.bIsPlayer : E.Actor.Get() == Actor) return &E;
    }
    return nullptr;
}

TArray<FLeaderboardEntry> UMatchManager::GetLeaderboard()
{
    EnsurePlayerEntry();

    TArray<FLeaderboardEntry> Sorted = Entries;
    Sorted.Sort([](const FLeaderboardEntry& A, const FLeaderboardEntry& B)
        {
            if (A.Kills != B.Kills) return A.Kills > B.Kills;     // most kills first
            return A.Name < B.Name;
        });
    return Sorted;
}

int32 UMatchManager::GetPlayerKills()
{
    EnsurePlayerEntry();
    for (const FLeaderboardEntry& E : Entries)
    {
        if (E.bIsPlayer) return E.Kills;
    }
    return 0;
}

// ---------------------------------------------------------------- pause / restart / quit
void UMatchManager::ShowMenu(UUserWidget* Widget)
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !Widget) return;

    Widget->AddToViewport(100);

    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(Widget->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(Mode);
    PC->SetShowMouseCursor(true);
}

void UMatchManager::SetGameplayInput()
{
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->SetShowMouseCursor(false);
    }
}

void UMatchManager::PauseGame()
{
    if (!bMatchActive || bPaused) return;

    const UMatchSettings* Settings = GetDefault<UMatchSettings>();
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !Settings->PauseWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("Pause widget class not set in Project Settings > Match Settings."));
        return;
    }

    bPaused = true;
    if (!PauseWidget)
    {
        PauseWidget = CreateWidget<UPauseMenuWidget>(PC, Settings->PauseWidgetClass);
    }
    ShowMenu(PauseWidget);
    UGameplayStatics::SetGamePaused(GetWorld(), true);
}

void UMatchManager::ResumeGame()
{
    if (!bPaused) return;
    bPaused = false;

    if (PauseWidget) { PauseWidget->RemoveFromParent(); }
    SetGameplayInput();
    UGameplayStatics::SetGamePaused(GetWorld(), false);
}

void UMatchManager::RestartMatch()
{
    UGameplayStatics::SetGamePaused(GetWorld(), false);
    SetGameplayInput();
    // Reloading the level resets health, bots, pickups and the scores
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void UMatchManager::QuitToTitle()
{
    UGameplayStatics::SetGamePaused(GetWorld(), false);
    SetGameplayInput();
    UGameplayStatics::OpenLevel(this, GetDefault<UMatchSettings>()->TitleLevelName);
}