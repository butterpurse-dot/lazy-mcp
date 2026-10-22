#include "BMPlayerState.h"
#include "Net/UnrealNetwork.h"

void ABMPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
	Super::GetLifetimeReplicatedProps(Out);
	DOREPLIFETIME(ABMPlayerState, Team);
	DOREPLIFETIME(ABMPlayerState, Money);
	DOREPLIFETIME(ABMPlayerState, Kills);
	DOREPLIFETIME(ABMPlayerState, Deaths);
	DOREPLIFETIME(ABMPlayerState, bIsBot);
}

void ABMPlayerState::AddMoney(int32 Amount, int32 Max) { Money = FMath::Clamp(Money + Amount, 0, Max); }
