#include "MatchHudWidget.h"
#include "MatchManager.h"
#include "Components/TextBlock.h"

void UMatchHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    UMatchManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UMatchManager>() : nullptr;
    if (!Manager) return;

    // CeilToInt so it shows 5:00 at the start and reaches 0:00 exactly when the match ends
    const int32 Seconds = FMath::CeilToInt(Manager->GetTimeRemaining());
    TimerText->SetText(FText::FromString(FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60)));

    if (KillsText)
    {
        KillsText->SetText(FText::FromString(FString::Printf(TEXT("Kills: %d"), Manager->GetPlayerKills())));
    }
}