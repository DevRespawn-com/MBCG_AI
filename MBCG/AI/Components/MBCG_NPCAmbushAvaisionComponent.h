// Copyright DevRespawn.com (MBCG). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "MBCG_NPCAmbushAvaisionComponent.generated.h"


class UMBCG_NPCAmbushAvaisionSubsystem;


/**
 * This component allows to use MBCG_NPCAmbushAvaisionSubsystem
 * This component should be added to ALyraCharacter (which is supposed to be an NPC)
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom))
class LYRAGAME_API UMBCG_NPCAmbushAvaisionComponent : public UPawnComponent
{
    GENERATED_BODY()

public:

    //~UActorComponent interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    //~End of UActorComponent interface

private:

    // This function runs when component's health changes
    UFUNCTION()
    void HandleOwnerHealthChanged(ULyraHealthComponent* HealthComponent, float OldValue, float NewValue, AActor* Instigator);

    // Returns True if Pawn is contolled by AI. Returns False otherwise, also if there is error.
    // Note that this function should be run only after Pawn is possessed by Controller
    bool IsControlledByAI(APawn* Pawn) const;

    UMBCG_NPCAmbushAvaisionSubsystem* NPCAmbushAvaisionSubsystem;
};
