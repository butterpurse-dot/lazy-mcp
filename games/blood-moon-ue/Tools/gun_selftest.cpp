// clang++ -std=c++17 -I../Source/BloodMoon gun_selftest.cpp ../Source/BloodMoon/BMGun.cpp
#include "BMGun.h"
#include <cstdio>
#include <initializer_list>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); std::exit(1); } } while (0)

static const float Dt = 1.f / 60.f;
static const FBMGunSpec Pistol{7.f, false, false, 12, 2.0f}, Rifle{10.f, true, false, 30, 2.5f}, Knife{1.8f, false, true, 1, 0.f};

// Simulate `Seconds` holding the trigger state; count shots.
static int Run(FBMGun& G, const FBMGunSpec& S, int& Mag, int& Res, float Seconds, bool bTrigger, bool bAllowed = true)
{
	int Shots = 0;
	for (int I = 0; I < int(Seconds / Dt + 0.5f); ++I) Shots += G.Step(S, Mag, Res, Dt, bTrigger, bAllowed) == EBMGunEvent::Shot;
	return Shots;
}

int main()
{
	{ // semi-auto: holding the trigger fires exactly once; releasing and pressing again fires again
		FBMGun G; int M = 12, R = 48;
		CHECK(Run(G, Pistol, M, R, 1.f, true) == 1 && M == 11);
		Run(G, Pistol, M, R, 0.2f, false);
		CHECK(Run(G, Pistol, M, R, 0.1f, true) == 1 && M == 10);
	}
	{ // semi-auto: rapid clicking is capped by fire rate, and a click slightly early is buffered, not lost
		FBMGun G; int M = 12, R = 48, Shots = 0;
		for (int Click = 0; Click < 7; ++Click) { Shots += Run(G, Pistol, M, R, 2 * Dt, true); Shots += Run(G, Pistol, M, R, 0.13f - 2 * Dt, false); }
		CHECK(Shots == 7); // 7 clicks at ~7.7/s against a 7/s gun: every click lands thanks to the buffer
	}
	{ // full-auto: holding fires at the weapon rate
		FBMGun G; int M = 30, R = 90;
		const int Shots = Run(G, Rifle, M, R, 1.f, true);
		CHECK(Shots == 10);
		CHECK(M == 30 - Shots);
	}
	{ // fire rate is exact (not rounded up to whole ticks) on 60, 30 and 144 Hz servers
		for (float Hz : {60.f, 30.f, 144.f})
			for (float Rate : {7.f, 10.f, 11.f, 14.f})
			{
				FBMGun G; int M = 1000, R = 0; const FBMGunSpec S{Rate, true, false, 1000, 1.f}; int Shots = 0;
				for (int I = 0; I < int(10.f * Hz); ++I) Shots += G.Step(S, M, R, 1.f / Hz, true, true) == EBMGunEvent::Shot;
				const float Real = Shots / 10.f;
				if (Real < Rate - 0.2f || Real > Rate + 0.2f) { std::printf("rate %.0f at %.0f Hz -> %.1f/s\n", Rate, Hz, Real); CHECK(false); }
			}
	}
	{ // empty magazine auto-reloads (no input), transfers the right amount, and blocks firing meanwhile
		FBMGun G; int M = 2, R = 5;
		Run(G, Rifle, M, R, 0.3f, true);
		CHECK(M == 0 && G.bReloading);
		CHECK(Run(G, Rifle, M, R, 2.4f, true) == 0); // mid-reload: no shots
		Run(G, Rifle, M, R, 0.2f, false);
		CHECK(!G.bReloading && M == 5 && R == 0);
	}
	{ // completely dry: clicks, never goes negative, never reloads from nothing
		FBMGun G; int M = 0, R = 0;
		EBMGunEvent E = G.Step(Rifle, M, R, Dt, true, true);
		CHECK(E == EBMGunEvent::Dry && M == 0 && R == 0 && !G.bReloading);
	}
	{ // manual reload: refuses when full or without reserve; partial reserve is respected
		FBMGun G; int M = 30, R = 90;
		CHECK(!G.StartReload(Rifle, M, R));
		M = 10; R = 5;
		CHECK(G.StartReload(Rifle, M, R));
		CHECK(!G.StartReload(Rifle, M, R)); // already reloading
		Run(G, Rifle, M, R, 2.6f, false);
		CHECK(M == 15 && R == 0);
	}
	{ // switching weapons cancels a reload (no ammo transferred) and applies the draw delay
		FBMGun G; int M = 10, R = 50;
		G.StartReload(Rifle, M, R); Run(G, Rifle, M, R, 1.f, false);
		G.Equip();
		CHECK(!G.bReloading && M == 10 && R == 50);
		CHECK(Run(G, Rifle, M, R, 0.3f, true) == 0); // still drawing
		CHECK(Run(G, Rifle, M, R, 0.1f, true) >= 1);
	}
	{ // not allowed (freeze phase, planting, dead): never fires and does not queue a shot for later
		FBMGun G; int M = 12, R = 48;
		CHECK(Run(G, Pistol, M, R, 1.f, true, false) == 0 && M == 12);
		CHECK(Run(G, Pistol, M, R, 0.5f, true, true) == 0); // still holding from before: semi needs a new press
	}
	{ // melee swings at its rate while held and never touches ammo
		FBMGun G; int M = 1, R = 0, Swings = 0;
		for (int I = 0; I < 120; ++I) Swings += G.Step(Knife, M, R, Dt, true, true) == EBMGunEvent::Swing;
		CHECK(Swings >= 3 && Swings <= 4 && M == 1 && R == 0);
	}
	{ // large frame hitch (0.5 s) does not produce more than one shot per step
		FBMGun G; int M = 30, R = 0;
		EBMGunEvent E = G.Step(Rifle, M, R, 0.5f, true, true);
		CHECK(E == EBMGunEvent::Shot && M == 29);
	}
	std::puts("BMGun self-test passed");
	return 0;
}
