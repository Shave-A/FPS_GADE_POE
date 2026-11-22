#include "MatchLibrary.h"
#include "MatchManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void UMatchLibrary::ReportKill(const UObject* WorldContextObject, AActor* Killer, AActor* Victim)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
    if (!World) return;

    if (UMatchManager* Manager = World->GetSubsystem<UMatchManager>())
    {
        Manager->ReportKill(Killer, Victim);
    }
}