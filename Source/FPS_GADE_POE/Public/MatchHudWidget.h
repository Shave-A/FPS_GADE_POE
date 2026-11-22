#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatchHudWidget.generated.h"

class UTextBlock;

/** In-match HUD. Designer needs a TextBlock named TimerText (and optionally KillsText). */
UCLASS()
class UMatchHudWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TimerText;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> KillsText;
};