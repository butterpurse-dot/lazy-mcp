#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BMHUD.generated.h"

class ABMCharacter;

/** Canvas HUD (code-only, so it needs no UMG assets): vitals, ammo, money, clock, score, radar, kill feed, buy menu, scoreboard. */
UCLASS()
class BLOODMOON_API ABMHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;

	bool bBuyOpen = false, bBoard = false;

	void PushKill(const FString& Killer, uint8 KillerTeam, const FString& Victim, uint8 VictimTeam, const FString& Weapon, bool bHead);
	void ShowBanner(const FString& Title, const FString& Sub, uint8 Team);
	void Notice(const FString& Text);
	void HitMarker(bool bHead);

private:
	struct FFeed { FString K, V, W; uint8 KT, VT; bool bHead; float Until; };
	TArray<FFeed> Feed;
	FString BannerTitle, BannerSub, NoticeText;
	uint8 BannerTeam = 2;
	float BannerUntil = 0.f, NoticeUntil = 0.f, HitUntil = 0.f; bool bHitHead = false;
	TMap<TWeakObjectPtr<ABMCharacter>, float> Spotted;
	float SpotT = 0.f;

	void Text(const FString& S, float X, float Y, FLinearColor C, float Scale = 1.f, bool bCenter = false);
	void DrawRadar(const ABMCharacter* View, float X, float Y, float Size);
	void DrawBuy(const ABMCharacter* View);
	void DrawBoard();
	FLinearColor TeamColor(uint8 Team) const;
};
