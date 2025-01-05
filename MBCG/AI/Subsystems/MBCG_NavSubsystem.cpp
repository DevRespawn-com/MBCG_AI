// Copyright DevRespawn.com (MBCG). All Rights Reserved.

#include "MBCG/AI/Subsystems/MBCG_NavSubsystem.h"
#include "Logging/StructuredLog.h"


DEFINE_LOG_CATEGORY_STATIC(LogUMBCG_NavSubsystem, All, All);


void UMBCG_NavSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}


void UMBCG_NavSubsystem::PostInitialize()
{
    AgentRadius = GetAgentRaidusFromSettings(GetWorld());
    UE_LOGFMT(LogUMBCG_NavSubsystem, Display, "- AgentRadius = {0}.", AgentRadius);
}


void UMBCG_NavSubsystem::Deinitialize()
{
    Super::Deinitialize();
}


float UMBCG_NavSubsystem::GetAgentRaidusFromSettings(UWorld* World) const
{
    if (!World)
    {
        return 0.f;
    }

    // Get the navigation system
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!NavSys)
    {
        UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "Navigation System not found.");
        return 0.f;
    }

    // Get the default supported agent
    const FNavDataConfig& NavConfig = NavSys->GetDefaultSupportedAgentConfig();
    if (NavConfig.IsValid())
    {
        return NavConfig.AgentRadius;
    }
    else
    {
        UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "Default supported agent config not found.");
    }

    return 0.f;
}


void UMBCG_NavSubsystem::DestroySingleNavModifierVolume(TArray<ANavModifierVolume*>& VolumesToDestroy, int32 idx)
{
    if (IsValid(VolumesToDestroy[idx]))
    {
        VolumesToDestroy[idx]->Destroy();
        VolumesToDestroy[idx] = nullptr;

        // debug
        // UE_LOGFMT(LogUMBCG_NavSubsystem, Display, "- NavModifierVolume (Idx = {0}) was destroyed.", idx);
    }
}


void UMBCG_NavSubsystem::DestroyNavModifierVolumes(TArray<ANavModifierVolume*>& VolumesToDestroy)
{
    for (int32 idx = 0; idx < VolumesToDestroy.Num(); ++idx)
    {
        DestroySingleNavModifierVolume(VolumesToDestroy, idx);
    }
}


float UMBCG_NavSubsystem::CalculateNavAreaObstacleCost(int32 DeathQuantity) const
{
    if (DeathQuantity == 1)
    {
        return 10.f;
    }

    // 20 * DeathQuantity^2 - 40
    float Cost = 20 * (1 << DeathQuantity) - 40;
    return Cost > 0.f ? Cost : 0.f;
}


AMBCG_DeathPlaceNavModifierVolume* UMBCG_NavSubsystem::SpawnDeathPlaceNavModifierVolume(const FDeathPlacement& DeathPlacement)
{
    UWorld* World = GetWorld();
    if (!World || DeathPlacement.DeathQuantity <= 0) return nullptr;

    const FTransform VolumeTransform(FRotator::ZeroRotator, DeathPlacement.Location);
    AMBCG_DeathPlaceNavModifierVolume* NavModifierVolume = World->SpawnActorDeferred<AMBCG_DeathPlaceNavModifierVolume>(AMBCG_DeathPlaceNavModifierVolume::StaticClass(), VolumeTransform);

    if (!NavModifierVolume)
    {
        UE_LOGFMT(LogUMBCG_NavSubsystem, Error, "Failed to spawn AMBCG_DeathPlaceNavModifierVolume");
        return nullptr;
    }

    // Assign default NavArea class
    NavModifierVolume->SetAreaClass(UNavArea_Obstacle::StaticClass());

    // Assign Area Class Override with NavArea_Obstacle's subclasses depending on how many deaths this area corresponds to.
    // Each NavArea_Obstacle's subclass (NavArea_Obstacle_TierXX_MBCG) already has DefaultCost specified.
    TSubclassOf<UNavArea> AreaClassOverride = GetNavAreaObstacleTierClass(DeathPlacement.DeathQuantity);
    if (!AreaClassOverride)
    {
        UE_LOGFMT(LogUMBCG_NavSubsystem, Error, "SpawnDeathPlaceNavModifierVolume(): TSubclassOf<UNavArea> AreaClass is not defined.");
        return nullptr;
    }
    UShapeComponent* ShapeComponent = NavModifierVolume->GetBoxComponent();
    if (!ShapeComponent)
    {
        UE_LOGFMT(LogUMBCG_NavSubsystem, Error, "SpawnDeathPlaceNavModifierVolume(): ShapeComponent is not valid.");
        return nullptr;
    }
    // FYI: e.g. AreaClassOverride == UNavArea_Obstacle_Tier01_MBCG::StaticClass()
    ShapeComponent->SetAreaClassOverride(AreaClassOverride);  // getter: GetDesiredAreaClass()

#if 0
    // COP: Each NavArea_Obstacle_TierXX_MBCG class already has DefaultCost specified, so there is no need to dynamically change their DefaultCost.
    // Left FYI
    /*
    // set cost for the NavArea
    UNavArea_Obstacle* NavAreaObstacle = Cast<UNavArea_Obstacle>(AreaClassOverride->GetDefaultObject());
    if (NavAreaObstacle)
    {
        NavAreaObstacle->DefaultCost = CalculateNavAreaObstacleCost(DeathPlacement.DeathQuantity);

        UE_LOGFMT(LogUMBCG_NavSubsystem, Display, "- AreaClassOverride->GetName() = {0}, Deaths = {1}", AreaClassOverride->GetName(), DeathPlacement.DeathQuantity);
        UE_LOGFMT(LogUMBCG_NavSubsystem, Display,
            "- SpawnDeathPlaceNavModifierVolume:\n"
            "   DeathPlacement.DeathQuantity = {0},\n"
            "   NavAreaObstacle->DefaultCost = {1},\n"
            "   PlacementID = {2},\n"
            "   NavModifierVolume->GetAreaClass() = {3},\n"
            "   NavAreaObstacle = {4},\n"
            "   AreaClassOverride: NavModifierVolume->GetBoxComponent()->GetDesiredAreaClass() = {5}",
            DeathPlacement.DeathQuantity,                   //
            NavAreaObstacle->DefaultCost,                   //
            DeathPlacement.DeathPlacementID,                //
            *NavModifierVolume->GetAreaClass()->GetName(),  //
            *NavAreaObstacle->GetName(),                    //
            *NavModifierVolume->GetBoxComponent()->GetDesiredAreaClass()->GetName());
    }
    else
    {
        UE_LOGFMT(LogUMBCG_NavSubsystem, Error, "SpawnDeathPlaceNavModifierVolume(): NavAreaObstacle is not defined.");
    }
    */
#endif

    // UBoxComponent to define the NavModifierVolume's bounds for a desired effect on NavMesh
    UBoxComponent* BoxComponent = NavModifierVolume->GetBoxComponent();
    if (BoxComponent)
    {
        // height should be enough to let the volume reach the ground since the death place location is generated at the killed pawn location which is above the ground
        float VolumeHeight = 100.f;
        BoxComponent->SetBoxExtent(FVector(DeathNavModifierVolumeHalfSize - AgentRadius, DeathNavModifierVolumeHalfSize - AgentRadius, VolumeHeight));

#if 0
        // COP: debug volume bounds
        /*
        FVector Min, Max;
        NavModifierVolume->GetActorBounds(true, Min, Max, true);
        UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "- SpawnDeathPlaceNavModifierVolume(): NavModifierVolume->GetActorBounds = {0}, {1}, DeathNavModifierVolumeHalfSize = {2}",  //
            Min.ToString(), Max.ToString(), DeathNavModifierVolumeHalfSize);
        */
#endif
    }
    else
    {
        UE_LOGFMT(LogUMBCG_NavSubsystem, Error, "SpawnDeathPlaceNavModifierVolume(): NavModifierVolume's BoxComponent is null.");
    }

    NavModifierVolume->FinishSpawning(VolumeTransform);

#if 0
    // COP: debug and experiments, left just in case
    /*
    // Force update transforms
    USceneComponent* RootComponent = NavModifierVolume->GetRootComponent();
    if (RootComponent)
    {
            UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "- before UpdateBounds");
        RootComponent->UpdateBounds();  //->UpdateComponentTransforms();
        UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "- before UpdateComponentToWorld()");
        // Update the root component transform to ensure bounds reflect
        RootComponent->UpdateComponentToWorld();
    }
    else
    {
        UE_LOGFMT(LogUMBCG_NavSubsystem, Error, "NavModifierVolume's RootComponent is null.");
    }
    */
#endif

    return NavModifierVolume;
}


void UMBCG_NavSubsystem::DestroyRespawnNavModifierVolumeByDeathPlacements(const TArray<int32>& SpecifiedDeathPlacementsIDs)
{
    UWorld* World = GetWorld();
    if (!World) return;

    // Note: DeathPlacements array should be already changed by this time in accordance with SpecifiedDeathPlacementsIDs

    // input check
    if (SpecifiedDeathPlacementsIDs.Num() == 0)
    {
        UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "DestroyRespawnNavModifierVolumeByDeathPlacements(): Unexpected: SpecifiedDeathPlacementsIDs is empty.");
    }

    // for safe writing elements to the array in DeathPlacements's range
    if (DeathNavModifierVolumes.Num() < DeathPlacements.Num())
    {
        DeathNavModifierVolumes.SetNum(DeathPlacements.Num());
    }


    // destroy the nav modifier volumes
    for (int32 idx = 0; idx < SpecifiedDeathPlacementsIDs.Num(); ++idx)
    {
        if (SpecifiedDeathPlacementsIDs[idx] >= 0)
        {
            // DeathNavModifierVolumes are in accordance with DeathPlacements and clusters by array index
            DestroySingleNavModifierVolume(DeathNavModifierVolumes, idx);
        }
    }

    // Empty bounds check
    if (FMath::IsNearlyEqual(DeathNavModifierVolumeHalfSize, 0.f))
    {
        UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "DeathNavModifierVolumeHalfSize is zero or nearly zero.");
    }

    // Spawn NavModifierVolumes according to specified DeathPlacements and remember the spawned volumes in DeathNavModifierVolumes
    for (const int32 DeathPlacementID : SpecifiedDeathPlacementsIDs)
    {
        if (DeathPlacementID == -1) continue;

        // DeathPlacements ID == corrrespnding array index
        const FDeathPlacement& DeathPlacement = DeathPlacements[DeathPlacementID];

        // Skip invalid placements
        if (!DeathPlacement.IsValid) continue;

        AMBCG_DeathPlaceNavModifierVolume* NavModifierVolume = SpawnDeathPlaceNavModifierVolume(DeathPlacement);
        if (!NavModifierVolume)
        {
            UE_LOGFMT(LogUMBCG_NavSubsystem, Error, "DestroyRespawnNavModifierVolumeByDeathPlacements(): Unexpected: Spawning NavModifierVolume failed for DeathPlacement: {0}.",  //
                DeathPlacement.ToString());

            continue;
        }

        // debug
        // UE_LOGFMT(LogUMBCG_NavSubsystem, Display, "- NavModifierVolume (ID = {0}) spawned.", DeathPlacementID);

        // remember the DeathNavModifierVolume actor at the array index which corresponds to DeathPlacement (and also to attack clusters)
        DeathNavModifierVolumes[DeathPlacementID] = NavModifierVolume;

#if 0
        // COP: Log bounds to check validity
        /*
        FVector Min, Max;
        NavModifierVolume->GetActorBounds(true, Min, Max);
        UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "- NavModifierVolume's bounds: Min: {0}, Max: {1}, DeathNavModifierVolumeHalfSize = {2}",  //
            Min.ToString(), Max.ToString(), DeathNavModifierVolumeHalfSize);
        */
#endif
    }
}


void UMBCG_NavSubsystem::ApplyDeathPlacements(bool bProcessAll, const TArray<int32>& SpecifiedDeathPlacementsIDs)
{
    // input check
    if (!bProcessAll && SpecifiedDeathPlacementsIDs.Num() == 0)
    {
        UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "ApplyDeathPlacements(): Unexpected: SpecifiedDeathPlacementsIDs is empty.");
    }

    // get Death Placement IDs to process
    TArray<int32> DeathPlacementsIDsToProcess;
    if (bProcessAll)
    {
        DeathPlacementsIDsToProcess.SetNum(DeathPlacements.Num());
        for (const FDeathPlacement& SingleDeathPlacement : DeathPlacements)
        {
            DeathPlacementsIDsToProcess[SingleDeathPlacement.DeathPlacementID] = SingleDeathPlacement.DeathPlacementID;
        }
    }
    else
    {
        DeathPlacementsIDsToProcess = SpecifiedDeathPlacementsIDs;
    }

    DestroyRespawnNavModifierVolumeByDeathPlacements(DeathPlacementsIDsToProcess);
}


#if 0
// COP: BAK
/*
void UMBCG_NavSubsystem::ApplyDeathPlacements()
{
    UWorld* World = GetWorld();
    if (!World) return;

    // destroy all DeathNavModifierVolumes
    DestroyNavModifierVolumes(DeathNavModifierVolumes);

    // for safe writing elements to the array in DeathPlacements's range
    DeathNavModifierVolumes.SetNum(DeathPlacements.Num());

    // Empty bounds check
    if (FMath::IsNearlyEqual(DeathNavModifierVolumeHalfSize, 0.f))
    {
        UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "DeathNavModifierVolumeHalfSize is zero or nearly zero.");
    }

    // Spawn NavModifierVolumes according to DeathPlacements and remember the spawned volumes in DeathNavModifierVolumes
    for (const FDeathPlacement& DeathPlacement : DeathPlacements)
    {
        // Skip invalid placements
        if (!DeathPlacement.IsValid)
        {
            continue;
        }

        AMBCG_DeathPlaceNavModifierVolume* NavModifierVolume = SpawnDeathPlaceNavModifierVolume(DeathPlacement);
        if (!NavModifierVolume) continue;

        // remember the DeathNavModifierVolume actor at the array index which corresponds to DeathPlacement (and also to attack clusters)
        DeathNavModifierVolumes[DeathPlacement.DeathPlacementID] = NavModifierVolume;
    }
}
*/
#endif


float UMBCG_NavSubsystem::GetNavAreaCostAtLocation(const FVector& TestLocation) const
{
    UWorld* World = GetWorld();

    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
    if (NavSys)
    {
        const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
        const ARecastNavMesh* RecastNavMesh = Cast<ARecastNavMesh>(NavData);

        if (RecastNavMesh)
        {
            // Project the point to the navmesh
            FNavLocation NavLocation;
            if (RecastNavMesh->ProjectPoint(TestLocation, NavLocation, FVector::ZAxisVector * 45.f))  // 45.f is the projection's extent
            {
                // Get the area class associated with the polygon
                uint8 AreaID = 0;
                AreaID = RecastNavMesh->GetPolyAreaID(NavLocation.NodeRef);
                if (AreaID != RECAST_NULL_AREA)
                {
                    const UClass* AreaClass = RecastNavMesh->GetAreaClass(AreaID);
                    if (AreaClass)
                    {
                        const UNavArea* NavArea = Cast<UNavArea>(AreaClass->GetDefaultObject());
                        if (NavArea)
                        {
                            const float AreaCost = NavArea->DefaultCost;
                            UE_LOGFMT(LogUMBCG_NavSubsystem, Display, "Location: {0}, Area Cost: {1}, NavArea: {2}", TestLocation.ToString(), AreaCost, *NavArea->GetName());
                            return AreaCost;
                        }
                        else
                        {
                            UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "Failed to get NavArea from AreaClass.");
                        }
                    }
                    else
                    {
                        UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "Failed to get AreaClass for AreaID {0}.", AreaID);
                    }
                }
                else
                {
                    UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "Failed to get AreaID for NavLocation.");
                }
            }
            else
            {
                UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "TestLocation {0} is not on the navmesh!", TestLocation.ToString());
            }
        }
        else
        {
            UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "RecastNavMesh is not available.");
        }
    }
    else
    {
        UE_LOGFMT(LogUMBCG_NavSubsystem, Warning, "NavigationSystemV1 is not available.");
    }

    return -1.f;
}
