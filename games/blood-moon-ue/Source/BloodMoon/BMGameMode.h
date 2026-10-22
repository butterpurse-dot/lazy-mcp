#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BMMatch.h"
#include "BMTypes.h"
#include "BMGameMode.generated.h"

class ABMArena;
class ABMCharacter;
class ABMPlayerState;
class ABMSigil;
struct FBMWeaponDef;

/**
 * Server-only rules: spawns the arena, fills teams with bots, runs rounds (via the engine-free FBMMatch),
 * pays economy, validates purchases and resolves kills. Launch options: ?Team=werewolf ?Diff=easy|normal|hard
 */
UCLASS()
class BLOODMOON_API ABMGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ABMGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override {}
	virtual void Tick(float Dt) override;

	UPROPERTY() TObjectPtr<ABMArena> Arena;
	TWeakObjectPtr<AController> Planter, Cleanser;
	TCHAR PlanSite = 'A';
	FRandomStream Rng;

	// ---- called by characters (server) ----
	void NotifyKill(ABMCharacter* Victim, ABMCharacter* Killer, const FBMWeaponDef* Weapon, bool bHead);
	FString TryBuy(ABMCharacter* Buyer, uint8 ShopIndex); // returns an error message, or empty on success
	void OnPlanted(ABMCharacter* Planter_);
	void OnCleansed(ABMCharacter* Cleanser_);
	const FBMMatch& GetMatch() const { return Match; }

private:
	void RestartMatch();
	void StartRound();
	void OnRoundEnded();
	void SpawnBots();
	void BotBuy(ABMPlayerState* PS);
	void UpdatePlan();
	void SyncPhase(EBMPhase Phase, float Duration);
	void CountAlive(int32& V, int32& W) const;
	ABMCharacter* NearestAlive(EBMTeam Team, const FVector& P) const;

	FBMMatch Match;
	bool bBegun = false;
	int32 DiffLevel = 1;
	EBMTeam HumanTeam = EBMTeam::Vampire;
	float PlanTimer = 0.f;
	FTimerHandle RestartHandle;
	UPROPERTY() TObjectPtr<ABMSigil> Sigil;
};
