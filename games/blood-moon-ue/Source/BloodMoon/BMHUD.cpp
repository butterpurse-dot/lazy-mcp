#include "BMHUD.h"
#include "BMCharacter.h"
#include "BMGameState.h"
#include "BMPlayerState.h"
#include "BMWeapons.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

FLinearColor ABMHUD::TeamColor(uint8 T) const
{
	return T == 0 ? FLinearColor(0.88f, 0.16f, 0.29f) : T == 1 ? FLinearColor(0.30f, 0.61f, 1.f) : FLinearColor(0.8f, 0.8f, 0.8f);
}

void ABMHUD::Text(const FString& S, float X, float Y, FLinearColor C, float Scale, bool bCenter)
{
	UFont* F = GEngine->GetMediumFont();
	if (bCenter) { float W = 0, H = 0; GetTextSize(S, W, H, F, Scale); X -= W * 0.5f; }
	DrawText(S, C, X, Y, F, Scale);
}

void ABMHUD::PushKill(const FString& K, uint8 KT, const FString& V, uint8 VT, const FString& W, bool bHead)
{
	Feed.Add({K, V, W, KT, VT, bHead, GetWorld()->GetTimeSeconds() + 6.f});
	if (Feed.Num() > 6) Feed.RemoveAt(0);
}

void ABMHUD::ShowBanner(const FString& T, const FString& S, uint8 Team) { BannerTitle = T; BannerSub = S; BannerTeam = Team; BannerUntil = GetWorld()->GetTimeSeconds() + 4.5f; }
void ABMHUD::Notice(const FString& T) { NoticeText = T; NoticeUntil = GetWorld()->GetTimeSeconds() + 2.f; }
void ABMHUD::HitMarker(bool bHead) { HitUntil = GetWorld()->GetTimeSeconds() + 0.15f; bHitHead = bHead; }


void ABMHUD::DrawHUD()
{
	Super::DrawHUD();
	const ABMGameState* GS = GetWorld()->GetGameState<ABMGameState>();
	const ABMCharacter* View = Cast<ABMCharacter>(PlayerOwner ? PlayerOwner->GetViewTarget() : nullptr);
	if (!GS || !View || !Canvas) return;
	const float S = Canvas->ClipY / 1080.f, W = Canvas->ClipX, H = Canvas->ClipY, Now = GetWorld()->GetTimeSeconds();
	const bool bSelf = View == GetOwningPawn();
	const FLinearColor White(1, 1, 1), Gold(1.f, 0.81f, 0.35f);

	// scoreboard strip + clock
	const int32 Remaining = FMath::CeilToInt(GS->bSigilPlanted ? GS->SigilRemaining() : GS->PhaseRemaining());
	DrawRect(FLinearColor(0, 0, 0, 0.6f), W * 0.5f - 260 * S, 8 * S, 520 * S, 54 * S);
	Text(FString::Printf(TEXT("%d"), GS->ScoreVampire), W * 0.5f - 240 * S, 14 * S, TeamColor(0), 2.f * S);
	Text(FString::Printf(TEXT("%d"), GS->ScoreWerewolf), W * 0.5f + 210 * S, 14 * S, TeamColor(1), 2.f * S);
	const FString Clock = GS->bSigilPlanted ? FString::Printf(TEXT("SIGIL %d"), Remaining) : FString::Printf(TEXT("%d:%02d"), Remaining / 60, Remaining % 60);
	Text(Clock, W * 0.5f, 10 * S, GS->bSigilPlanted ? TeamColor(0) : White, 1.8f * S, true);
	Text(GS->Phase == EBMPhase::Freeze ? TEXT("BUY PHASE") : FString::Printf(TEXT("Round %d"), GS->Round), W * 0.5f, 42 * S, FLinearColor(0.7f, 0.7f, 0.7f), 0.9f * S, true);

	// vitals / ammo
	Text(FString::Printf(TEXT("HP %d"), FMath::CeilToInt(View->Health)), 24 * S, H - 90 * S, View->Health < 30.f ? FLinearColor(1, 0.25f, 0.25f) : White, 2.2f * S);
	Text(FString::Printf(TEXT("ARMOR %d"), FMath::CeilToInt(View->Armor)), 24 * S, H - 140 * S, FLinearColor(0.7f, 0.8f, 0.95f), 1.2f * S);
	if (const ABMPlayerState* PS = View->GetPlayerState<ABMPlayerState>()) Text(FString::Printf(TEXT("$%d"), PS->Money), 24 * S, H - 180 * S, Gold, 1.6f * S);
	if (const FBMWeaponDef* WD = View->CurrentDef())
	{
		const FBMAmmo& A = View->Ammo[FMath::Min<int32>(View->Slot, 2)];
		Text(WD->Name, W - 300 * S, H - 140 * S, White, 1.1f * S);
		Text(WD->bMelee ? FString(TEXT("-")) : FString::Printf(TEXT("%d / %d%s"), A.Mag, A.Reserve, View->bReloading ? TEXT("  reloading") : TEXT("")), W - 300 * S, H - 100 * S, White, 2.2f * S);
	}
	const bool bVamp = View->Team == EBMTeam::Vampire;
	const float Ready = View->AbilityReadyTime - GS->GetServerWorldTimeSeconds();
	Text(FString::Printf(TEXT("[F] %s %s"), bVamp ? TEXT("Shadow Step") : TEXT("Feral Howl"), Ready > 0.f ? *FString::Printf(TEXT("%ds"), FMath::CeilToInt(Ready)) : TEXT("READY")), W - 300 * S, H - 190 * S, Ready > 0.f ? FLinearColor(0.6f, 0.6f, 0.6f) : Gold, 1.f * S);

	// crosshair / hit marker
	if (bSelf && View->bAlive)
	{
		const float C = 6 * S, L = 10 * S;
		const FLinearColor X(0.5f, 1.f, 0.7f);
		DrawLine(W * 0.5f - C - L, H * 0.5f, W * 0.5f - C, H * 0.5f, X, 2.f); DrawLine(W * 0.5f + C, H * 0.5f, W * 0.5f + C + L, H * 0.5f, X, 2.f);
		DrawLine(W * 0.5f, H * 0.5f - C - L, W * 0.5f, H * 0.5f - C, X, 2.f); DrawLine(W * 0.5f, H * 0.5f + C, W * 0.5f, H * 0.5f + C + L, X, 2.f);
		if (Now < HitUntil)
		{
			const FLinearColor M = bHitHead ? FLinearColor(1, 0.2f, 0.3f) : White; const float D = 12 * S;
			DrawLine(W * 0.5f - D, H * 0.5f - D, W * 0.5f + D, H * 0.5f + D, M, 2.f); DrawLine(W * 0.5f - D, H * 0.5f + D, W * 0.5f + D, H * 0.5f - D, M, 2.f);
		}
	}
	if (!bSelf) Text(FString::Printf(TEXT("Spectating %s"), *(View->GetPlayerState() ? View->GetPlayerState()->GetPlayerName() : FString(TEXT("?")))), W * 0.5f, H - 120 * S, White, 1.2f * S, true);

	// objective progress / prompts
	if (bSelf && View->ActionType)
	{
		DrawRect(FLinearColor(0, 0, 0, 0.6f), W * 0.5f - 130 * S, H * 0.6f, 260 * S, 18 * S);
		DrawRect(TeamColor(0), W * 0.5f - 130 * S, H * 0.6f, 260 * S * View->ActionProgress, 18 * S);
		Text(View->ActionType == 1 ? TEXT("PLANTING") : TEXT("CLEANSING"), W * 0.5f, H * 0.6f - 22 * S, White, 0.9f * S, true);
	}

	// kill feed
	Feed.RemoveAll([Now](const FFeed& F) { return F.Until < Now; });
	float Y = 14 * S;
	for (const FFeed& F : Feed)
	{
		Text(FString::Printf(TEXT("%s  [%s%s]  %s"), *F.K, *F.W, F.bHead ? TEXT(" HS") : TEXT(""), *F.V), W - 460 * S, Y, F.KT == 2 ? White : TeamColor(F.KT), 0.95f * S);
		Y += 24 * S;
	}

	if (Now < BannerUntil) { Text(BannerTitle, W * 0.5f, H * 0.22f, TeamColor(BannerTeam), 2.6f * S, true); Text(BannerSub, W * 0.5f, H * 0.22f + 60 * S, White, 1.1f * S, true); }
	if (Now < NoticeUntil) Text(NoticeText, W * 0.5f, H * 0.7f, FLinearColor(1, 0.7f, 0.5f), 1.2f * S, true);

	DrawRadar(View, 16 * S, 16 * S, 200 * S);
	if (bBuyOpen && bSelf) DrawBuy(View);
	if (bBoard) DrawBoard();
	if (GS->Phase == EBMPhase::MatchOver) Text(GS->ScoreVampire > GS->ScoreWerewolf ? TEXT("VAMPIRES WIN THE MATCH") : TEXT("WEREWOLVES WIN THE MATCH"), W * 0.5f, H * 0.4f, Gold, 3.f * S, true);
}

void ABMHUD::DrawRadar(const ABMCharacter* View, float X, float Y, float Size)
{
	const ABMGameState* GS = GetWorld()->GetGameState<ABMGameState>();
	const float Now = GetWorld()->GetTimeSeconds(), Range = 3800.f, K = (Size * 0.5f) / Range;
	DrawRect(FLinearColor(0, 0, 0, 0.55f), X, Y, Size, Size);
	const FVector C2(X + Size * 0.5f, Y + Size * 0.5f, 0);
	const float Yaw = FMath::DegreesToRadians(View->GetControlRotation().Yaw), Cs = FMath::Cos(Yaw), Sn = FMath::Sin(Yaw);
	auto Dot = [&](const FVector& World, FLinearColor Col, float R)
	{
		const FVector D = World - View->GetActorLocation();
		const float Fwd = D.X * Cs + D.Y * Sn, Right = -D.X * Sn + D.Y * Cs;
		const float Px = C2.X + Right * K, Py = C2.Y - Fwd * K;
		if (Px < X + 3 || Py < Y + 3 || Px > X + Size - 3 || Py > Y + Size - 3) return;
		DrawRect(Col, Px - R, Py - R, R * 2, R * 2);
	};
	// enemies: revealed by Howl or seen by a teammate (checked 4x/sec)
	if ((SpotT -= GetWorld()->GetDeltaSeconds()) <= 0.f)
	{
		SpotT = 0.25f;
		for (auto It = Spotted.CreateIterator(); It; ++It) if (!It.Key().IsValid() || It.Value() < Now) It.RemoveCurrent(); // bounded: pawns are re-created every round
		for (TActorIterator<ABMCharacter> E(GetWorld()); E; ++E)
		{
			if (!E->bAlive || E->Team == View->Team) continue;
			for (TActorIterator<ABMCharacter> M(GetWorld()); M; ++M)
			{
				if (!M->bAlive || M->Team != View->Team) continue;
				FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(BMRadar), true, *M);
				if (!GetWorld()->LineTraceSingleByChannel(Hit, M->EyeLocation(), E->GetActorLocation() + FVector(0, 0, 40), ECC_GameTraceChannel1, Q) || Hit.GetActor() == *E) { Spotted.FindOrAdd(*E) = Now + 0.9f; break; }
			}
		}
	}
	for (TActorIterator<ABMCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->bAlive || *It == View) continue;
		if (It->Team == View->Team) Dot(It->GetActorLocation(), TeamColor(BMIdx(It->Team)), 3.f);
		else if (It->IsRevealed() || (Spotted.Contains(*It) && Spotted[*It] > Now)) Dot(It->GetActorLocation(), TeamColor(BMIdx(It->Team)), 4.f);
	}
	if (GS && GS->bSigilPlanted) Dot(GS->SigilLocation, FLinearColor(1, 0.1f, 0.2f, 0.5f + 0.5f * FMath::Sin(Now * 8.f)), 6.f);
	DrawRect(FLinearColor(1.f, 0.81f, 0.35f), C2.X - 3, C2.Y - 3, 6, 6);
}

void ABMHUD::DrawBuy(const ABMCharacter* View)
{
	const float S = Canvas->ClipY / 1080.f, W = Canvas->ClipX, H = Canvas->ClipY;
	DrawRect(FLinearColor(0, 0, 0, 0.8f), W * 0.5f - 330 * S, H * 0.2f, 660 * S, 560 * S);
	Text(TEXT("ARMORY  (press the number to buy, B to close)"), W * 0.5f, H * 0.2f + 14 * S, FLinearColor(1, 1, 1), 1.2f * S, true);
	int32 N = 1; float Y = H * 0.2f + 70 * S;
	const ABMPlayerState* PS = View->GetPlayerState<ABMPlayerState>();
	for (const FName Id : BMWeapons::Shop(View->Team))
	{
		const FBMWeaponDef* D = BMWeapons::Find(Id);
		const int32 Price = D ? D->Price : BMWeapons::ArmorPrice;
		const bool bCan = PS && PS->Money >= Price;
		Text(FString::Printf(TEXT("%d.  %-16s $%d"), N++, D ? D->Name : TEXT("Silver Plate (armor)"), Price), W * 0.5f - 300 * S, Y, bCan ? FLinearColor(1, 1, 1) : FLinearColor(0.5f, 0.5f, 0.5f), 1.2f * S);
		Y += 44 * S;
	}
}

void ABMHUD::DrawBoard()
{
	const float S = Canvas->ClipY / 1080.f, W = Canvas->ClipX, H = Canvas->ClipY;
	DrawRect(FLinearColor(0, 0, 0, 0.8f), W * 0.5f - 420 * S, H * 0.18f, 840 * S, 600 * S);
	for (int32 T = 0; T < 2; ++T)
	{
		float Y = H * 0.18f + 20 * S + T * 280 * S;
		Text(T == 0 ? TEXT("VAMPIRES   Kills  Deaths  Money") : TEXT("WEREWOLVES   Kills  Deaths  Money"), W * 0.5f - 390 * S, Y, TeamColor(T), 1.1f * S);
		for (APlayerState* P : GetWorld()->GetGameState()->PlayerArray)
		{
			const ABMPlayerState* PS = Cast<ABMPlayerState>(P);
			if (!PS || BMIdx(PS->Team) != T) continue;
			Y += 34 * S;
			Text(FString::Printf(TEXT("%-14s %4d   %4d   $%d"), *PS->GetPlayerName(), PS->Kills, PS->Deaths, PS->Money), W * 0.5f - 390 * S, Y, FLinearColor(1, 1, 1), 1.f * S);
		}
	}
}
