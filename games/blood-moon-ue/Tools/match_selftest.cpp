// Standalone check of the match rules: clang++ -std=c++17 -I../Source/BloodMoon match_selftest.cpp ../Source/BloodMoon/BMMatch.cpp
#include "BMMatch.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); std::exit(1); } } while (0)

static FBMMatch Live() { FBMMatch m; m.StartRound(); CHECK(m.Tick(BMRules::FreezeTime + 0.1f, 5, 5) == EMatchEvent::Live); return m; }

int main()
{
	{ FBMMatch m = Live(); CHECK(m.BuyOpen()); m.Tick(BMRules::RoundTime + 1, 5, 5); CHECK(m.LastWinner == 1 && m.LastReason == EMatchReason::Time); }
	{ FBMMatch m = Live(); m.Tick(0.1f, 2, 0); CHECK(m.LastWinner == 0 && m.LastReason == EMatchReason::Elimination); }
	{ FBMMatch m = Live(); CHECK(m.Plant()); CHECK(!m.Plant()); m.Tick(0.1f, 0, 3); CHECK(m.Phase == EMatchPhase::Live);
	  CHECK(m.Cleanse()); CHECK(m.LastReason == EMatchReason::Cleansed && m.LastWinner == 1); }
	{ FBMMatch m = Live(); m.Plant(); CHECK(m.Tick(BMRules::SigilFuse + 1, 3, 3) == EMatchEvent::Ended); CHECK(m.LastReason == EMatchReason::Detonated && m.LastWinner == 0); }
	{ FBMMatch m; int32_t last = 0;
	  for (int i = 0; i < BMRules::WinRounds; ++i) { m.StartRound(); m.Tick(10, 5, 5); m.Tick(0.1f, 5, 0); last = m.Reward[1]; if (i == 0) CHECK(last == BMRules::LossBase); if (i == 1) CHECK(last == BMRules::LossBase + BMRules::LossStep); }
	  CHECK(last == BMRules::LossBase + BMRules::LossStep * BMRules::LossMaxStreak);
	  CHECK(m.Reward[0] == BMRules::WinReward && m.MatchWinner == 0);
	  CHECK(m.Tick(BMRules::EndDelay + 1, 5, 5) == EMatchEvent::MatchOver); }
	std::puts("BMMatch self-test passed");
	return 0;
}
