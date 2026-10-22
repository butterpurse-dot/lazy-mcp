#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "BMBotController.generated.h"

class ABMCharacter;
class ABMArena;

/**
 * Bot brain (port of core/bot.js): throttled perception with reaction delay, aim smoothing, strafing,
 * objective logic (plant / guard / cleanse), hearing/being hit, grid A* pathing and stuck recovery.
 */
UCLASS()
class BLOODMOON_API ABMBotController : public AAIController
{
	GENERATED_BODY()
public:
	ABMBotController();
	virtual void Tick(float Dt) override;

	void SetDifficulty(int32 Level); // 0 easy, 1 normal, 2 hard
	void RoundReset(const FVector& NewHold);
	void OnHurt(ABMCharacter* From);

private:
	bool CanSee(const ABMCharacter* Me, const ABMCharacter* E) const;
	void Scan(ABMCharacter* Me);
	void Engage(ABMCharacter* Me, float Dt);
	void Navigate(ABMCharacter* Me, float Dt);
	bool ChooseGoal(ABMCharacter* Me, FVector& Goal, bool& bUse);
	FVector PickGuard(const FVector& Around);

	// difficulty
	float React = 0.34f, TurnRate = 430.f, ExtraSpread = 2.2f, HeadAim = 0.22f, Fov = 115.f;

	TWeakObjectPtr<ABMCharacter> Target;
	float ScanT = 0.f, LostT = 0.f, ReactT = 0.f, StrafeT = 0.f, StrafeDir = 0.f, RepathT = 0.f, LookT = 0.f, LookYaw = 0.f, StuckT = 0.f;
	int32 StuckN = 0, PathIdx = 0;
	bool bAimHead = false, bHasInvestigate = false, bHasHold = false, bHasGuard = false;
	float InvestigateT = 0.f;
	FVector Investigate = FVector::ZeroVector, Hold = FVector::ZeroVector, Guard = FVector::ZeroVector, LastPos = FVector::ZeroVector, GoalKey = FVector(-1e9);
	TArray<FVector> Path;
};
