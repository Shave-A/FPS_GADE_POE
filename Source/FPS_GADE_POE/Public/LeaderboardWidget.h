#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LeaderboardWidget.generated.h"

class UVerticalBox;

/** One leaderboard, reused by the Match Over screen and the Pause menu. Needs a VerticalBox named EntriesBox. */
UCLASS()
class ULeaderboardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable)
    void Refresh();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget)) TObjectPtr<UVerticalBox> EntriesBox;

    UPROPERTY(EditAnywhere, Category = "Leaderboard") int32 FontSize = 26;
    UPROPERTY(EditAnywhere, Category = "Leaderboard") FLinearColor RowColor = FLinearColor(0.f, 0.f, 0.f, 0.45f);
    UPROPERTY(EditAnywhere, Category = "Leaderboard") FLinearColor PlayerRowColor = FLinearColor(0.9f, 0.65f, 0.f, 0.85f);

private:
    FDelegateHandle ScoresHandle;
};