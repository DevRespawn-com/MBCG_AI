// Copyright DevRespawn.com (MBCG). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "NavArea_Obstacle_Tier01_MBCG.generated.h"

class UObject;

/**
 * It represents a cost area of tier 1 (e.g. to mark a place with 1 death)
 */
UCLASS(Config = Engine, MinimalAPI)
// class LYRAGAME_API UNavArea_Obstacle_Tier01_MBCG : public UNavArea
class UNavArea_Obstacle_Tier01_MBCG : public UNavArea_Obstacle
{
    GENERATED_UCLASS_BODY()
};
