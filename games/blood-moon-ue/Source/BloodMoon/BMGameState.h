#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BMTypes.h"
#include "BMGameState.generated.h"

/** Replicated match state. Clocks are replicated as absolute server end-times, so clients interpolate for free. */
UCLASS()
class BLOODMOON_API ABMGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

	UPROPERTY(Replicated) EBMPhase Phase = EBMPhase::Warmup;
	UPROPERTY(Replicated) int32 Round = 0;
	UPROPERTY(Replicated) int32 ScoreVampire = 0;
	UPROPERTY(Replicated) int32 ScoreWerewolf = 0;
	UPROPERTY(Replicated) float PhaseEndTime = 0.f;   // server time the current phase ends (not used while a sigil is armed)
	UPROPERTY(Replicated) bool bSigilPlanted = false;
	UPROPERTY(Replicated) float SigilEndTime = 0.f;   // server time of detonation
	UPROPERTY(Replicated) uint8 LastWinner = 2;       // EBMTeam, 2 = none
	UPROPERTY(Replicated) uint8 LastReason = 0;       // EMatchReason
	UPROPERTY(Replicated) FVector SigilLocation = FVector::ZeroVector;

	float PhaseRemaining() const { return FMath::Max(0.f, PhaseEndTime - GetServerWorldTimeSeconds()); }
	float SigilRemaining() const { return bSigilPlanted ? FMath::Max(0.f, SigilEndTime - GetServerWorldTimeSeconds()) : 0.f; }

	UFUNCTION(NetMulticast, Reliable) void MulticastKillFeed(const FString& Killer, uint8 KillerTeam, const FString& Victim, uint8 VictimTeam, const FString& Weapon, bool bHead);
	UFUNCTION(NetMulticast, Reliable) void MulticastBanner(const FString& Title, const FString& Sub, uint8 Team);
};
