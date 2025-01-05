// Copyright DevRespawn.com (MBCG). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MBCG/AI/Subsystems/MBCG_AttackClusteringSubsystem.h"
#include "MBCG/AI/Subsystems/MBCG_NavSubsystem.h"
#include "MBCG_NPCAmbushAvaisionSubsystem.generated.h"

/**
 * This is a managing hub for subsystems that allow NPC to avoid ambush.
 * Other subsystems that are called from this subsystem:
 * - MBCG_AttackClusteringSubsystem (this subsystem tracks where and as well as from where NPC was killed. The core of this susbsystem is clasterizing locations.
 * - MBCG_NavSubsystem (this subsystem helps NPC avoid the ambush places (e.g. found by MBCG_AttackClusteringSubsystem).
 */


#if 0
// COP: Commented because AttackType is not determined in Blueprints for now.
/*
UENUM(BlueprintType)
enum class EAttackType : uint8
{
    Default,
    Melee,
    RangedWeapon,
    Grenade
};
*/
#endif


// This enumerates the attack types specified by RegisterNewAttack's users (e.g. when calling RegisterNewAttack from Blueprints)
// Depending on the value one or two types of clusters will be formed
UENUM(BlueprintType)
enum class EAttackRegistrationType : uint8
{
    OnlyInstigator,  // Default
    OnlyVictim,
    InstigatorAndVictim
};


UCLASS()
class LYRAGAME_API UMBCG_NPCAmbushAvaisionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void PostInitialize() override;
    virtual void Deinitialize() override;

public:

    // Create one or more cluster entires (but no more than one entry of each EntryType), place them into a cluster of the corresponding type, adjust clusters if needed.
    // VictimDirection is not used for now.
    // This function calls functionality from MBCG_NPCAmbushAvaisionSubsystem.
    UFUNCTION(BlueprintCallable, Category = "NPC Ambush Avaision Subsystem")
    void RegisterNewAttack(                                                                               //
        const FVector& InstigatorLocation,                                                                //
        const FVector& InstigatorDirection = FVector::ZeroVector,                                         //
        const EAttackRegistrationType& AttackRegistrationType = EAttackRegistrationType::OnlyInstigator,  //
        const FVector& VictimLocation = FVector::ZeroVector,                                              //
        const FVector& VictimDirection = FVector::ZeroVector);

private:

    // subsystems
    UMBCG_AttackClusteringSubsystem* AttackClusteringSubsystem;
    UMBCG_NavSubsystem* NavSubsystem;

private:

    // callback function when it's supposed that all clusters changed
    UFUNCTION()
    void OnAttackClustersChanged();
    // callback function when it's supposed that specified clusters changed
    // @param ChangedClustersIDsPayload IDs of the clusters which were changed
    UFUNCTION()
    void OnSomeAttackClustersChanged(const TArray<int32>& ChangedClustersIDsPayload);
    // Calls MBCG_NavSubsystem's function to re-spawn NavModifiers after attack clusters were changed
    // @param bAllClustersChanged True if all clusters were changed, Flase if specified clusters were changed
    // @param ChangedClustersIDs IDs of changed clusters (bAllClustersChanged should be True to consider this parameter)
    void ProcessAttackClustersChanged(bool bAllClustersChanged /* = true*/, const TArray<int32>& ChangedClustersIDs /* = {}*/);

    // Clear and fill in DeathPlacementsFromClusters by copying relevant data from AttackClusters to adapt the data (DeathPlacementsFromClusters) for using in NavSubsystem.
    // Only Victims' clusters's data is copied.
    // Correspondence with AttackClusters by index is maintained, as well as DeathPlacement.DeathPlacementID == Cluster.ClusterID.
    // @param AttackClusters Source array
    // @param DeathPlacementsFromClusters Target array (emptied first)
    void GetDeathPlacementsFromAttackClusters(const TArray<FAttackCluster>& AttackClusters, TArray<FDeathPlacement>& DeathPlacementsFromClusters /* Target */);
};
