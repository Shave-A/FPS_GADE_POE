#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MatchLibrary.generated.h"

UCLASS()
class UMatchLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Call from PlayerBP when the player dies, so the killer gets the point. */
    UFUNCTION(BlueprintCallable, Category = "Match", meta = (WorldContext = "WorldContextObject"))
    static void ReportKill(const UObject* WorldContextObject, AActor* Killer, AActor* Victim);
};