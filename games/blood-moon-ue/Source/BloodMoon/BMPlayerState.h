#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BMTypes.h"
#include "BMPlayerState.generated.h"

/** Economy, score and the loadout that survives between rounds. Bots have one too, so the scoreboard is uniform. */
UCLASS()
class BLOODMOON_API ABMPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

	UPROPERTY(Replicated) EBMTeam Team = EBMTeam::None;
	UPROPERTY(Replicated) int32 Money = 0;
	UPROPERTY(Replicated) int32 Kills = 0;
	UPROPERTY(Replicated) int32 Deaths = 0;
	UPROPERTY(Replicated) bool bIsBot = false;

	// Server-only: what the pawn is re-equipped with at round start.
	FName Primary = NAME_None, Secondary = FName(TEXT("nightfang"));
	float Armor = 0.f;
	bool bSurvived = false;

	void ResetLoadout() { Primary = NAME_None; Secondary = FName(TEXT("nightfang")); Armor = 0.f; }
	void AddMoney(int32 Amount, int32 Max);
};
