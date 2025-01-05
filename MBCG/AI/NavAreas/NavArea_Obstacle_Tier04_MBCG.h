// Copyright DevRespawn.com (MBCG). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "NavArea_Obstacle_Tier04_MBCG.generated.h"

class UObject;

/**
 * It represents a cost area of tier 4 (e.g. to mark a place with 4 deaths)
 */
UCLASS(Config = Engine, MinimalAPI)
class UNavArea_Obstacle_Tier04_MBCG : public UNavArea_Obstacle
{
    GENERATED_UCLASS_BODY()
};