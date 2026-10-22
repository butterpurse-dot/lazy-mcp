#include "BMBotController.h"
#include "BMArena.h"
#include "BMCharacter.h"
#include "BMGameMode.h"
#include "BMGameState.h"
#include "BMWeapons.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

static constexpr ECollisionChannel WeaponChannel = ECC_GameTraceChannel1;

ABMBotController::ABMBotController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = true;
}

void ABMBotController::SetDifficulty(int32 Level)
{
	static const float Reacts[3] = {0.6f, 0.34f, 0.18f}, Turns[3] = {260.f, 430.f, 630.f}, Spreads[3] = {4.f, 2.2f, 1.1f}, Heads[3] = {0.08f, 0.22f, 0.4f}, Fovs[3] = {100.f, 115.f, 130.f};
	const int32 L = FMath::Clamp(Level, 0, 2);
	React = Reacts[L]; TurnRate = Turns[L]; ExtraSpread = Spreads[L]; HeadAim = Heads[L]; Fov = Fovs[L];
	ScanT = FMath::FRand() * 0.12f;
}

void ABMBotController::RoundReset(const FVector& NewHold)
{
	Hold = NewHold; bHasHold = true; bHasGuard = false; bHasInvestigate = false; Target = nullptr; Path.Reset(); GoalKey = FVector(-1e9);
	StuckN = 0;
}

void ABMBotController::OnHurt(ABMCharacter* From)
{
	if (From && !Target.IsValid()) { Investigate = From->GetActorLocation(); InvestigateT = 6.f; bHasInvestigate = true; }
}

bool ABMBotController::CanSee(const ABMCharacter* Me, const ABMCharacter* E) const
{
	const FVector D = E->GetActorLocation() - Me->GetActorLocation();
	const float Dist = D.Size2D();
	if (Dist > 8500.f) return false;
	const FVector Fwd = FRotator(0, GetControlRotation().Yaw, 0).Vector();
	if (Dist > 500.f && FVector::DotProduct(Fwd, D.GetSafeNormal2D()) < FMath::Cos(FMath::DegreesToRadians(Fov * 0.5f))) return false;
	FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(BMSee), true, Me);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Me->EyeLocation(), E->GetActorLocation() + FVector(0, 0, 20), WeaponChannel, Q);
	return !bHit || Hit.GetActor() == E;
}

void ABMBotController::Scan(ABMCharacter* Me)
{
	ABMCharacter* Best = nullptr; float Bd = TNumericLimits<float>::Max();
	for (TActorIterator<ABMCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->bAlive || It->Team == Me->Team) continue;
		const float D = FVector::DistSquared2D(It->GetActorLocation(), Me->GetActorLocation());
		if (D < Bd && CanSee(Me, *It)) { Bd = D; Best = *It; }
	}
	if (Best)
	{
		if (Best != Target.Get()) { ReactT = React * (0.7f + FMath::FRand() * 0.6f); bAimHead = FMath::FRand() < HeadAim; }
		Target = Best; LostT = 0.f;
	}
	else if (Target.IsValid() && (LostT += 0.12f) > 0.5f)
	{
		Investigate = Target->GetActorLocation(); InvestigateT = 6.f; bHasInvestigate = true; Target = nullptr;
	}
}

FVector ABMBotController::PickGuard(const FVector& Around)
{
	ABMGameMode* GMode = GetWorld()->GetAuthGameMode<ABMGameMode>();
	for (int32 I = 0; I < 25; ++I)
	{
		const FVector P = Around + FVector(FMath::FRandRange(-800.f, 800.f), FMath::FRandRange(-800.f, 800.f), 0.f);
		if (GMode->Arena->HitsWall(P, 50.f)) continue;
		FHitResult H; if (!GetWorld()->LineTraceSingleByChannel(H, P + FVector(0, 0, 150), Around + FVector(0, 0, 100), ECC_Visibility)) return P;
	}
	return Around;
}

bool ABMBotController::ChooseGoal(ABMCharacter* Me, FVector& Goal, bool& bUse)
{
	ABMGameMode* GMode = GetWorld()->GetAuthGameMode<ABMGameMode>();
	const ABMGameState* GS = GetWorld()->GetGameState<ABMGameState>();
	if (!GMode || !GS || !GMode->Arena) return false;
	bUse = false;
	const bool bSigil = GS->bSigilPlanted, bVamp = Me->Team == EBMTeam::Vampire;
	const bool bPlanter = GMode->Planter.Get() == this, bCleanser = GMode->Cleanser.Get() == this;
	if (bHasInvestigate && InvestigateT > 0.f && !bSigil)
	{
		const bool bLeash = FVector::Dist2D(Investigate, Hold) < 3300.f;
		if (bVamp ? !bPlanter : bLeash) { Goal = Investigate; return true; }
	}
	if (bSigil)
	{
		if (!bVamp && bCleanser) { Goal = GS->SigilLocation; bUse = FVector::Dist2D(Me->GetActorLocation(), Goal) < 220.f; return true; }
		if (!bHasGuard) { Guard = PickGuard(GS->SigilLocation); bHasGuard = true; }
		Goal = Guard; return true;
	}
	if (bVamp && bPlanter)
	{
		Goal = GMode->Arena->SiteCenter(GMode->PlanSite);
		bUse = GMode->Arena->SiteAt(Me->GetActorLocation()) != 0 && FVector::Dist2D(Me->GetActorLocation(), Goal) < 500.f;
		return true;
	}
	Goal = bHasHold ? Hold : Me->GetActorLocation();
	return true;
}

void ABMBotController::Engage(ABMCharacter* Me, float Dt)
{
	ABMCharacter* T = Target.Get();
	const FBMWeaponDef* W = Me->CurrentDef();
	const FVector Eye = Me->EyeLocation(), Aim = T->GetActorLocation() + FVector(0, 0, bAimHead ? 72.f : 20.f);
	const FRotator Want = (Aim - Eye).Rotation(), Cur = GetControlRotation();
	const float Step = TurnRate * Dt;
	const float Yaw = FMath::FixedTurn(Cur.Yaw, Want.Yaw, Step), Pitch = FMath::FixedTurn(FRotator::NormalizeAxis(Cur.Pitch), FRotator::NormalizeAxis(Want.Pitch), Step);
	SetControlRotation(FRotator(Pitch, Yaw, 0.f)); Me->FaceRotation(FRotator(0, Yaw, 0), Dt);

	const float Dist = FVector::Dist2D(Aim, Eye), Tol = FMath::RadiansToDegrees(FMath::Atan(45.f / FMath::Max(Dist, 100.f))) + 1.5f;
	const bool bAimed = FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, Want.Yaw)) < Tol && FMath::Abs(FMath::FindDeltaAngleDegrees(Pitch, Want.Pitch)) < Tol * 1.5f;
	ReactT -= Dt;
	Me->SetFiring(ReactT <= 0.f && bAimed && (!W || !W->bMelee || Dist < W->Range));

	if ((StrafeT -= Dt) <= 0.f) { StrafeT = 0.5f + FMath::FRand() * 0.9f; static const float Dirs[4] = {-1.f, 0.f, 0.f, 1.f}; StrafeDir = Dirs[FMath::RandRange(0, 3)]; }
	const FVector Right = FRotationMatrix(FRotator(0, Yaw, 0)).GetUnitAxis(EAxis::Y), Fwd = FRotator(0, Yaw, 0).Vector();
	if (W && (W->bMelee || W->Range < 4500.f)) { if (Dist > 200.f) Me->AddMovementInput(Fwd, 1.f); Me->AddMovementInput(Right, StrafeDir * 0.4f); }
	else { Me->AddMovementInput(Right, StrafeDir); if (Dist < 500.f) Me->AddMovementInput(Fwd, -0.6f); }
}

void ABMBotController::Navigate(ABMCharacter* Me, float Dt)
{
	ABMGameMode* GMode = GetWorld()->GetAuthGameMode<ABMGameMode>();
	FVector Goal; bool bUse = false;
	if (!ChooseGoal(Me, Goal, bUse)) return;
	Me->SetUse(bUse);
	const FVector Loc = Me->GetActorLocation();
	const float Gd = FVector::Dist2D(Loc, Goal);
	float YawGoal = GetControlRotation().Yaw; bool bMove = false; FVector Dir = FVector::ZeroVector;
	RepathT -= Dt;
	if (Gd > 120.f && !(bUse && Gd < 150.f))
	{
		if (!Goal.Equals(GoalKey, 100.f) || RepathT <= 0.f || Path.Num() == 0) { GMode->Arena->FindPath(Loc, Goal, Path); PathIdx = 0; GoalKey = Goal; RepathT = 3.f; }
		while (Path.IsValidIndex(PathIdx) && FVector::Dist2D(Loc, Path[PathIdx]) < 70.f) ++PathIdx;
		const FVector Tgt = Path.IsValidIndex(PathIdx) ? Path[PathIdx] : Goal;
		Dir = (Tgt - Loc).GetSafeNormal2D(); YawGoal = Dir.Rotation().Yaw; bMove = true;
	}
	else
	{
		if ((LookT -= Dt) <= 0.f) { LookT = 1.2f + FMath::FRand() * 1.8f; LookYaw = GetControlRotation().Yaw + FMath::FRandRange(-45.f, 45.f); }
		YawGoal = LookYaw;
	}
	const FRotator Cur = GetControlRotation();
	const float Yaw = FMath::FixedTurn(Cur.Yaw, YawGoal, TurnRate * 0.6f * Dt);
	SetControlRotation(FRotator(FMath::FixedTurn(FRotator::NormalizeAxis(Cur.Pitch), 0.f, 60.f * Dt), Yaw, 0.f)); Me->FaceRotation(FRotator(0, Yaw, 0), Dt);
	if (bMove)
	{
		Me->AddMovementInput(Dir, 1.f);
		if ((StuckT += Dt) > 0.6f)
		{
			StuckT = 0.f;
			if (FVector::Dist2D(Loc, LastPos) < 25.f) { ++StuckN; RepathT = 0.f; Me->Jump(); if (StuckN > 4) { StuckN = 0; bHasGuard = false; Path.Reset(); } }
			else StuckN = 0;
			LastPos = Loc;
		}
	}
}

void ABMBotController::Tick(float Dt)
{
	Super::Tick(Dt);
	ABMCharacter* Me = Cast<ABMCharacter>(GetPawn());
	const ABMGameState* GS = GetWorld()->GetGameState<ABMGameState>();
	if (!Me || !Me->bAlive || !GS) return;
	Me->BotExtraSpread = ExtraSpread;
	Me->SetFiring(false);
	if (GS->Phase == EBMPhase::Freeze || GS->Phase == EBMPhase::MatchOver || GS->Phase == EBMPhase::Warmup) return;
	if (bHasInvestigate && (InvestigateT -= Dt) <= 0.f) bHasInvestigate = false;

	if ((ScanT -= Dt) <= 0.f) { ScanT = 0.12f; Scan(Me); }
	if (Target.IsValid() && !Target->bAlive) Target = nullptr;

	// weapon housekeeping
	auto HasAmmo = [Me](int32 S) { return Me->Loadout[S] != NAME_None && Me->Ammo[S].Mag + Me->Ammo[S].Reserve > 0; };
	const uint8 Want = HasAmmo(0) ? 0 : HasAmmo(1) ? 1 : 2; // fall back to the dagger when completely dry
	if (Me->Slot != Want) Me->SelectSlot(Want);
	if (const FBMWeaponDef* W = Me->CurrentDef())
		if (!Target.IsValid() && !W->bMelee && Me->Ammo[Me->Slot].Mag < W->Mag / 2) Me->StartReload();

	if (Target.IsValid() && LostT == 0.f) { Me->SetUse(false); Engage(Me, Dt); }
	else Navigate(Me, Dt);
}
