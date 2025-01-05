// Copyright DevRespawn.com (MBCG). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NavigationSystem.h" // for UNavigationSystemV1
#include "NavModifierVolume.h"
#include "AI/Navigation/NavAreaBase.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "MBCG/AI/NavAreas/NavArea_Obstacle_Tier01_MBCG.h"
#include "MBCG/AI/NavAreas/NavArea_Obstacle_Tier02_MBCG.h"
#include "MBCG/AI/NavAreas/NavArea_Obstacle_Tier03_MBCG.h"
#include "MBCG/AI/NavAreas/NavArea_Obstacle_Tier04_MBCG.h"
#include "MBCG/AI/NavAreas/NavArea_Obstacle_Tier05_MBCG.h"
#include "MBCG/AI/NavAreas/NavArea_Obstacle_Tier06_MBCG.h"
#include "MBCG/AI/NavAreas/NavArea_Obstacle_Tier07_MBCG.h"
#include "MBCG/AI/NavAreas/NavArea_Obstacle_Tier08_MBCG.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"  // for GetNavAreaCostAtLocation()
#include "Components/ShapeComponent.h"
#include "Components/BoxComponent.h"
#include "MBCG/AI/Volumes/MBCG_DeathPlaceNavModifierVolume.h"
#include "MBCG_NavSubsystem.generated.h"


/**
 * This subsystem can be used separately for managing Navigation for NPC (best way to go, places to avoid etc) or in conjunction with MBCG_NPCAmbushAvaisionSubsystem.
 * 
 * Basic usage: SetDeathPlacements() to define the locations of higher costs, then ApplyDeathPlacements() to apply these high-cost location to the NavMesh.
 */


// It serves as a mediator structure between UMBCG_AttackClusteringSubsystem to represent a attack cluster location
USTRUCT(BlueprintType)
struct FDeathPlacement
{
    GENERATED_BODY()

    // identifier, invalid by default
    UPROPERTY(BlueprintReadOnly)
    int32 DeathPlacementID = -1;

    // How many deaths have taken place in this place (equals FAttackCluster.EntryIDs.Num())
    UPROPERTY(BlueprintReadOnly)
    int32 DeathQuantity = 0;

    // Location center
    UPROPERTY(BlueprintReadOnly)
    FVector Location = FVector::ZeroVector;

    // Corresponds to FAttackCluster.IsValid
    UPROPERTY(BlueprintReadOnly)
    bool IsValid = false;

    // ToString function to convert the struct to a readable string
    FString ToString() const
    {
        return FString::Printf(TEXT("DeathPlacement[ID: %d, Deaths: %d, Location: (%0.1f, %0.1f, %0.1f), Valid: %s]"),  //
            DeathPlacementID, DeathQuantity, Location.X, Location.Y, Location.Z, IsValid ? TEXT("true") : TEXT("false"));
    }
};


// Returns UNavArea class depending on the input.
// By default or incorrect (Tier <= 0 or too big Tier) input the function returns UNavArea_Obstacle.
// This is required because we can't set different DefaultCost for different NavAreaObstacle, there fore we have to use different UNavArea classes.
static TSubclassOf<UNavArea> GetNavAreaObstacleTierClass(int32 Tier)
{
    switch (Tier)
    {
        case 1: return UNavArea_Obstacle_Tier01_MBCG::StaticClass();
        case 2: return UNavArea_Obstacle_Tier02_MBCG::StaticClass();
        case 3: return UNavArea_Obstacle_Tier03_MBCG::StaticClass();
        case 4: return UNavArea_Obstacle_Tier04_MBCG::StaticClass();
        case 5: return UNavArea_Obstacle_Tier05_MBCG::StaticClass();
        case 6: return UNavArea_Obstacle_Tier06_MBCG::StaticClass();
        case 7: return UNavArea_Obstacle_Tier07_MBCG::StaticClass();
        case 8: return UNavArea_Obstacle_Tier08_MBCG::StaticClass();
        default: return UNavArea_Obstacle::StaticClass();
    }
}


UCLASS()
class LYRAGAME_API UMBCG_NavSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void PostInitialize() override;
    virtual void Deinitialize() override;

public:

    // Get all Death Places
    // UFUNCTION(BlueprintCallable, Category = "Navigation Subsystem")
    const TArray<ANavModifierVolume*>& GetDeathNavModifierVolumes() const { return DeathNavModifierVolumes; }

    // Apply changes in navigation subsystem according to DeathPlacements (e.g. destroy and re-spawn corresponding NavModifierVolumes).
    // @param bProcessAll True: process all Death Placements. False: process only the Death Placements with specified IDs.
    // @param SpecifiedDeathPlacementsIDs IDs of Death Placements to process. bProcessAll must be False to consider it.
    void ApplyDeathPlacements(bool bProcessAll = true, const TArray<int32>& SpecifiedDeathPlacementsIDs = {});

    void SetDeathPlacements(const TArray<FDeathPlacement>& InDeathPlacements) { DeathPlacements = InDeathPlacements; }

    void SetDeathNavModifierVolumeHalfSize(float Radius) { DeathNavModifierVolumeHalfSize = Radius; }

    // For Debug only
    UFUNCTION(BlueprintCallable, Category = "NPC NavSystem|Debug")
    void DestroyAllDeathNavModifierVolumes_DEBUG() { DestroyNavModifierVolumes(DeathNavModifierVolumes); }

    // For debug. Returns -1 if error
    UFUNCTION(BlueprintCallable, Category = "NPC NavSystem|Debug")
    float GetNavAreaCostAtLocation(const FVector& TestLocation) const;

private:

    // list of locations adapted from UMBCG_AttackClusteringSubsystem to represent attack clusters where NPC died.
    // Accordance between DeathPlacements and UMBCG_AttackClusteringSubsystem.Clusters is by ID and index (i.e. DeathPlacements[idx].DeathPlacementID = Clusters[idx] = ClusterID)
    TArray<FDeathPlacement> DeathPlacements;

    // Nav modifer volumes that represent death places. Works in accordance with DeathPlacements (accordance by array index)
    TArray<ANavModifierVolume*> DeathNavModifierVolumes;

    // radius for round-shape NavModifierVolume (or dimension for square-shape volume)
    float DeathNavModifierVolumeHalfSize = 50.f;

    // Spawns custom NavModifierVolume with custom area Cost according to the specified death placement's data.
    // The more deaths are in the input placement, the bigger Cost the spawned NavModifierVolume will have
    // Returns nullptr and does not spawn anything if DeathPlacement.DeathQuantity <= 0
    AMBCG_DeathPlaceNavModifierVolume* SpawnDeathPlaceNavModifierVolume(const FDeathPlacement& DeathPlacement);

    // Calculate DefaultCost for NavAreaObstacle depending on number of deaths in the placement
    // NOT USED because each NavArea_Obstacle_TierXX_MBCG class already has DefaultCost specified, so there is no need to dynamically change their DefaultCost.
    // @param DeathQuantity  Number of deaths in the placement
    float CalculateNavAreaObstacleCost(int32 DeathQuantity) const;

    // destroy NavModifierVolume actors and set nullptr to corresponding elements in VolumesToDestroy array
    // @param NavModifierVolumes Array of NavModifierVolumes to destroy
    void DestroyNavModifierVolumes(TArray<ANavModifierVolume*>& VolumesToDestroy);
    // destroy a single NavModifierVolume actor and set nullptr to corresponding element in VolumesToDestroy array
    // @param NavModifierVolumes Array of NavModifierVolumes
    // @param idx Index in the NavModifierVolumes array elements of which should be destroyed
    void DestroySingleNavModifierVolume(TArray<ANavModifierVolume*>& VolumesToDestroy, int32 idx);

    // Destroys and re-spawns NavModifierVolumes listed in DeathNavModifierVolumes for the specified Death Placements IDs.
    // Note: DeathPlacements must be already up-to-date because this function relies on its data.
    // Note: NavMesh must be not static (e.g. it should be "Dynamic Modifiers Only") in order to apply the added NavModifierVolumes automatically.
    // @param SpecifiedDeathPlacementsIDs Process only the NavModifierVolumes which are represented by this parameter.
    void DestroyRespawnNavModifierVolumeByDeathPlacements(const TArray<int32>& SpecifiedDeathPlacementsIDs);

    // agent radius used for calculating generated navmesh non-traversable areas near obstacles
    float AgentRadius = 0.f;
    // get agent radus from settings
    float GetAgentRaidusFromSettings(UWorld* World) const;

};
