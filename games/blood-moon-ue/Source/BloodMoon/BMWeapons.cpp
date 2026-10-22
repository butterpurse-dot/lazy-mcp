#include "BMWeapons.h"

const TArray<FBMWeaponDef>& BMWeapons::All()
{
	// Id, Name, Slot, Teams, Price, Dmg, HeadMul, Pen, Rate, Auto, Mag, Reserve, Reload, Spread, MoveSpread, Kick, Range, Falloff/10m, Pellets, MoveMul, Melee, KillReward
	static const TArray<FBMWeaponDef> Table = {
		{ FName(TEXT("dagger")),       TEXT("Silver Dagger"),  2, 3, 0,    40.f,  1.f,  0.7f,  1.8f, false, 1,  0,   0.f,  0.f,  0.f, 0.f,   240.f,   1.f,    1, 1.08f, true,  1500 },
		{ FName(TEXT("nightfang")),    TEXT("Nightfang .45"),  1, 3, 0,    26.f,  3.f,  0.5f,  7.f,  false, 12, 48,  2.0f, 0.5f, 3.f, 0.9f,  12000.f, 0.97f,  1, 1.f,   false, 300 },
		{ FName(TEXT("hexmagnum")),    TEXT("Hex Magnum"),     1, 3, 700,  58.f,  3.4f, 0.8f,  2.2f, false, 7,  28,  2.4f, 0.4f, 3.f, 2.4f,  12000.f, 0.97f,  1, 1.f,   false, 300 },
		{ FName(TEXT("cryptsmg")),     TEXT("Crypt SMG"),      0, 3, 1200, 21.f,  2.6f, 0.5f,  14.f, true,  30, 120, 2.2f, 0.8f, 1.8f, 0.5f, 12000.f, 0.94f,  1, 1.02f, false, 600 },
		{ FName(TEXT("gravegun")),     TEXT("Grave Shotgun"),  0, 3, 1800, 11.f,  2.f,  0.4f,  1.3f, false, 8,  32,  3.0f, 3.2f, 2.f, 4.f,   4000.f,  0.88f,  9, 1.f,   false, 900 },
		{ FName(TEXT("bloodcarbine")), TEXT("Blood Carbine"),  0, 1, 2700, 33.f,  3.6f, 0.77f, 10.f, true,  30, 90,  2.5f, 0.35f, 4.5f, 1.15f, 12000.f, 0.98f, 1, 0.93f, false, 300 },
		{ FName(TEXT("moonrifle")),    TEXT("Moon Rifle"),     0, 2, 3100, 29.f,  3.6f, 0.8f,  11.f, true,  30, 90,  2.3f, 0.3f, 4.f, 0.85f, 12000.f, 0.985f, 1, 0.93f, false, 300 },
		{ FName(TEXT("silverbolt")),   TEXT("Silver Sniper"),  0, 3, 4750, 135.f, 2.f,  0.95f, 0.75f, false, 5,  20,  3.4f, 0.05f, 9.f, 3.f,  12000.f, 1.f,    1, 0.82f, false, 100 },
	};
	return Table;
}

const FBMWeaponDef* BMWeapons::Find(FName Id)
{
	for (const FBMWeaponDef& W : All()) if (W.Id == Id) return &W;
	return nullptr;
}

bool BMWeapons::CanUse(const FBMWeaponDef& W, EBMTeam Team)
{
	return (W.TeamMask & (1 << BMIdx(Team))) != 0;
}

TArray<FName> BMWeapons::Shop(EBMTeam Team)
{
	TArray<FName> Out;
	for (const FBMWeaponDef& W : All()) if (W.Price > 0 && CanUse(W, Team)) Out.Add(W.Id);
	Out.Add(ArmorId());
	return Out;
}
