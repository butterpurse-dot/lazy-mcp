#include "BMGun.h"
#include <algorithm>

bool FBMGun::StartReload(const FBMGunSpec& S, int32_t Mag, int32_t Reserve)
{
	if (S.bMelee || bReloading || Mag >= S.MagSize || Reserve <= 0) return false;
	bReloading = true; ReloadLeft = S.ReloadTime; Buffered = 0.f;
	return true;
}

EBMGunEvent FBMGun::Step(const FBMGunSpec& S, int32_t& Mag, int32_t& Reserve, float Dt, bool bTrigger, bool bAllowed)
{
	const bool bPressed = bTrigger && !bPrevTrigger;
	bPrevTrigger = bTrigger;
	// When the cooldown expires mid-tick, the overshoot (< one tick) is carried into the next shot so the real fire
	// rate matches the spec on any tick rate (clamping to 0 would round every interval up to whole ticks). An idle
	// weapon banks nothing: Cooldown rests at 0.
	Cooldown = Cooldown > 0.f ? std::max(-Dt, Cooldown - Dt) : 0.f;
	Buffered = bPressed ? ClickBuffer : std::max(0.f, Buffered - Dt);

	if (bReloading)
	{
		ReloadLeft -= Dt;
		if (ReloadLeft > 0.f) return EBMGunEvent::None;
		const int32_t N = std::min(S.MagSize - Mag, Reserve);
		Mag += N; Reserve -= N; bReloading = false; ReloadLeft = 0.f;
		return EBMGunEvent::ReloadFinished;
	}
	if (!bAllowed) { Buffered = 0.f; return EBMGunEvent::None; }
	if (Cooldown > 1e-6f) return EBMGunEvent::None;

	// Auto-reload an empty magazine as soon as the weapon is ready (no input needed).
	if (!S.bMelee && Mag <= 0 && Reserve > 0) { StartReload(S, Mag, Reserve); return EBMGunEvent::ReloadStarted; }

	const bool bWants = (S.bAuto || S.bMelee) ? bTrigger : Buffered > 0.f;
	if (!bWants) return EBMGunEvent::None;
	Buffered = 0.f;
	const float Carry = std::min(0.f, Cooldown);
	if (S.bMelee) { Cooldown = Carry + 1.f / S.Rate; return EBMGunEvent::Swing; }
	if (Mag <= 0) { Cooldown = DryDelay; return EBMGunEvent::Dry; }
	--Mag; Cooldown = Carry + 1.f / S.Rate;
	return EBMGunEvent::Shot;
}
