#include "MatchMenuWidget.h"
#include "MatchManager.h"
#include "Components/Button.h"

void UMatchMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (RestartButton) { RestartButton->OnClicked.AddDynamic(this, &UMatchMenuWidget::HandleRestart); }
    if (QuitButton) { QuitButton->OnClicked.AddDynamic(this, &UMatchMenuWidget::HandleQuit); }
}

void UMatchMenuWidget::HandleRestart()
{
    if (UMatchManager* Manager = GetWorld()->GetSubsystem<UMatchManager>()) { Manager->RestartMatch(); }
}

void UMatchMenuWidget::HandleQuit()
{
    if (UMatchManager* Manager = GetWorld()->GetSubsystem<UMatchManager>()) { Manager->QuitToTitle(); }
}