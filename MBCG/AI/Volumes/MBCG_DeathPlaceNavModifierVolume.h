// Copyright DevRespawn.com (MBCG). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavModifierVolume.h"
#include "Components/BoxComponent.h"
#include "MBCG_DeathPlaceNavModifierVolume.generated.h"

/**
 * This is a ANavModifierVolume for spawning in runtime as Death placements
 */
UCLASS(Blueprintable, BlueprintType)
class LYRAGAME_API AMBCG_DeathPlaceNavModifierVolume : public ANavModifierVolume
{
    GENERATED_BODY()

public:

    // Constructor with FObjectInitializer
    AMBCG_DeathPlaceNavModifierVolume(const FObjectInitializer& ObjectInitializer);

    // Returns BrushComponent subobject
    UBoxComponent* GetBoxComponent() const { return BoxComponent; }

private:

    UPROPERTY(Category = Collision, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UBoxComponent> BoxComponent;
};
