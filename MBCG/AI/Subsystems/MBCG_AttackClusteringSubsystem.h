// Copyright DevRespawn.com (MBCG). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MBCG_AttackClusteringSubsystem.generated.h"

/**
 * This susbsystem implements clasterizing locations (both instigators's and victims' in separate cluster groups) to define ambush locations or places of death.
 * Currently only attack source location (InstigatorLocation) and target location (VictimLocation) are taken into account when clasterizing.
 * Other parameters such InstigatorDirection and VictimDirection are not considered
 * This subsystem can be used separately for clastering purposes or in conjunction with MBCG_NPCAmbushAvaisionSubsystem
 */


// Type of a cluster entry
UENUM(BlueprintType)
enum class EEntryType : uint8
{
    Instigator,  // Default
    Victim
};


// This is an entry for FAttackCluster
// It is not supposed to be input by user (e.g. Blueprint user) directly
// One user-input (RegisterNewAttack) may result into one or two FClusterEntry-s depending on EAttackRegistrationType
USTRUCT(BlueprintType)
struct FClusterEntry
{
    GENERATED_BODY()

    // identifier for this entry, serving as its index in the NPCAmbushAvoidanceSubsystem's attack array
    UPROPERTY(BlueprintReadOnly)
    int32 EntryID = -1;

    // Type of entry. Any cluster contains entries of just one type
    UPROPERTY(BlueprintReadOnly)
    EEntryType EntryType = EEntryType::Instigator;

    // Location by which clusters are defined
    UPROPERTY(BlueprintReadOnly)
    FVector EntryLocation = FVector::ZeroVector;

    // Normalized direction of the entry
    UPROPERTY(BlueprintReadOnly)
    FVector EntryDirection = FVector::ZeroVector;

    // ID of the cluster to which this entry belongs (-1 = unclustered)
    UPROPERTY(BlueprintReadOnly)
    int32 ClusterID = -1;
};


// A cluster unites closely located (see MaxClusterRadius) cluster entries which have the same AttackType
// .. @TODO: Perhaps consider grouping clusters not only by EntryType, but also by similar InstigatorDirection or VictimDirection (see CosMaxAmbushSectorDegrees)
USTRUCT(BlueprintType)
struct FAttackCluster
{
    GENERATED_BODY()

    // identifier for the cluster
    UPROPERTY(BlueprintReadOnly)
    int32 ClusterID = -1;

    // Clustering takes place only among clusters of the same EntryType
    UPROPERTY(BlueprintReadOnly)
    EEntryType EntryType = EEntryType::Instigator;

    // List of cluster entries IDs belonging to the cluster, only entries of the same EntryType are in there
    UPROPERTY(BlueprintReadOnly)
    TArray<int32> EntryIDs;

    // Average location of the cluster entries in the cluster
    UPROPERTY(BlueprintReadOnly)
    FVector CentroidLocation = FVector::ZeroVector;

    // Average normalized direction of cluster entries in the cluster
    UPROPERTY(BlueprintReadOnly)
    FVector Direction = FVector::ZeroVector;

    // If cluster is valid: False by default and if e.g. cluster was removed
    UPROPERTY(BlueprintReadOnly)
    bool IsValid = false;

    // Update the cluster's centroid and average direction
    // @param AttackEntries Reference to all cluster entries
    void UpdateCentroidProperties(const TArray<FClusterEntry>& ClusterEntries);
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAttackClustersChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSomeAttackClustersChanged, const TArray<int32>&, ChangedClustersIDsPayload);


UCLASS()
class LYRAGAME_API UMBCG_AttackClusteringSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

public:

    // Get all cluster entries
    UFUNCTION(BlueprintCallable, Category = "NPC Ambush Avaision Subsystem")
    const TArray<FClusterEntry>& GetClusterEntries() const { return ClusterEntries; }

    // Get all clusters
    UFUNCTION(BlueprintCallable, Category = "NPC Ambush Avaision Subsystem")
    const TArray<FAttackCluster>& GetClusters() const { return Clusters; }

    // Get maximum radius of a cluster
    UFUNCTION(BlueprintCallable, Category = "NPC Ambush Avaision Subsystem")
    float GetMaxClusterRadius() const { return MaxClusterRadius; }

    // Set maximum radius of clusters. This functin is supposed to be run before clastering.
    UFUNCTION(BlueprintCallable, Category = "NPC Ambush Avaision Subsystem")
    void SetMaxClusterRadius(float NewMaxClusterRadius) { MaxClusterRadius = NewMaxClusterRadius; }

    // From user-input (UMBCG_NPCAmbushAvaisionSubsystem::RegisterNewAttack) create one or more cluster entries depending on AttackRegistrationType
    void RegisterNewClusterEntry(const FVector& EntryLocation, const FVector& EntryDirection, const EEntryType EntryType = EEntryType::Instigator);

    // Delegate for broadcasting when any of clusters are changed
    UPROPERTY(BLueprintAssignable)
    FOnAttackClustersChanged OnAttackClustersChangedDelegate;

    // Delegate for broadcasting when specific clusters are changed
    UPROPERTY(BLueprintAssignable)
    FOnSomeAttackClustersChanged OnSomeAttackClustersChangedDelegate;

    // return ChangedClustersIDsPayload - the aray with Cluster IDs which were changed as a result of the last call of RegisterNewClusterEntry()
    const TArray<int32>& GetChangedClustersIDsPayload() const { return ChangedClustersIDsPayload; }

private:

    // List of all cluster entries, with the array index corresponding to EntryID (e.g. ClusterEntries[7].EntryID = 7)
    TArray<FClusterEntry> ClusterEntries;
    // List of all clusters, with the array index corresponding to ClusterID (e.g. Clusters[7].ClusterID = 7)
    TArray<FAttackCluster> Clusters;

    // Parameters
    // .. Maximum distance between cluster centroid and the cluster entries' Locations to belong to the same cluster
    float MaxClusterRadius = 175.0f;
    // .. Maximum recursive depth to prevent infinite loops
    const int32 MaxRecursionDepth = 100;
    // .. Precomputed cosine of 30 degrees for directional similarity
    // .. COP: Not used for now
    // float CosMaxMeleeAmbushSectorDegrees = 0.87f;

    // gravity constant to calculate how much a cluster attracts its cluster entries
    float ClusterGravity = 9.8f;

    // Returns reciprocal effect of cluster gravity (the closer to the cluster, the lower value). Returns FLT_MAX if distance is outside cluster's boundaries. Returning 0 is possible
    float CalculateClusterReciprocalGravityEffect(float DistanceToCluster, int32 ClusterID) const;

    // Intelligently (using recursion) integrates a new cluster entry into an appropriate attack cluster, managing cluster composition and relationships.
    //
    // This function attempts to place the given cluster entry into an existing cluster or create a new cluster if needed.
    // The integration process is dynamic and may cause cascading adjustments to existing clusters:
    // - A cluster entry might be added to an existing cluster based on similarity attributes
    // - Existing cluster entries in the chosen or other clusters may be relocated to maintain cluster coherence
    // - Similarity is determined by location (direction is not considered as a clastering parameter)
    // - Cluster are grouped independently by entry type
    //
    // @param ClusterEntry Reference to the cluster entry to be integrated into the cluster system
    // @param Depth Current recursion depth to prevent infinite recursive clustering (default: 0)
    //
    // @return True if successful integration, false if recursion depth exceeds a predefined threshold
    bool IntegrateClusterEntry(const FClusterEntry& ClusterEntry, int32 Depth = 0);

    // Find the best cluster for a cluster entry, or return -1 if no suitable cluster exists
    // @return Clusters's array index which is equal to ClusterID
    int32 FindBestCluster(const FClusterEntry& ClusterEntry) const;

    // Create a new cluster for a cluster entry. Returns ID of the created cluster, or -1 if there was something wrong
    int32 CreateNewCluster(const FClusterEntry& ClusterEntry);

    // Adjusts clusters by expelling cluster entries outside the MaxClusterRadius and forming new clusters
    // @param ClusterCopy Reference to a copy of the cluster to be adjusted. The original cluster will be identified using ClusterCopy.ClusterID and modified in the Clusters array.
    // Passing a reference to the original cluster is prohibited to ensure changes are made directly to the Clusters array because of the potentially recursive nature of function calls.
    //
    // @return True if cluster was changed, False if there were no changes made to the cluster or recursion depth was exceeded
    bool HandleExpelledClusterEntries(const FAttackCluster& ClusterCopy, int32 Depth = 0);

    // Manages cluster assignment for cluster entries to be re-assigned to a different cluster in scenarios with overlapping clusters.
    //
    // A cluster entry may spatially be within the radius of multiple clusters, but must be assigned to only one cluster - the most suitable
    //
    // When a new cluster entry is registered and added to a cluster, the cluster's centroid location will likely shift.
    // This shift can potentially change the optimal cluster for previously registered cluster entires, causing some entries to become closer to a different cluster than their current assignment.
    //
    // Consequently, after registering and assigning a new cluster entry to a cluster, this function checks and potentially reassigns all registered entries to ensure each entry remains in its
    // most appropriate cluster based on updated clusters' centroid locations.
    //
    // Key responsibilities:
    // - Ensure each cluster entry is assigned to its most suitable cluster
    //
    // @param EntryType Specifies clusters of which type to consider for re-assignment
    void HandleEntriesInOverlappingClusters(const EEntryType EntryType, int32 Depth = 0);


    // Array of cluster IDs that were changed as a result of the last call of RegisterNewClusterEntry().
    // The array is passed as a OnSomeAttackClustersChangedDelegate delegate's payload
    // The array's elements are stored in accordance with Clusters and by index (i.e. ChangedAttackClustersIDsPayload[SomeIdx] == Clusters[SomeIdx] == SomeIdx)
    // The array may contain null elements
    TArray<int32> ChangedClustersIDsPayload;

    // Add the input ClusterID into ChangedClustersIDsPayload increasing the size of the array if required
    void AddToChangedClustersPayloadIfNeeded(int32 ClusterID);
    void AddToChangedClustersPayloadIfNeeded(const TArray<int32>& ClusterIDs);

    // Checks
private:

    // returns true if cluster is healthy (i.e. valid, contains cluster entries etc), false otherwise
    bool SoftCheckCluster(int32 ClusterID) const;

    // returns true if cluster entry is healthy (i.e. valid), false otherwise
    bool SoftCheckClusterEntry(int32 EntryID) const;


    // Uniting overlapping clusters
private:

    // Find and unite clusters that are fully overlapping by their entries
    //
    // Iterates through clusters and identifies cases where one cluster's entries are completely contained within another cluster's boundaries. When such clusters are found, the most suitable
    // cluster is chosen to absorb the overlapping cluster.
    //
    // @param EntryType Specifies clusters of which type to consider for processing
    void FindAndUniteFullyOverlappingClusters(const EEntryType EntryType);

    // Determines whether one cluster's entries are completely contained within
    // the boundaries of another cluster by comparing their spatial characteristics.
    //
    // @param SourceClusterID The ID of the cluster being checked for full overlap
    // @param TargetClusterID The ID of the cluster against which overlap is being verified
    //
    // @return bool True if the source cluster is fully within the target cluster, false otherwise
    bool AreClustersFullyOverlapping(int32 SourceClusterID, int32 TargetClusterID) const;

    // Identify the most appropriate cluster to absorb a fully overlapping cluster
    //
    // Evaluates potential master clusters and selects the most suitable candidate
    // based on predefined criteria for cluster absorption.
    //
    // @param SourceClusterID The ID of the cluster to be absorbed
    // @param MasterCandidateClusterIDs Array of potential clusters that could absorb the source cluster
    //
    // @return int32 The ID of the best master cluster candidate, or return -1 if no suitable cluster found
    int32 FindBestMasterClusterCandidate(int32 SourceClusterID, const TArray<int32>& MasterCandidateClusterIDs) const;

    // Merge all cluster entries from one cluster into another
    //
    // Transfers cluster entries from a source cluster to a target cluster, assuming
    // complete spatial overlap between the clusters.
    // The clusters which are to be united must be of the same EntryType and contain at least one cluster entry each.
    //
    // @param MovedSourceClusterID The ID of the cluster whose entries will be transferred
    // @param BestMasterClusterID The ID of the cluster receiving the transferred entries
    //
    // @return bool True if entries were successfully moved, false if an error occurred
    bool UniteClusters(const int32 MovedSourceClusterID, int32 BestMasterClusterID);
};
