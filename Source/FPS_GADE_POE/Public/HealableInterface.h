#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HealableInterface.generated.h"

UINTERFACE(MinimalAPI)
class UHealableInterface : public UInterface
{
    GENERATED_BODY()
};

class IHealableInterface
{
    GENERATED_BODY()

public:
    virtual float Heal(float Amount) = 0;
};