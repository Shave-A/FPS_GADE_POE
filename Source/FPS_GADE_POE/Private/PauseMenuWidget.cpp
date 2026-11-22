#include "PauseMenuWidget.h"
#include "MatchManager.h"
#include "Components/Button.h"
#include "InputCoreTypes.h"

void UPauseMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (ResumeButton) { ResumeButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleResume); }
}

FReply UPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)     // ESC again resumes
    {
        HandleResume();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPauseMenuWidget::HandleResume()
{
    if (UMatchManager* Manager = GetWorld()->GetSubsystem<UMatchManager>()) { Manager->ResumeGame(); }
}