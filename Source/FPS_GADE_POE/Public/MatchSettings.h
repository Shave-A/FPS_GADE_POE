#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MatchSettings.generated.h"

class UMatchHudWidget;
class UMatchMenuWidget;
class UPauseMenuWidget;

/** Project Settings > Game > Match Settings */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Match Settings"))
class UMatchSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override { return TEXT("Game"); }

    UPROPERTY(Config, EditAnywhere, Category = "Match", meta = (ClampMin = "1"))
    float MatchLengthSeconds = 300.f;      // 5 minutes

    UPROPERTY(Config, EditAnywhere, Category = "Match")
    FString PlayerDisplayName = TEXT("You");

    /** Level opened by "Quit to title". Create this level when you build the title screen. */
    UPROPERTY(Config, EditAnywhere, Category = "Match")
    FName TitleLevelName = TEXT("TitleScreen");

    UPROPERTY(Config, EditAnywhere, Category = "UI")
    TSubclassOf<UMatchHudWidget> HudWidgetClass;

    UPROPERTY(Config, EditAnywhere, Category = "UI")
    TSubclassOf<UMatchMenuWidget> MatchOverWidgetClass;

    UPROPERTY(Config, EditAnywhere, Category = "UI")
    TSubclassOf<UPauseMenuWidget> PauseWidgetClass;
};