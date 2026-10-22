#pragma once
#include "CoreMinimal.h"
#include "BMTypes.h"

/** Weapon definition. Ranges in cm. Slot: 0 primary, 1 sidearm, 2 melee. TeamMask: bit0 Vampire, bit1 Werewolf. */
struct FBMWeaponDef
{
	FName Id; const TCHAR* Name; uint8 Slot; uint8 TeamMask; int32 Price;
	float Damage, HeadMul, Pen, Rate; bool bAuto; int32 Mag, Reserve; float Reload;
	float Spread, MoveSpread, Kick, Range, Falloff; int32 Pellets; float MoveMul; bool bMelee; int32 KillReward;
};

namespace BMWeapons
{
	const TArray<FBMWeaponDef>& All();
	const FBMWeaponDef* Find(FName Id);
	bool CanUse(const FBMWeaponDef& W, EBMTeam Team);
	inline FName ArmorId() { return FName(TEXT("armor")); }
	inline FName SidearmId() { return FName(TEXT("nightfang")); }
	inline FName DaggerId() { return FName(TEXT("dagger")); }
	/** Items a team can buy, in menu order (weapons first, armor last). Index into this is what clients send. */
	TArray<FName> Shop(EBMTeam Team);
	constexpr int32 ArmorPrice = 650;
	constexpr float ArmorValue = 100.f;
}
