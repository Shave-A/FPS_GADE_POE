#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatchMenuWidget.generated.h"

class UButton;

/** Match Over screen. Needs buttons named RestartButton and QuitButton. Add a WBP_Leaderboard inside it. */
UCLASS()
class UMatchMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;

    UFUNCTION() void HandleRestart();
    UFUNCTION() void HandleQuit();

    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> RestartButton;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> QuitButton;
};