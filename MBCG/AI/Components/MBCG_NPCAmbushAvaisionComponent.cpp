// Copyright DevRespawn.com (MBCG). All Rights Reserved.

#include "MBCG/AI/Components/MBCG_NPCAmbushAvaisionComponent.h"
#include "Character/LyraCharacter.h"
#include "Character/LyraHealthComponent.h"
#include "AIController.h"
#include "MBCG/AI/Subsystems/MBCG_NPCAmbushAvaisionSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "Logging/StructuredLog.h"


DEFINE_LOG_CATEGORY_STATIC(LogUMBCG_NPCAmbushAvaisionComponent, All, All);


bool UMBCG_NPCAmbushAvaisionComponent::IsControlledByAI(APawn* Pawn) const
{
    if (!Pawn) return false;
    AController* Controller = Pawn->GetController();
    if (!Controller) return false;
    if (Controller->IsA(AAIController::StaticClass())) return true;

    return false;
}


void UMBCG_NPCAmbushAvaisionComponent::HandleOwnerHealthChanged(ULyraHealthComponent* HealthComponent, float OldValue, float NewValue, AActor* Instigator)
{
    ALyraCharacter* LyraCharacter = Cast<ALyraCharacter>(GetOwner());
    if (!LyraCharacter || !IsControlledByAI(LyraCharacter) || !HealthComponent) return;
    // Deadly damage
    if (NewValue <= 0.f && OldValue > 0.f)
    {
        if (Instigator)
        {
            APawn* AssociatedInstigatorPawn = nullptr;

            // Try casting to Pawn first
            AssociatedInstigatorPawn = Cast<APawn>(Instigator);

            if (!AssociatedInstigatorPawn)
            {
                // Instigator as a APlayerState is a default method used in Lyra
                APlayerState* InstigatorPlayerState = Cast<APlayerState>(Instigator);
                if (InstigatorPlayerState)
                {
                    AssociatedInstigatorPawn = InstigatorPlayerState->GetPawn();
                }
            }

            if (!AssociatedInstigatorPawn)
            {
                AController* InstigatorController = Cast<AController>(Instigator);
                if (InstigatorController)
                {
                    AssociatedInstigatorPawn = InstigatorController->GetPawn();
                }
            }

            // UE_LOGFMT(LogUMBCG_NPCAmbushAvaisionComponent, Display, "Owner ({0}) was killed by insitgator's ({1}) attack, InstigatorPawn = {2} ", LyraCharacter->GetDebugName(LyraCharacter),
            // Instigator->GetDebugName(Instigator), AssociatedInstigatorPawn->GetDebugName(AssociatedInstigatorPawn));

            NPCAmbushAvaisionSubsystem->RegisterNewAttack(             //
                AssociatedInstigatorPawn->GetActorLocation(),          //
                AssociatedInstigatorPawn->GetViewRotation().Vector(),  //
                EAttackRegistrationType::InstigatorAndVictim,          //
                LyraCharacter->GetActorLocation());
        }
    }
}


void UMBCG_NPCAmbushAvaisionComponent::BeginPlay()
{
    Super::BeginPlay();

    // WeakNPCAmbushAvaisionSubsystem = UGameInstance::GetSubsystem<UMBCG_NPCAmbushAvaisionSubsystem>(GetWorld()->GetGameInstance());
    NPCAmbushAvaisionSubsystem = GetWorld()->GetSubsystem<UMBCG_NPCAmbushAvaisionSubsystem>();
    check(NPCAmbushAvaisionSubsystem);

    ALyraCharacter* LyraCharacter = Cast<ALyraCharacter>(GetOwner());
    if (LyraCharacter)
    {
        ULyraHealthComponent* HealthComponent = LyraCharacter->FindComponentByClass<ULyraHealthComponent>();
        if (HealthComponent)
        {
            HealthComponent->OnHealthChanged.AddDynamic(this, &UMBCG_NPCAmbushAvaisionComponent::HandleOwnerHealthChanged);
        }
    }
}


void UMBCG_NPCAmbushAvaisionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ALyraCharacter* LyraCharacter = Cast<ALyraCharacter>(GetOwner());
    if (LyraCharacter)
    {
        ULyraHealthComponent* HealthComponent = LyraCharacter->FindComponentByClass<ULyraHealthComponent>();
        if (HealthComponent)
        {
            HealthComponent->OnHealthChanged.RemoveDynamic(this, &UMBCG_NPCAmbushAvaisionComponent::HandleOwnerHealthChanged);
        }
    }

    Super::EndPlay(EndPlayReason);
}
