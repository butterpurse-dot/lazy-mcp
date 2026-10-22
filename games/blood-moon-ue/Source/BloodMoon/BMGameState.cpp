#include "BMGameState.h"
#include "BMHUD.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

void ABMGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
	Super::GetLifetimeReplicatedProps(Out);
	DOREPLIFETIME(ABMGameState, Phase);
	DOREPLIFETIME(ABMGameState, Round);
	DOREPLIFETIME(ABMGameState, ScoreVampire);
	DOREPLIFETIME(ABMGameState, ScoreWerewolf);
	DOREPLIFETIME(ABMGameState, PhaseEndTime);
	DOREPLIFETIME(ABMGameState, bSigilPlanted);
	DOREPLIFETIME(ABMGameState, SigilEndTime);
	DOREPLIFETIME(ABMGameState, LastWinner);
	DOREPLIFETIME(ABMGameState, LastReason);
	DOREPLIFETIME(ABMGameState, SigilLocation);
	DOREPLIFETIME(ABMGameState, BuyEndsAtMs);
}

static ABMHUD* LocalHUD(const UObject* Ctx)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(Ctx, 0);
	return PC ? Cast<ABMHUD>(PC->GetHUD()) : nullptr;
}

void ABMGameState::MulticastKillFeed_Implementation(const FString& Killer, uint8 KillerTeam, const FString& Victim, uint8 VictimTeam, const FString& Weapon, bool bHead)
{
	if (ABMHUD* H = LocalHUD(this)) H->PushKill(Killer, KillerTeam, Victim, VictimTeam, Weapon, bHead);
}

void ABMGameState::MulticastBanner_Implementation(const FString& Title, const FString& Sub, uint8 Team)
{
	if (ABMHUD* H = LocalHUD(this)) H->ShowBanner(Title, Sub, Team);
}
