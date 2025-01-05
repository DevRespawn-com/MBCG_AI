// Copyright DevRespawn.com (MBCG). All Rights Reserved.

#include "MBCG/AI/Subsystems/MBCG_NPCAmbushAvaisionSubsystem.h"
#include "Logging/StructuredLog.h"


DEFINE_LOG_CATEGORY_STATIC(LogUMBCG_NPCAmbushAvaisionSubsystem, All, All);


void UMBCG_NPCAmbushAvaisionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}


void UMBCG_NPCAmbushAvaisionSubsystem::PostInitialize()
{
    AttackClusteringSubsystem = GetWorld()->GetSubsystem<UMBCG_AttackClusteringSubsystem>();
    NavSubsystem = GetWorld()->GetSubsystem<UMBCG_NavSubsystem>();

    check(AttackClusteringSubsystem);
    check(NavSubsystem);

    // this hub subsystem listens to the AttackClusteringSubsystem's delegate to update the navmesh
    AttackClusteringSubsystem->OnAttackClustersChangedDelegate.AddDynamic(this, &UMBCG_NPCAmbushAvaisionSubsystem::OnAttackClustersChanged);
    AttackClusteringSubsystem->OnSomeAttackClustersChangedDelegate.AddDynamic(this, &UMBCG_NPCAmbushAvaisionSubsystem::OnSomeAttackClustersChanged);
    // OnSomeAttackClustersChanged(const TArray<int32>& ChangedClustersIDsPayload)

    // make DeathNavModifierVolume a similar size as cluster
    NavSubsystem->SetDeathNavModifierVolumeHalfSize(AttackClusteringSubsystem->GetMaxClusterRadius());
}


void UMBCG_NPCAmbushAvaisionSubsystem::Deinitialize()
{
    AttackClusteringSubsystem->OnAttackClustersChangedDelegate.RemoveDynamic(this, &UMBCG_NPCAmbushAvaisionSubsystem::OnAttackClustersChanged);
    AttackClusteringSubsystem->OnSomeAttackClustersChangedDelegate.RemoveDynamic(this, &UMBCG_NPCAmbushAvaisionSubsystem::OnSomeAttackClustersChanged);

    Super::Deinitialize();
}


void UMBCG_NPCAmbushAvaisionSubsystem::RegisterNewAttack(                   //
    const FVector& InstigatorLocation, const FVector& InstigatorDirection,  //
    const EAttackRegistrationType& AttackRegistrationType,                  //
    const FVector& VictimLocation, const FVector& VictimDirection)
{
    if (AttackRegistrationType == EAttackRegistrationType::OnlyInstigator)
    {
        AttackClusteringSubsystem->RegisterNewClusterEntry(InstigatorLocation, InstigatorDirection, EEntryType::Instigator);
    }
    if (AttackRegistrationType == EAttackRegistrationType::OnlyVictim)
    {
        AttackClusteringSubsystem->RegisterNewClusterEntry(VictimLocation, VictimDirection, EEntryType::Victim);
    }
    if (AttackRegistrationType == EAttackRegistrationType::InstigatorAndVictim)
    {
        // Cluster are independently grouped by EEntryType
        AttackClusteringSubsystem->RegisterNewClusterEntry(InstigatorLocation, InstigatorDirection, EEntryType::Instigator);
        AttackClusteringSubsystem->RegisterNewClusterEntry(VictimLocation, VictimDirection, EEntryType::Victim);
    }
}


void UMBCG_NPCAmbushAvaisionSubsystem::GetDeathPlacementsFromAttackClusters(const TArray<FAttackCluster>& AttackClusters, TArray<FDeathPlacement>& DeathPlacementsFromClusters /* Target */)
{
    DeathPlacementsFromClusters.Empty();
    DeathPlacementsFromClusters.SetNum(AttackClusters.Num());

    for (int32 idx = 0; idx < AttackClusters.Num(); ++idx)
    {
        FDeathPlacement DeathPlacement;
        const FAttackCluster& Cluster = AttackClusters[idx];
        if (Cluster.ClusterID >= 0 && Cluster.EntryType == EEntryType::Victim)
        {
            DeathPlacement.DeathPlacementID = Cluster.ClusterID;
            DeathPlacement.DeathQuantity = Cluster.EntryIDs.Num();
            DeathPlacement.Location = Cluster.CentroidLocation;
            DeathPlacement.IsValid = Cluster.IsValid;
        }
        // by default DeathPlacement is invalid
        DeathPlacementsFromClusters[idx] = DeathPlacement;
    }
}


void UMBCG_NPCAmbushAvaisionSubsystem::ProcessAttackClustersChanged(bool bAllClustersChanged /* = true*/, const TArray<int32>& ChangedClustersIDs /* = {}*/)
{
    // re-write NavSubsysytem's DeathPlacements with data from AttackClusters
    const TArray<FAttackCluster>& AttackClusters = AttackClusteringSubsystem->GetClusters();
    TArray<FDeathPlacement> DeathPlacementsFromClusters;
    GetDeathPlacementsFromAttackClusters(AttackClusters, DeathPlacementsFromClusters);
    NavSubsystem->SetDeathPlacements(DeathPlacementsFromClusters);

    // Let NavSubsystem deal with the updated DeathPlacements
    if (bAllClustersChanged)
    {
        // apply all death placements
        NavSubsystem->ApplyDeathPlacements();
    }
    else
    {
        // apply only the death placements for the specified clusters
        NavSubsystem->ApplyDeathPlacements(false /* bProcessAll */, ChangedClustersIDs);
    }
}


void UMBCG_NPCAmbushAvaisionSubsystem::OnAttackClustersChanged()
{
    ProcessAttackClustersChanged(true /* bAllClustersChanged */, {} /* ChangedClustersIDs */);
}


void UMBCG_NPCAmbushAvaisionSubsystem::OnSomeAttackClustersChanged(const TArray<int32>& ChangedClustersIDsPayload)
{
    // input check
    if (ChangedClustersIDsPayload.Num() == 0)
    {
        UE_LOGFMT(LogUMBCG_NPCAmbushAvaisionSubsystem, Warning, "OnSomeAttackClustersChanged(): Unexpected: ChangedClustersIDsPayload is empty.");
    }

    ProcessAttackClustersChanged(false /* bAllClustersChanged */, ChangedClustersIDsPayload /* ChangedClustersIDs */);
}

#if 0
/* BAK
void UMBCG_NPCAmbushAvaisionSubsystem::OnSomeAttackClustersChanged(const TArray<int32>& ChangedClustersIDsPayload)
{
    const TArray<FAttackCluster>& AttackClusters = AttackClusteringSubsystem->GetClusters();
    TArray<FDeathPlacement> DeathPlacementsFromClusters;
    ConvertAttackClustersToDeathPlacements(AttackClusters, DeathPlacementsFromClusters);
    UpdateDeathPlacements(AttackClusters, DeathPlacementsFromClusters, ChangedClustersIDsPayload);

    // Clusters and DeathPlacmenets are in full accordance by index and IDs
    // UpdateDeathPlacements();
    NavSubsystem->SetDeathPlacements(DeathPlacementsFromClusters);
    NavSubsystem->ApplyDeathPlacements(ChangedClustersIDsPayload);
}*/
#endif