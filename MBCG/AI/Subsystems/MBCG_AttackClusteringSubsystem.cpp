// Copyright DevRespawn.com (MBCG). All Rights Reserved.

#include "MBCG/AI/Subsystems/MBCG_AttackClusteringSubsystem.h"
#include "MBCG/FunctionLibraries/MBCG_BPFL_Utils.h"  // for SafeSetNum()
#include "Logging/StructuredLog.h"


DEFINE_LOG_CATEGORY_STATIC(LogUMBCG_AttackClusteringSubsystem, All, All);


namespace MBCG_AttackClusteringSubsystem_Debug
{
    int32 RecursionDepth = 0;

    // Remembers RecursionDepth as a maximum of input Depth argument
    static void SetRecursionDepthIfNeeded(const int32 Depth)
    {
        if (RecursionDepth < Depth)
        {
            RecursionDepth = Depth;
        }
    }

    static void PrintRecursionDepth()
    {
        UE_LOGFMT(LogUMBCG_AttackClusteringSubsystem, Display, "Maximum recursion depth recorded = {0}", RecursionDepth);
    }

    // reset the recursion depth counter
    static void ResetRecursionDepth()
    {
        RecursionDepth = 0;
    }
}  // namespace MBCG_AttackClusteringSubsystem_Debug


namespace LocalDebug = MBCG_AttackClusteringSubsystem_Debug;


void FAttackCluster::UpdateCentroidProperties(const TArray<FClusterEntry>& ClusterEntries)
{
    if (EntryIDs.Num() == 0)
    {
        CentroidLocation = FVector::ZeroVector;
        Direction = FVector::ZeroVector;
        return;
    }

    FVector LocationSum = FVector::ZeroVector;
    FVector DirectionSum = FVector::ZeroVector;

    for (int32 EntryID : EntryIDs)
    {
        LocationSum += ClusterEntries[EntryID].EntryLocation;
        DirectionSum += ClusterEntries[EntryID].EntryDirection;
    }

    CentroidLocation = LocationSum / EntryIDs.Num();
    Direction = DirectionSum.GetSafeNormal();
}


void UMBCG_AttackClusteringSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}


void UMBCG_AttackClusteringSubsystem::Deinitialize()
{
    Super::Deinitialize();

    // Clear all data
    ClusterEntries.Empty();
    Clusters.Empty();
}


bool UMBCG_AttackClusteringSubsystem::SoftCheckCluster(int32 ClusterID) const
{
    if (ClusterID == -1) return false;
    if (!Clusters[ClusterID].IsValid) return false;
    if (Clusters[ClusterID].EntryIDs.Num() == 0) return false;

    return true;
}


bool UMBCG_AttackClusteringSubsystem::SoftCheckClusterEntry(int32 EntryID) const
{
    if (EntryID == -1) return false;

    return true;
}


void UMBCG_AttackClusteringSubsystem::AddToChangedClustersPayloadIfNeeded(int32 ClusterID)
{
    // check
    if (ClusterID < 0)
    {
        UE_LOGFMT(LogUMBCG_AttackClusteringSubsystem, Error, "AddToChangedClustersPayloadIfNeeded(): Incorrect NewClusterID input parameter.");
    }

    // increase ChangedClustersIDsPayload array if needed
    if (ChangedClustersIDsPayload.Num() <= ClusterID)
    {
        UMBCG_BPFL_Utils::SafeSetNum(ChangedClustersIDsPayload, ClusterID + 1, -1);
    }

    // add element
    ChangedClustersIDsPayload[ClusterID] = ClusterID;
}


void UMBCG_AttackClusteringSubsystem::AddToChangedClustersPayloadIfNeeded(const TArray<int32>& ClusterIDs)
{
    // increase ChangedClustersIDsPayload array if needed
    if (ChangedClustersIDsPayload.Num() < ClusterIDs.Num())
    {
        UMBCG_BPFL_Utils::SafeSetNum(ChangedClustersIDsPayload, ClusterIDs.Num(), -1);
    }

    for (const int32 ClusterID : ClusterIDs)
    {
        if (ClusterID >= 0)
        {
            AddToChangedClustersPayloadIfNeeded(ClusterID);
        }
    }
}


float UMBCG_AttackClusteringSubsystem::CalculateClusterReciprocalGravityEffect(float DistanceToCluster, int32 ClusterID) const
{
    // check input
    if (!SoftCheckCluster(ClusterID)) return FLT_MAX;
    if (DistanceToCluster < 0) return FLT_MAX;

    // Check if outside the cluster boundaries
    if (DistanceToCluster > MaxClusterRadius) return FLT_MAX;

    return (DistanceToCluster * DistanceToCluster) / (ClusterGravity * Clusters[ClusterID].EntryIDs.Num());
}


int32 UMBCG_AttackClusteringSubsystem::FindBestCluster(const FClusterEntry& ClusterEntry) const
{
    int32 BestClusterIndex = -1;
    float BestScore = FLT_MAX;

    // Find if the new cluster entry is located witin already existing cluster's radius
    for (int32 i = 0; i < Clusters.Num(); ++i)
    {
        if (!Clusters[i].IsValid || Clusters[i].EntryType != ClusterEntry.EntryType) continue;

        // Skip the the current cluster if the cluster entry is the only entry in this cluster
        // This allows a single-entry cluster to be moved to another cluster
        if (ClusterEntry.EntryID == i && Clusters[i].EntryIDs.Num() == 1) continue;

        const FAttackCluster& CurrentCluster = Clusters[i];
        float Distance = FVector::Dist(CurrentCluster.CentroidLocation, ClusterEntry.EntryLocation);

        // score as equivalent of reciporcal of gravity
        float ClusterScore = FLT_MAX;

        // COP: Don't consider cluster entry Direction for now
        // Check if the cluster entry aligns directionally
        // if (FVector::DotProduct(Cluster.Direction, ClusterEntry.Direction) >= CosMaxAmbushSectorDegrees)

        // Skip clusters where the cluster entry falls outside their radius
        if (Distance <= MaxClusterRadius)
        {
            ClusterScore = CalculateClusterReciprocalGravityEffect(Distance, CurrentCluster.ClusterID);

            // Prioritize clusters based on proximity, weighted by their "heaviness" (lower ClusterScore indicates higher priority)
            if (ClusterScore < BestScore)
            {
                BestScore = ClusterScore;
                BestClusterIndex = i;
            }
        }
    }

    if (BestClusterIndex != -1)
    {
        return BestClusterIndex;
    }

    // Consider joining the new cluster entry to the closest single-entry cluster
    if (BestClusterIndex == -1)
    {
        for (int32 i = 0; i < Clusters.Num(); ++i)
        {
            // considering only valid clusters with the same EntryType with only one entry
            if (!Clusters[i].IsValid || Clusters[i].EntryIDs.Num() != 1 || Clusters[i].EntryType != ClusterEntry.EntryType) continue;

            const FAttackCluster& CurrentCluster = Clusters[i];
            float Distance = FVector::Dist(CurrentCluster.CentroidLocation, ClusterEntry.EntryLocation);
            // The new cluster entry and the existing single-entry cluster should be no further from each other than a cluster's diameter
            if (Distance <= MaxClusterRadius * 2)
            {
                // Prioritize closer clusters
                if (Distance < BestScore)
                {
                    BestScore = Distance;
                    BestClusterIndex = i;
                }
            }
        }
    }

    return BestClusterIndex;
}


void UMBCG_AttackClusteringSubsystem::HandleEntriesInOverlappingClusters(const EEntryType EntryType, int32 Depth)
{
    LocalDebug::SetRecursionDepthIfNeeded(Depth);
    if (Depth > MaxRecursionDepth)
    {
        UE_LOGFMT(LogUMBCG_AttackClusteringSubsystem, Warning, "HandleEntriesInOverlappingClusters(): Reached recursion depth limit. Halting clustering adjustments in In overlapping clusters.");
        return;
    }

    // clusters which were affected by moved cluster entries
    TArray<int32> AffectedClusterIDs;
    // Cluster entries which are expelled from their former clusters due to clusters centroid shifting when handling overlapping clusters
    TArray<int32> ExpelledClusterEntryIDs;

    // Find cluster entries which should be moved to a different cluster and assign the most suitable cluster to them without changes of cluster centroids
    for (FClusterEntry& SingleClusterEntry : ClusterEntries)
    {
        // Only clusters of the specified EntryType to be processed
        if (SingleClusterEntry.EntryType != EntryType) continue;

        int32 BestClusterIndex = FindBestCluster(SingleClusterEntry);
        // cluster entry should be expelled and handled additionally
        if (BestClusterIndex == -1)
        {
            ExpelledClusterEntryIDs.Add(SingleClusterEntry.EntryID);

            continue;
        }
        // cluster entry should be moved to a different cluster
        if (SingleClusterEntry.ClusterID != BestClusterIndex)
        {
            // remove cluster entry from a former cluster
            int32 OldClusterID = SingleClusterEntry.ClusterID;
            FAttackCluster& OldCluster = Clusters[OldClusterID];
            ClusterEntries[SingleClusterEntry.EntryID].ClusterID = -1;
            OldCluster.EntryIDs.Remove(SingleClusterEntry.EntryID);
            AffectedClusterIDs.Add(OldClusterID);

            // assign the cluster entry to the most suitable cluster
            ClusterEntries[SingleClusterEntry.EntryID].ClusterID = BestClusterIndex;
            FAttackCluster& BestCluster = Clusters[BestClusterIndex];
            BestCluster.EntryIDs.Add(SingleClusterEntry.EntryID);
            AffectedClusterIDs.Add(BestClusterIndex);
        }
    }

    // for all modified clusters:UpdateCentroidProperties()
    for (int32 ClusterID : AffectedClusterIDs)
    {
        Clusters[ClusterID].UpdateCentroidProperties(ClusterEntries);
    }

    // Handle cluster entries which were expelled from their former clusters
    for (int32 EntryID : ExpelledClusterEntryIDs)
    {
        IntegrateClusterEntry(ClusterEntries[EntryID]);
    }

    // update ChangedClustersIDsPayload
    AddToChangedClustersPayloadIfNeeded(AffectedClusterIDs);

    // If any cluster shifted, then cluster entries may need to be assigned to some other clusters
    if (AffectedClusterIDs.Num() > 0)
    {
        HandleEntriesInOverlappingClusters(EntryType, Depth + 1);
    }
}


bool UMBCG_AttackClusteringSubsystem::AreClustersFullyOverlapping(int32 SourceClusterID, int32 TargetClusterID) const
{
    // check input
    if (!SoftCheckCluster(SourceClusterID) || !SoftCheckCluster(TargetClusterID)) return false;

    for (int32 EntryID : Clusters[SourceClusterID].EntryIDs)
    {
        float Distance = FVector::Dist(Clusters[TargetClusterID].CentroidLocation, ClusterEntries[EntryID].EntryLocation);
        if (Distance > MaxClusterRadius)
        {
            return false;
        }
    }

    return true;
}


int32 UMBCG_AttackClusteringSubsystem::FindBestMasterClusterCandidate(int32 SourceClusterID, const TArray<int32>& MasterCandidateClusterIDs) const
{
    // check input
    if (!SoftCheckCluster(SourceClusterID)) return -1;

    float BestScore = FLT_MAX;
    int32 BestClusterID = -1;

    for (int32 MasterClusterID : MasterCandidateClusterIDs)
    {
        if (!SoftCheckCluster(MasterClusterID)) continue;

        // check of function argument correctness
        if (Clusters[SourceClusterID].EntryType != Clusters[MasterClusterID].EntryType)
        {
            UE_LOGFMT(LogUMBCG_AttackClusteringSubsystem, Warning,
                "FindBestMasterClusterCandidate(): Argument correctness warning. EntryType mismatch in SourceClusterID and MasterCandidateClusterIDs arguments. Check the arguments of this function. "
                "The function will continue ignoring the clusters with mismatched EntryType.");
            continue;
        }

        float Distance = FVector::Dist(Clusters[SourceClusterID].CentroidLocation, Clusters[MasterClusterID].CentroidLocation);
        float CurrentMasterCandidateClusterScore = CalculateClusterReciprocalGravityEffect(Distance, MasterClusterID);
        if (CurrentMasterCandidateClusterScore < BestScore)
        {
            BestScore = CurrentMasterCandidateClusterScore;
            BestClusterID = MasterClusterID;
        }
    }

    return BestClusterID;
}


bool UMBCG_AttackClusteringSubsystem::UniteClusters(const int32 MovedSourceClusterID, int32 BestMasterClusterID)
{
    // basic input check
    if (!SoftCheckCluster(MovedSourceClusterID) || !SoftCheckCluster(BestMasterClusterID)) return false;

    // check EntryType correctness
    if (Clusters[MovedSourceClusterID].EntryType != Clusters[BestMasterClusterID].EntryType)
    {
        UE_LOGFMT(LogUMBCG_AttackClusteringSubsystem, Warning,
            "UniteClusters(): Argument correctness warning. EntryType mismatch in input clusters. Check the arguments of this function. "
            "The function will abort.");
        return false;
    }

    // move cluster entries from source cluster to master cluster, invalidating the moved cluster
    TArray<int32> SourceCopyEntryIDs = Clusters[MovedSourceClusterID].EntryIDs;
    for (int32 MovedEntryID : SourceCopyEntryIDs)
    {
        Clusters[BestMasterClusterID].EntryIDs.Add(MovedEntryID);
        ClusterEntries[MovedEntryID].ClusterID = BestMasterClusterID;
    }
    Clusters[MovedSourceClusterID].EntryIDs.Empty();
    Clusters[MovedSourceClusterID].IsValid = false;
    Clusters[BestMasterClusterID].UpdateCentroidProperties(ClusterEntries);

    // update ChangedClustersIDsPayload with changed clusters IDs
    AddToChangedClustersPayloadIfNeeded({Clusters[MovedSourceClusterID].ClusterID, Clusters[BestMasterClusterID].ClusterID});

    // A cluster was changed, therefore its centroid moved, so perhaps its entries or other cluster's entries should be re-assigned to other clusters
    HandleEntriesInOverlappingClusters(Clusters[MovedSourceClusterID].EntryType);

    // return true if there were changes (by default), false if something went wrong
    return true;
}


void UMBCG_AttackClusteringSubsystem::FindAndUniteFullyOverlappingClusters(const EEntryType EntryType)
{
    bool ChangesOccurred = false;
    // Unite clusters until no further changes occur
    do
    {
        int32 MovedSourceClusterIdx = -1;
        int32 BestMasterClusterIdx = -1;
        bool ChangesExpected = false;

        // Go though all clusters and consider SourceCluster to be "absorbed" by the TargetCluster
        for (int32 SourceClusterIdx = 0; SourceClusterIdx < Clusters.Num() - 1; ++SourceClusterIdx)
        {
            // only valid clusters of the specified EntryType to be considered
            if (!Clusters[SourceClusterIdx].IsValid || Clusters[SourceClusterIdx].EntryType != EntryType) continue;

            // Clusters that are suitable to be masters when uniting with the current source cluster
            TArray<int32> MasterCandidateClusterIDs;
            for (int32 TargetClusterIdx = 0; TargetClusterIdx < Clusters.Num(); ++TargetClusterIdx)
            {
                if (SourceClusterIdx == TargetClusterIdx || !Clusters[TargetClusterIdx].IsValid || Clusters[TargetClusterIdx].EntryType != EntryType) continue;

                // It makes sense to consider uniting clusters if their centroids are no further than a cluster diameter from each other
                if (FVector::Dist(Clusters[SourceClusterIdx].CentroidLocation, Clusters[TargetClusterIdx].CentroidLocation) > 2 * MaxClusterRadius) continue;

                // Unite clusters if all entries of one of the cluster is within the other cluster's bounds
                if (AreClustersFullyOverlapping(SourceClusterIdx, TargetClusterIdx))
                {
                    MasterCandidateClusterIDs.Add(TargetClusterIdx);
                }
            }

            // Find the best cluster candidate to be a master for the current source cluster
            BestMasterClusterIdx = FindBestMasterClusterCandidate(SourceClusterIdx, MasterCandidateClusterIDs);
            if (BestMasterClusterIdx != -1)
            {
                MovedSourceClusterIdx = SourceClusterIdx;
                ChangesExpected = true;
                // Quit the loop since the clusters array has been changed and run the main loop again afresh
                break;
            }
        }

        if (ChangesExpected)
        {
            ChangesOccurred = UniteClusters(MovedSourceClusterIdx, BestMasterClusterIdx);
        }
        else
        {
            ChangesOccurred = false;
        }


    } while (ChangesOccurred);
}


void UMBCG_AttackClusteringSubsystem::RegisterNewClusterEntry(const FVector& EntryLocation, const FVector& EntryDirection, const EEntryType EntryType)
{
    LocalDebug::ResetRecursionDepth();

    FClusterEntry NewClusterEntry;
    NewClusterEntry.EntryID = ClusterEntries.Num();
    NewClusterEntry.EntryType = EntryType;
    NewClusterEntry.EntryLocation = EntryLocation;
    NewClusterEntry.EntryDirection = EntryDirection.GetSafeNormal();

    ClusterEntries.Add(NewClusterEntry);

    ChangedClustersIDsPayload.Empty();

    IntegrateClusterEntry(ClusterEntries[NewClusterEntry.EntryID]);
    HandleEntriesInOverlappingClusters(EntryType);
    FindAndUniteFullyOverlappingClusters(EntryType);

#if 0
    // Broadcast that clusters changed
    // COP: Use OnSomeAttackClustersChangedDelegate which is more efficient
    // OnAttackClustersChangedDelegate.Broadcast();
#endif
    // Braodcast that some clusters changed (or addeded, removed etc)
    OnSomeAttackClustersChangedDelegate.Broadcast(ChangedClustersIDsPayload);

    // Uncomment this to log out the maximum recursion depth that took place
    LocalDebug::PrintRecursionDepth();
}


int32 UMBCG_AttackClusteringSubsystem::CreateNewCluster(const FClusterEntry& ClusterEntry)
{
    // input check
    if (!SoftCheckClusterEntry(ClusterEntry.EntryID))
    {
        UE_LOGFMT(LogUMBCG_AttackClusteringSubsystem, Warning, "CreateNewCluster(): Wrong input: ClusterEntry.EntryID == -1.");
        return -1;
    }

    FAttackCluster NewCluster;
    NewCluster.ClusterID = Clusters.Num();
    NewCluster.EntryType = ClusterEntry.EntryType;
    NewCluster.EntryIDs.Add(ClusterEntry.EntryID);
    NewCluster.CentroidLocation = ClusterEntry.EntryLocation;
    NewCluster.Direction = ClusterEntry.EntryDirection;
    NewCluster.IsValid = true;

    Clusters.Add(NewCluster);

    // update ChangedClustersIDsPayload
    AddToChangedClustersPayloadIfNeeded(NewCluster.ClusterID);

    return NewCluster.ClusterID;
}


bool UMBCG_AttackClusteringSubsystem::HandleExpelledClusterEntries(const FAttackCluster& ClusterCopy, int32 Depth)
{
    // input check
    if (!SoftCheckCluster(ClusterCopy.ClusterID)) return false;

    LocalDebug::SetRecursionDepthIfNeeded(Depth);

    if (Depth > MaxRecursionDepth)
    {
        UE_LOGFMT(LogUMBCG_AttackClusteringSubsystem, Warning, "HandleExpelledClusterEntries(): Reached recursion depth limit. Halting clustering adjustments.");
        return false;
    }

    TArray<int32> ExpelledClusterEntryIDs;

    // Identify cluster entries to expel based on MaxClusterRadius
    for (int32 EntryID : ClusterCopy.EntryIDs)
    {
        float Distance = FVector::Dist(ClusterCopy.CentroidLocation, ClusterEntries[EntryID].EntryLocation);
        if (Distance > MaxClusterRadius)
        {
            ExpelledClusterEntryIDs.Add(EntryID);
        }
    }

    // If no cluster entries are expelled, nothing has changed
    if (ExpelledClusterEntryIDs.Num() == 0)
    {
        return false;
    }

    // Remove expelled cluster entries from the cluster
    for (int32 ExpelledEntryID : ExpelledClusterEntryIDs)
    {
        Clusters[ClusterCopy.ClusterID].EntryIDs.Remove(ExpelledEntryID);
        ClusterEntries[ExpelledEntryID].ClusterID = -1;  // Mark as unclustered
    }

    // Update cluster centroid after expulsion
    Clusters[ClusterCopy.ClusterID].UpdateCentroidProperties(ClusterEntries);

    // update ChangedClustersIDsPayload
    AddToChangedClustersPayloadIfNeeded(ClusterCopy.ClusterID);

    bool ChangesOccurred = false;

    // Re-assign expelled cluster entries, potentially forming new clusters
    for (int32 ExpelledEntryID : ExpelledClusterEntryIDs)
    {
        ChangesOccurred |= IntegrateClusterEntry(ClusterEntries[ExpelledEntryID], Depth + 1);
    }

    return ChangesOccurred;
}


bool UMBCG_AttackClusteringSubsystem::IntegrateClusterEntry(const FClusterEntry& ClusterEntry, int32 Depth)
{
    // input check
    if (!SoftCheckClusterEntry(ClusterEntry.EntryID))
    {
        UE_LOGFMT(LogUMBCG_AttackClusteringSubsystem, Warning, "IntegrateClusterEntry(): Wrong input: ClusterEntry.EntryID == -1.");
        return false;
    }

    LocalDebug::SetRecursionDepthIfNeeded(Depth);

    if (Depth > MaxRecursionDepth)
    {
        UE_LOGFMT(LogUMBCG_AttackClusteringSubsystem, Warning, "IntegrateClusterEntry(): Recursion limit reached. Stopping cluster entry processing.");
        return false;
    }

    // Find or create the most suitable cluster for the new cluster entry
    int32 BestClusterIndex = FindBestCluster(ClusterEntry);
    if (BestClusterIndex == -1)
    {
        // Create a new cluster
        int32 NewClusterID = CreateNewCluster(ClusterEntry);
        ClusterEntries[ClusterEntry.EntryID].ClusterID = NewClusterID;
        AddToChangedClustersPayloadIfNeeded(NewClusterID);
        return true;
    }

    // Assign the cluster entry to the best cluster
    FAttackCluster& BestCluster = Clusters[BestClusterIndex];
    BestCluster.EntryIDs.Add(ClusterEntry.EntryID);
    ClusterEntries[ClusterEntry.EntryID].ClusterID = BestClusterIndex;
    BestCluster.UpdateCentroidProperties(ClusterEntries);
    // update ChangedClustersIDsPayload
    AddToChangedClustersPayloadIfNeeded(Clusters[BestClusterIndex].ClusterID);

    // Adjust clusters until no further changes occur
    bool ChangesOccurred = true;
    while (ChangesOccurred)
    {
        ChangesOccurred = false;
        // a copy to avoid "Array has changed during ranged-for iteration" error because of the recursive nature of function calls
        TArray<FAttackCluster> ClustersCopy = Clusters;
        for (FAttackCluster& CurrentClusterCopy : ClustersCopy)
        {
            ChangesOccurred |= HandleExpelledClusterEntries(CurrentClusterCopy, Depth);
        }
    }

    return true;
}
