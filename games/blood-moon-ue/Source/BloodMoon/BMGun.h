#pragma once
// Engine-free trigger / fire-rate / ammo / reload state machine for the active weapon (server side).
// Tested with plain C++ in Tools/gun_selftest.cpp. Mirrors the browser build's rules, plus a small click buffer.
#include <cstdint>

struct FBMGunSpec
{
	float Rate = 1.f;     // shots (or swings) per second
	bool bAuto = false;   // hold to keep firing; otherwise one shot per trigger press
	bool bMelee = false;  // no ammo, swings while held
	int32_t MagSize = 0;
	float ReloadTime = 0.f;
};

enum class EBMGunEvent : uint8_t { None, Shot, Swing, Dry, ReloadStarted, ReloadFinished };

struct FBMGun
{
	static constexpr float DrawTime = 0.35f;   // after switching weapons
	static constexpr float DryDelay = 0.25f;   // between empty clicks
	static constexpr float ClickBuffer = 0.12f; // a semi-auto press slightly early still fires

	float Cooldown = 0.f, ReloadLeft = 0.f, Buffered = 0.f;
	bool bReloading = false, bPrevTrigger = false;

	/** Weapon became active (spawn, buy, switch): cancels reload and plays the draw delay. */
	void Equip(float Draw = DrawTime) { Cooldown = Draw; bReloading = false; ReloadLeft = 0.f; Buffered = 0.f; }
	/** Requested reload (R). Returns true if a reload started. */
	bool StartReload(const FBMGunSpec& S, int32_t Mag, int32_t Reserve);
	/**
	 * Advance by Dt with the current trigger state. bAllowed = may shoot now (alive, round live, not planting...).
	 * Mag/Reserve are the active weapon's ammo and are updated in place. Returns at most one event per call.
	 */
	EBMGunEvent Step(const FBMGunSpec& S, int32_t& Mag, int32_t& Reserve, float Dt, bool bTrigger, bool bAllowed);
};
