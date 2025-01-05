// Copyright DevRespawn.com (MBCG). All Rights Reserved.

#include "MBCG/AI/Volumes/MBCG_DeathPlaceNavModifierVolume.h"
#include "Logging/StructuredLog.h"


DEFINE_LOG_CATEGORY_STATIC(LogAMBCG_DeathPlaceNavModifierVolume, All, All);


AMBCG_DeathPlaceNavModifierVolume::AMBCG_DeathPlaceNavModifierVolume(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Create and attach UBoxComponent as the root
    BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("VolumeBounds"));
    if (BoxComponent)
    {
        SetRootComponent(BoxComponent);
        BoxComponent->SetBoxExtent(FVector(50.f, 50.f, 50.f));  // Example size
        BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

        BoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
        BoxComponent->SetGenerateOverlapEvents(true);
        // Critical for navmesh updates
        BoxComponent->SetCanEverAffectNavigation(true);
        BoxComponent->bDynamicObstacle = 1;
        // this is bascially to set bUseSystemDefaultObstacleAreaClass = false;
        BoxComponent->SetAreaClassOverride(BoxComponent->GetDesiredAreaClass());
        // COP: because we need to set it to false, not to true
        // BoxComponent->SetUseSystemDefaultObstacleAreaClass();
    }
    else
    {
        UE_LOGFMT(LogAMBCG_DeathPlaceNavModifierVolume, Error, "Failed to create UBoxComponent for NavModifierVolume");
    }
}
