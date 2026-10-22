#include "BMGameMode.h"
#include "BMArena.h"
#include "BMBotController.h"
#include "BMCharacter.h"
#include "BMGameState.h"
#include "BMHUD.h"
#include "BMPlayerState.h"
#include "BMSigil.h"
#include "BMWeapons.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

static const TCHAR* VampNames[] = {TEXT("Vlad"), TEXT("Selene"), TEXT("Lestat"), TEXT("Carmilla"), TEXT("Nosferatu")};
static const TCHAR* WolfNames[] = {TEXT("Fenrir"), TEXT("Remus"), TEXT("Greyback"), TEXT("Lupin"), TEXT("Akela")};

ABMGameMode::ABMGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ABMCharacter::StaticClass();
	PlayerStateClass = ABMPlayerState::StaticClass();
	GameStateClass = ABMGameState::StaticClass();
	HUDClass = ABMHUD::StaticClass();
}

void ABMGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	HumanTeam = UGameplayStatics::ParseOption(Options, TEXT("Team")).Equals(TEXT("werewolf"), ESearchCase::IgnoreCase) ? EBMTeam::Werewolf : EBMTeam::Vampire;
	const FString D = UGameplayStatics::ParseOption(Options, TEXT("Diff"));
	DiffLevel = D.Equals(TEXT("easy"), ESearchCase::IgnoreCase) ? 0 : D.Equals(TEXT("hard"), ESearchCase::IgnoreCase) ? 2 : 1;
	Rng.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
}

void ABMGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (ABMPlayerState* PS = NewPlayer->GetPlayerState<ABMPlayerState>())
	{
		int32 Count[2] = {0, 0};
		for (APlayerState* Other : GameState->PlayerArray)
			if (const ABMPlayerState* O = Cast<ABMPlayerState>(Other); O && O != PS && !O->bIsBot && O->Team != EBMTeam::None) ++Count[BMIdx(O->Team)];
		PS->Team = (Count[0] + Count[1] == 0) ? HumanTeam : (Count[0] <= Count[1] ? EBMTeam::Vampire : EBMTeam::Werewolf);
		PS->Money = BMRules::StartMoney;
	}
}

void ABMGameMode::StartPlay()
{
	Super::StartPlay();
	Arena = GetWorld()->SpawnActor<ABMArena>(FVector::ZeroVector, FRotator::ZeroRotator);
	SpawnBots();
	bBegun = true;
	RestartMatch();
}

void ABMGameMode::SpawnBots()
{
	int32 Humans[2] = {0, 0};
	for (APlayerState* P : GameState->PlayerArray)
		if (const ABMPlayerState* PS = Cast<ABMPlayerState>(P); PS && PS->Team != EBMTeam::None) ++Humans[BMIdx(PS->Team)];
	for (int32 T = 0; T < 2; ++T)
		for (int32 I = Humans[T]; I < BMRules::TeamSize; ++I)
		{
			ABMBotController* C = GetWorld()->SpawnActor<ABMBotController>();
			if (!C) continue;
			C->SetDifficulty(DiffLevel);
			if (ABMPlayerState* PS = C->GetPlayerState<ABMPlayerState>())
			{
				PS->Team = static_cast<EBMTeam>(T); PS->bIsBot = true; PS->Money = BMRules::StartMoney;
				PS->SetPlayerName(T == 0 ? VampNames[I % 5] : WolfNames[I % 5]);
			}
		}
}

void ABMGameMode::RestartMatch()
{
	Match.Reset();
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
		if (ABMPlayerState* PS = (*It)->GetPlayerState<ABMPlayerState>()) { PS->Money = BMRules::StartMoney; PS->Kills = PS->Deaths = 0; PS->bSurvived = false; }
	StartRound();
}

void ABMGameMode::SyncPhase(EBMPhase Phase, float Duration)
{
	if (ABMGameState* GS = GetGameState<ABMGameState>())
	{
		GS->Phase = Phase; GS->PhaseEndTime = GS->GetServerWorldTimeSeconds() + Duration; GS->Round = Match.Round;
		GS->ScoreVampire = Match.Score[0]; GS->ScoreWerewolf = Match.Score[1];
		GS->bSigilPlanted = Match.bSigil;
	}
}

void ABMGameMode::StartRound()
{
	Match.StartRound();
	if (Sigil) { Sigil->Destroy(); Sigil = nullptr; }
	PlanSite = Rng.FRand() < 0.5f ? 'A' : 'B';
	SyncPhase(EBMPhase::Freeze, BMRules::FreezeTime);

	int32 SpawnIdx[2] = {0, 0};
	int32 WolfHold = 0; static const TCHAR WolfRegions[5] = {'A', 'A', 'B', 'B', 'M'};
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* C = It->Get();
		ABMPlayerState* PS = C ? C->GetPlayerState<ABMPlayerState>() : nullptr;
		if (!PS || PS->Team == EBMTeam::None) continue;
		if (!PS->bSurvived) PS->ResetLoadout();
		if (PS->bIsBot) BotBuy(PS);
		if (APawn* Old = C->GetPawn()) Old->Destroy();
		const FTransform T = Arena->GetSpawn(PS->Team, SpawnIdx[BMIdx(PS->Team)]++);
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ABMCharacter* Pawn = GetWorld()->SpawnActor<ABMCharacter>(ABMCharacter::StaticClass(), T, P);
		if (!Pawn) continue;
		C->Possess(Pawn);
		C->SetControlRotation(T.Rotator());
		Pawn->InitLoadout(PS);
		if (ABMBotController* Bot = Cast<ABMBotController>(C))
		{
			const TCHAR Region = PS->Team == EBMTeam::Vampire ? (Rng.FRand() < 0.75f ? PlanSite : 'M') : WolfRegions[WolfHold++ % 5];
			Bot->RoundReset(Arena->RandomHold(Region, Rng));
		}
		PS->bSurvived = false;
	}
	UpdatePlan();
	if (ABMGameState* GS = GetGameState<ABMGameState>())
		GS->MulticastBanner(FString::Printf(TEXT("Round %d"), Match.Round), TEXT("Buy phase - press B for the armory"), 2);
}

void ABMGameMode::UpdatePlan()
{
	ABMGameState* GS = GetGameState<ABMGameState>();
	if (!Arena || !GS) return;
	if (!Planter.IsValid() || !Planter->GetPawn() || !Cast<ABMCharacter>(Planter->GetPawn())->bAlive)
		if (ABMCharacter* C = NearestAlive(EBMTeam::Vampire, Arena->SiteCenter(PlanSite))) Planter = C->GetController();
	if (Match.bSigil)
		if (ABMCharacter* C = NearestAlive(EBMTeam::Werewolf, GS->SigilLocation)) Cleanser = C->GetController();
}

ABMCharacter* ABMGameMode::NearestAlive(EBMTeam Team, const FVector& P) const
{
	ABMCharacter* Best = nullptr; float Bd = TNumericLimits<float>::Max();
	for (TActorIterator<ABMCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->bAlive || It->Team != Team) continue;
		const float D = FVector::DistSquared2D(It->GetActorLocation(), P);
		if (D < Bd) { Bd = D; Best = *It; }
	}
	return Best;
}

void ABMGameMode::CountAlive(int32& V, int32& W) const
{
	V = W = 0;
	for (TActorIterator<ABMCharacter> It(GetWorld()); It; ++It) if (It->bAlive) (It->Team == EBMTeam::Vampire ? V : W)++;
}

void ABMGameMode::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bBegun) return;
	if ((PlanTimer -= Dt) <= 0.f) { PlanTimer = 0.5f; UpdatePlan(); }
	int32 V, W; CountAlive(V, W);
	switch (Match.Tick(Dt, V, W))
	{
	case EMatchEvent::Live: SyncPhase(EBMPhase::Live, BMRules::RoundTime); break;
	case EMatchEvent::Ended: OnRoundEnded(); break;
	case EMatchEvent::Next: StartRound(); break;
	case EMatchEvent::MatchOver:
		SyncPhase(EBMPhase::MatchOver, 10.f);
		GetWorldTimerManager().SetTimer(RestartHandle, this, &ABMGameMode::RestartMatch, 10.f);
		break;
	default: break;
	}
}

void ABMGameMode::OnRoundEnded()
{
	ABMGameState* GS = GetGameState<ABMGameState>();
	if (Match.LastReason == EMatchReason::Detonated && GS)
		for (TActorIterator<ABMCharacter> It(GetWorld()); It; ++It)
			if (It->bAlive && FVector::Dist2D(It->GetActorLocation(), GS->SigilLocation) < BMRules::SigilRadiusCm) It->Die(nullptr, nullptr, false);
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		ABMPlayerState* PS = (*It)->GetPlayerState<ABMPlayerState>();
		if (!PS || PS->Team == EBMTeam::None) continue;
		PS->AddMoney(Match.Reward[BMIdx(PS->Team)], BMRules::MaxMoney);
		const ABMCharacter* P = Cast<ABMCharacter>((*It)->GetPawn());
		PS->bSurvived = P && P->bAlive;
	}
	SyncPhase(EBMPhase::RoundEnd, BMRules::EndDelay);
	if (GS)
	{
		GS->LastWinner = static_cast<uint8>(Match.LastWinner); GS->LastReason = static_cast<uint8>(Match.LastReason);
		static const TCHAR* Why[] = {TEXT(""), TEXT("All enemies eliminated"), TEXT("Time ran out"), TEXT("The Blood Sigil detonated"), TEXT("The Blood Sigil was cleansed")};
		GS->MulticastBanner(Match.LastWinner == 0 ? TEXT("VAMPIRES WIN") : TEXT("WEREWOLVES WIN"), Why[static_cast<int32>(Match.LastReason)], static_cast<uint8>(Match.LastWinner));
	}
}

void ABMGameMode::OnPlanted(ABMCharacter* P)
{
	ABMGameState* GS = GetGameState<ABMGameState>();
	if (!GS || !Match.Plant()) return;
	GS->bSigilPlanted = true; GS->SigilLocation = P->GetActorLocation() * FVector(1, 1, 0);
	GS->SigilEndTime = GS->GetServerWorldTimeSeconds() + BMRules::SigilFuse;
	Sigil = GetWorld()->SpawnActor<ABMSigil>(GS->SigilLocation + FVector(0, 0, 3.f), FRotator::ZeroRotator);
	if (ABMPlayerState* PS = P->GetPlayerState<ABMPlayerState>()) PS->AddMoney(BMRules::PlantReward, BMRules::MaxMoney);
	UpdatePlan();
	GS->MulticastBanner(TEXT("Blood Sigil planted"), TEXT("Werewolves: cleanse it!  Vampires: defend it!"), 0);
}

void ABMGameMode::OnCleansed(ABMCharacter*)
{
	if (Match.Cleanse()) OnRoundEnded();
}

void ABMGameMode::NotifyKill(ABMCharacter* Victim, ABMCharacter* Killer, const FBMWeaponDef* Weapon, bool bHead)
{
	ABMPlayerState* VPS = Victim->GetPlayerState<ABMPlayerState>();
	ABMPlayerState* KPS = Killer ? Killer->GetPlayerState<ABMPlayerState>() : nullptr;
	if (VPS) ++VPS->Deaths;
	if (KPS)
	{
		++KPS->Kills; KPS->AddMoney(Weapon ? Weapon->KillReward : BMRules::KillReward, BMRules::MaxMoney);
		if (Killer->Team == EBMTeam::Vampire) Killer->Health = FMath::Min(Killer->MaxHealth, Killer->Health + 25.f);
	}
	if (ABMGameState* GS = GetGameState<ABMGameState>())
		GS->MulticastKillFeed(KPS ? KPS->GetPlayerName() : TEXT("Blood Sigil"), KPS ? static_cast<uint8>(Killer->Team) : 2,
			VPS ? VPS->GetPlayerName() : TEXT("?"), static_cast<uint8>(Victim->Team), Weapon ? Weapon->Name : TEXT("Detonation"), bHead);
	// Dead players spectate a living teammate through their eyes.
	if (APlayerController* PC = Cast<APlayerController>(Victim->GetController()))
		if (ABMCharacter* Mate = NearestAlive(Victim->Team, Victim->GetActorLocation())) PC->SetViewTargetWithBlend(Mate, 0.6f);
}

FString ABMGameMode::TryBuy(ABMCharacter* B, FName Id)
{
	ABMPlayerState* PS = B ? B->GetPlayerState<ABMPlayerState>() : nullptr;
	if (!PS || !B->bAlive || !Match.BuyOpen()) return TEXT("Buy time is over");
	if (Id == "armor")
	{
		if (B->Armor >= BMWeapons::ArmorValue) return TEXT("Already armored");
		if (PS->Money < BMWeapons::ArmorPrice) return TEXT("Not enough money");
		PS->Money -= BMWeapons::ArmorPrice; PS->Armor = B->Armor = BMWeapons::ArmorValue;
		return FString();
	}
	const FBMWeaponDef* W = BMWeapons::Find(Id);
	if (!W || W->Price <= 0) return TEXT("Unknown item");
	if (!BMWeapons::CanUse(*W, PS->Team)) return TEXT("Not available to your team");
	if (B->Loadout[W->Slot] == Id) return TEXT("Already owned");
	if (PS->Money < W->Price) return TEXT("Not enough money");
	PS->Money -= W->Price;
	(W->Slot == 0 ? PS->Primary : PS->Secondary) = Id;
	B->Loadout[W->Slot] = Id; B->Ammo[W->Slot].Mag = W->Mag; B->Ammo[W->Slot].Reserve = W->Reserve;
	B->SelectSlot(W->Slot);
	return FString();
}

void ABMGameMode::BotBuy(ABMPlayerState* PS)
{
	const bool bVamp = PS->Team == EBMTeam::Vampire;
	const int32 M = PS->Money;
	auto Afford = [&](FName Id) { const FBMWeaponDef* W = BMWeapons::Find(Id); return W && BMWeapons::CanUse(*W, PS->Team) && W->Price + (PS->Armor < 50.f ? BMWeapons::ArmorPrice : 0) <= M; };
	if (PS->Primary == NAME_None)
	{
		const FName Pick[] = {Rng.FRand() < 0.3f ? FName("silverbolt") : NAME_None, bVamp ? FName("bloodcarbine") : FName("moonrifle"), Rng.FRand() < 0.5f ? FName("gravegun") : FName("cryptsmg"), FName("cryptsmg")};
		for (const FName Id : Pick) if (Id != NAME_None && Afford(Id)) { PS->Primary = Id; PS->Money -= BMWeapons::Find(Id)->Price; break; }
	}
	if (PS->Armor < 50.f && PS->Money >= BMWeapons::ArmorPrice) { PS->Money -= BMWeapons::ArmorPrice; PS->Armor = BMWeapons::ArmorValue; }
}
