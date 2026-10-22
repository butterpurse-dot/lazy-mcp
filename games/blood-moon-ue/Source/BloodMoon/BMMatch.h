#pragma once
// Round / economy state machine. Deliberately free of Unreal types so it can be unit-tested with a plain
// C++ compiler (see Tools/match_selftest.cpp) and reused on a dedicated server. Mirrors core/match.js.
#include <cstdint>

namespace BMRules
{
	constexpr int32_t TeamSize = 5, WinRounds = 7, StartMoney = 800, MaxMoney = 16000, KillReward = 300, PlantReward = 300,
		WinReward = 3250, LossBase = 1400, LossStep = 500, LossMaxStreak = 4;
	constexpr float FreezeTime = 7.f, BuyWindow = 20.f, RoundTime = 100.f, EndDelay = 6.f, PlantTime = 3.f, CleanseTime = 5.f,
		SigilFuse = 35.f, SigilRadiusCm = 1400.f;
}

enum class EMatchPhase : uint8_t { Idle, Freeze, Live, Ended, MatchOver };
enum class EMatchEvent : uint8_t { None, Live, Ended, Next, MatchOver };
enum class EMatchReason : uint8_t { None, Elimination, Time, Detonated, Cleansed };

struct FBMMatch
{
	// Team index: 0 = Vampire (attackers), 1 = Werewolf (defenders)
	EMatchPhase Phase = EMatchPhase::Idle;
	int32_t Round = 0;
	float Timer = 0.f, Elapsed = 0.f, Fuse = 0.f;
	int32_t Score[2] = {0, 0}, Streak[2] = {0, 0}, Reward[2] = {0, 0};
	bool bSigil = false;
	int32_t LastWinner = -1, MatchWinner = -1;
	EMatchReason LastReason = EMatchReason::None;

	void Reset();
	void StartRound();
	bool BuyOpen() const { return Phase == EMatchPhase::Freeze || (Phase == EMatchPhase::Live && Elapsed < BMRules::BuyWindow); }
	bool Plant();
	bool Cleanse();
	EMatchEvent Tick(float Dt, int32_t AliveVampires, int32_t AliveWerewolves);

private:
	void End(int32_t Winner, EMatchReason Reason);
};
