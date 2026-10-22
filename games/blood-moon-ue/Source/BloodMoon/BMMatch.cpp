#include "BMMatch.h"

void FBMMatch::Reset() { *this = FBMMatch(); }

void FBMMatch::StartRound()
{
	++Round; Phase = EMatchPhase::Freeze; Timer = BMRules::FreezeTime; Elapsed = 0.f; bSigil = false; Fuse = 0.f;
	LastWinner = -1; LastReason = EMatchReason::None;
}

bool FBMMatch::Plant()
{
	if (Phase != EMatchPhase::Live || bSigil) return false;
	bSigil = true; Fuse = BMRules::SigilFuse;
	return true;
}

bool FBMMatch::Cleanse()
{
	if (Phase != EMatchPhase::Live || !bSigil) return false;
	End(1, EMatchReason::Cleansed);
	return true;
}

void FBMMatch::End(int32_t Winner, EMatchReason Reason)
{
	const int32_t Loser = 1 - Winner;
	int32_t Streak_ = Streak[Loser] < BMRules::LossMaxStreak ? Streak[Loser] : BMRules::LossMaxStreak;
	int32_t Loss = BMRules::LossBase + BMRules::LossStep * Streak_;
	if (Loss > BMRules::MaxMoney) Loss = BMRules::MaxMoney;
	++Score[Winner]; Streak[Winner] = 0; ++Streak[Loser];
	Reward[Winner] = BMRules::WinReward; Reward[Loser] = Loss;
	LastWinner = Winner; LastReason = Reason;
	Phase = EMatchPhase::Ended; Timer = BMRules::EndDelay;
	if (Score[Winner] >= BMRules::WinRounds) MatchWinner = Winner;
}

EMatchEvent FBMMatch::Tick(float Dt, int32_t AliveVampires, int32_t AliveWerewolves)
{
	switch (Phase)
	{
	case EMatchPhase::Freeze:
		Timer -= Dt;
		if (Timer <= 0.f) { Phase = EMatchPhase::Live; Timer = BMRules::RoundTime; Elapsed = 0.f; return EMatchEvent::Live; }
		return EMatchEvent::None;
	case EMatchPhase::Live:
		Elapsed += Dt;
		if (bSigil)
		{
			Fuse -= Dt;
			if (Fuse <= 0.f) { Fuse = 0.f; End(0, EMatchReason::Detonated); return EMatchEvent::Ended; }
		}
		else
		{
			Timer -= Dt;
			if (Timer <= 0.f) { Timer = 0.f; End(1, EMatchReason::Time); return EMatchEvent::Ended; }
		}
		if (AliveWerewolves <= 0) { End(0, EMatchReason::Elimination); return EMatchEvent::Ended; }
		if (AliveVampires <= 0 && !bSigil) { End(1, EMatchReason::Elimination); return EMatchEvent::Ended; }
		return EMatchEvent::None;
	case EMatchPhase::Ended:
		Timer -= Dt;
		if (Timer > 0.f) return EMatchEvent::None;
		if (MatchWinner >= 0) { Phase = EMatchPhase::MatchOver; return EMatchEvent::MatchOver; }
		return EMatchEvent::Next;
	default:
		return EMatchEvent::None;
	}
}
