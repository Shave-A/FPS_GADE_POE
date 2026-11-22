#pragma once

#include "CoreMinimal.h"
#include "MatchMenuWidget.h"
#include "PauseMenuWidget.generated.h"

/** Pause menu = the match menu plus Resume and the ESC key. Needs a button named ResumeButton. */
UCLASS()
class UPauseMenuWidget : public UMatchMenuWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UFUNCTION() void HandleResume();

    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ResumeButton;
};