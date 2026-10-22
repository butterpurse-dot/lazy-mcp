#include "BMCharacter.h"
#include "BMArena.h"
#include "BMGameMode.h"
#include "BMGameState.h"
#include "BMHUD.h"
#include "BMPlayerState.h"
#include "BMBotController.h"
#include "BMTracer.h"
#include "BMWeapons.h"
#include "BMMaterials.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<float> CVarSens(TEXT("bm.Sens"), 0.07f, TEXT("Mouse sensitivity in degrees per count"));
static constexpr ECollisionChannel WeaponChannel = ECC_GameTraceChannel1;

ABMCharacter::ABMCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bUseControllerRotationYaw = true;
	BaseEyeHeight = 70.f;
	GetCapsuleComponent()->InitCapsuleSize(42.f, 92.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(WeaponChannel, ECR_Block);
	GetMesh()->SetVisibility(false); GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UCharacterMovementComponent* M = GetCharacterMovement();
	M->GravityScale = 2.2f; M->JumpZVelocity = 700.f; M->MaxAcceleration = 2400.f; M->BrakingDecelerationWalking = 2400.f; M->AirControl = 0.25f; M->MaxWalkSpeed = 550.f;

	Cam = CreateDefaultSubobject<UCameraComponent>(TEXT("Cam"));
	Cam->SetupAttachment(GetCapsuleComponent()); Cam->SetRelativeLocation(FVector(0, 0, 70)); Cam->bUsePawnControlRotation = true; Cam->SetFieldOfView(90.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sph(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto Make = [&](const TCHAR* N, UStaticMesh* Mesh, USceneComponent* Parent)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(N);
		C->SetupAttachment(Parent); C->SetStaticMesh(Mesh); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); return C;
	};
	Body = Make(TEXT("Body"), Cyl.Object, GetCapsuleComponent()); Body->SetOwnerNoSee(true);
	Head = Make(TEXT("Head"), Sph.Object, GetCapsuleComponent()); Head->SetOwnerNoSee(true);
	Gun = Make(TEXT("Gun"), Cube.Object, Cam); Gun->SetOnlyOwnerSee(true); Gun->SetCastShadow(false);
	Gun->SetRelativeLocation(FVector(40, 18, -18)); Gun->SetRelativeScale3D(FVector(0.5f, 0.06f, 0.09f));
	Loadout[1] = BMWeapons::SidearmId(); Loadout[2] = BMWeapons::DaggerId();
}

void ABMCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
	Super::GetLifetimeReplicatedProps(Out);
	DOREPLIFETIME(ABMCharacter, Team); DOREPLIFETIME(ABMCharacter, Health); DOREPLIFETIME(ABMCharacter, MaxHealth);
	DOREPLIFETIME(ABMCharacter, Armor); DOREPLIFETIME(ABMCharacter, bAlive); DOREPLIFETIME(ABMCharacter, Slot);
	DOREPLIFETIME(ABMCharacter, bReloading); DOREPLIFETIME(ABMCharacter, ActionType); DOREPLIFETIME(ABMCharacter, ActionProgress);
	DOREPLIFETIME(ABMCharacter, RevealUntil); DOREPLIFETIME(ABMCharacter, bHaste);
	DOREPLIFETIME(ABMCharacter, Loadout);
	DOREPLIFETIME(ABMCharacter, Ammo);             // spectators' HUD shows the viewed player's ammo
	DOREPLIFETIME(ABMCharacter, AbilityReadyTime);
}

bool ABMCharacter::IsHumanControlled() const { return Cast<APlayerController>(GetController()) != nullptr; }

ABMGameMode* ABMCharacter::GM() const { return GetWorld() ? GetWorld()->GetAuthGameMode<ABMGameMode>() : nullptr; }

const FBMWeaponDef* ABMCharacter::CurrentDef() const { return BMWeapons::Find(Loadout[FMath::Min<int32>(Slot, 2)]); }

bool ABMCharacter::IsRevealed() const
{
	const AGameStateBase* GS = GetWorld()->GetGameState();
	return GS && RevealUntil > GS->GetServerWorldTimeSeconds();
}

// ---------------------------------------------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------------------------------------------
void ABMCharacter::InitLoadout(ABMPlayerState* PS)
{
	Team = PS->Team;
	MaxHealth = Team == EBMTeam::Werewolf ? 125.f : 100.f;
	Health = MaxHealth; Armor = PS->Armor; bAlive = true; bHaste = false;
	Loadout[0] = PS->Primary; Loadout[1] = PS->Secondary; Loadout[2] = BMWeapons::DaggerId();
	for (int32 I = 0; I < 3; ++I)
		if (const FBMWeaponDef* W = BMWeapons::Find(Loadout[I])) { Ammo[I].Mag = W->Mag; Ammo[I].Reserve = W->Reserve; }
	Slot = Loadout[0] != NAME_None ? 0 : 1;
	FireCd = 0.4f; bReloading = false; RevealUntil = 0.f; AbilityReadyTime = 0.f; ActionType = 0; ActionProgress = 0.f;
	OnRep_Team(); OnRep_Alive();
}

void ABMCharacter::OnRep_Team() { ApplyVisuals(); UpdateSpeed(); }

void ABMCharacter::ApplyVisuals()
{
	if (Team == EBMTeam::None || Team == VisualTeam) return; // idempotent: OnRep_Team also fires for haste changes
	VisualTeam = Team;
	const bool bWolf = Team == EBMTeam::Werewolf;
	const FLinearColor Col = bWolf ? FLinearColor(0.20f, 0.11f, 0.05f) : FLinearColor(0.35f, 0.04f, 0.08f);
	const float Wd = bWolf ? 1.15f : 0.85f;
	Body->SetRelativeScale3D(FVector(Wd, Wd, 1.4f)); Body->SetRelativeLocation(FVector(0, 0, -22.f));
	Head->SetRelativeScale3D(FVector(bWolf ? 0.42f : 0.3f)); Head->SetRelativeLocation(FVector(0, 0, 76.f));
	BMMaterials::Apply(Body, BMMaterials::Lit(), Col);
	BMMaterials::Apply(Head, BMMaterials::Lit(), bWolf ? Col : FLinearColor(0.85f, 0.8f, 0.8f));
	BMMaterials::Apply(Gun, BMMaterials::Lit(), FLinearColor(0.02f, 0.02f, 0.03f));
}

void ABMCharacter::OnRep_Alive()
{
	GetCapsuleComponent()->SetCollisionEnabled(bAlive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Body->SetRelativeRotation(bAlive ? FRotator::ZeroRotator : FRotator(90.f, 0.f, 0.f));
	if (!bAlive) { Body->SetRelativeLocation(FVector(40, 0, -80)); Head->SetRelativeLocation(FVector(110, 0, -75)); Gun->SetVisibility(false); }
}

void ABMCharacter::UpdateSpeed()
{
	const float Base = Team == EBMTeam::Werewolf ? 530.f : 570.f;
	const FBMWeaponDef* W = CurrentDef();
	GetCharacterMovement()->MaxWalkSpeed = Base * (W ? W->MoveMul : 1.f) * (bHaste ? 1.25f : 1.f);
}

// ---------------------------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------------------------
void ABMCharacter::Tick(float Dt)
{
	Super::Tick(Dt);
	UpdateSpeed();
	if (IsLocallyControlled() && Cast<APlayerController>(GetController())) LocalInput(Dt);
	if (HasAuthority()) ServerTick(Dt);
}

void ABMCharacter::LocalInput(float Dt)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	ABMHUD* HUD = PC ? Cast<ABMHUD>(PC->GetHUD()) : nullptr;
	const ABMGameState* GS = GetWorld()->GetGameState<ABMGameState>();
	if (!PC || !bAlive) return;

	float Dx = 0.f, Dy = 0.f; PC->GetInputMouseDelta(Dx, Dy);
	FRotator R = PC->GetControlRotation();
	const float Sens = CVarSens.GetValueOnGameThread();
	R.Yaw += Dx * Sens; R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch) + Dy * Sens, -88.f, 88.f);
	PC->SetControlRotation(R);

	const bool bFrozen = GS && (GS->Phase == EBMPhase::Freeze || GS->Phase == EBMPhase::MatchOver || GS->Phase == EBMPhase::Warmup);
	if (!bFrozen && ActionType == 0)
	{
		const FRotationMatrix Yaw(FRotator(0.f, R.Yaw, 0.f));
		const float Walk = PC->IsInputKeyDown(EKeys::LeftShift) ? 0.5f : 1.f;
		AddMovementInput(Yaw.GetUnitAxis(EAxis::X), ((PC->IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) ? 1.f : 0.f)) * Walk);
		AddMovementInput(Yaw.GetUnitAxis(EAxis::Y), ((PC->IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::A) ? 1.f : 0.f)) * Walk);
		if (PC->WasInputKeyJustPressed(EKeys::SpaceBar)) Jump();
	}

	const bool bFire = PC->IsInputKeyDown(EKeys::LeftMouseButton) && !(HUD && HUD->bBuyOpen);
	if (bFire != bSentFire) { bSentFire = bFire; ServerSetFiring(bFire); }
	const bool bUse = PC->IsInputKeyDown(EKeys::E);
	if (bUse != bSentUse) { bSentUse = bUse; ServerSetUse(bUse); }
	if (PC->WasInputKeyJustPressed(EKeys::R)) ServerReload();
	if (PC->WasInputKeyJustPressed(EKeys::F)) ServerAbility();
	if (HUD)
	{
		if (PC->WasInputKeyJustPressed(EKeys::B)) HUD->bBuyOpen = !HUD->bBuyOpen;
		HUD->bBoard = PC->IsInputKeyDown(EKeys::Tab);
	}
	static const FKey Digits[9] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
	for (int32 I = 0; I < 9; ++I)
	{
		if (!PC->WasInputKeyJustPressed(Digits[I])) continue;
		if (HUD && HUD->bBuyOpen) { if (I < BMWeapons::Shop(Team).Num()) ServerBuy(static_cast<uint8>(I)); }
		else if (I < 3) ServerSelectSlot(I == 0 ? 0 : I == 1 ? 1 : 2);
	}
}

void ABMCharacter::ServerTick(float Dt)
{
	if (!bAlive) return;
	const ABMGameState* GS = GetWorld()->GetGameState<ABMGameState>();
	ABMGameMode* Mode = GM();
	if (!GS || !Mode) return;
	const bool bCanAct = GS->Phase == EBMPhase::Live || GS->Phase == EBMPhase::RoundEnd;
	FireCd = FMath::Max(0.f, FireCd - Dt); Recoil = FMath::Max(0.f, Recoil - 10.f * Dt);
	if (bHaste && (HasteLeft -= Dt) <= 0.f) { bHaste = false; }
	if (bReloading && (ReloadLeft -= Dt) <= 0.f)
	{
		if (const FBMWeaponDef* W = CurrentDef())
		{
			FBMAmmo& A = Ammo[Slot]; const int32 N = FMath::Min(W->Mag - A.Mag, A.Reserve);
			A.Mag += N; A.Reserve -= N;
		}
		bReloading = false;
	}
	if (bCanAct && bWantFire && FireCd <= 0.f && !bReloading && ActionType == 0) Fire();

	// Plant / cleanse
	uint8 Next = 0;
	if (bWantUse && GS->Phase == EBMPhase::Live)
	{
		if (Team == EBMTeam::Vampire && !GS->bSigilPlanted && Mode->Arena && Mode->Arena->SiteAt(GetActorLocation()) && GetCharacterMovement()->IsMovingOnGround()) Next = 1;
		else if (Team == EBMTeam::Werewolf && GS->bSigilPlanted && FVector::Dist2D(GetActorLocation(), GS->SigilLocation) < 260.f) Next = 2;
	}
	if (Next != ActionType) { ActionType = Next; ActionTime = 0.f; ActionProgress = 0.f; }
	if (Next)
	{
		const float Need = Next == 1 ? BMRules::PlantTime : BMRules::CleanseTime;
		ActionTime += Dt; ActionProgress = FMath::Clamp(ActionTime / Need, 0.f, 1.f);
		GetCharacterMovement()->StopMovementImmediately();
		if (ActionTime >= Need) { if (Next == 1) Mode->OnPlanted(this); else Mode->OnCleansed(this); ActionType = 0; ActionTime = 0.f; ActionProgress = 0.f; }
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Weapons
// ---------------------------------------------------------------------------------------------------------------
void ABMCharacter::SelectSlot(uint8 NewSlot)
{
	if (NewSlot > 2 || Loadout[NewSlot] == NAME_None || NewSlot == Slot) return;
	Slot = NewSlot; bReloading = false; FireCd = 0.35f;
}

void ABMCharacter::StartReload()
{
	const FBMWeaponDef* W = CurrentDef();
	if (!W || W->bMelee || bReloading || !bAlive) return;
	const FBMAmmo& A = Ammo[Slot];
	if (A.Mag >= W->Mag || A.Reserve <= 0) return;
	bReloading = true; ReloadLeft = W->Reload;
}

void ABMCharacter::Fire()
{
	const FBMWeaponDef* W = CurrentDef();
	if (!W) return;
	if (W->bMelee) { Melee(*W); return; }
	FBMAmmo& A = Ammo[Slot];
	if (A.Mag <= 0) { FireCd = 0.25f; StartReload(); return; }
	--A.Mag; FireCd = 1.f / W->Rate;

	const float SpeedFrac = FMath::Clamp(GetVelocity().Size2D() / FMath::Max(1.f, GetCharacterMovement()->MaxWalkSpeed), 0.f, 1.f);
	const float Spread = W->Spread + W->MoveSpread * SpeedFrac + (GetCharacterMovement()->IsFalling() ? 4.f : 0.f) + Recoil + BotExtraSpread * (0.55f + 0.45f * SpeedFrac);
	const FVector Eye = EyeLocation(); const FVector Aim = GetControlRotation().Vector();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BMShot), true, this);
	for (int32 P = 0; P < W->Pellets; ++P)
	{
		const FVector Dir = FMath::VRandCone(Aim, FMath::DegreesToRadians(Spread));
		FHitResult Hit; const FVector End = Eye + Dir * W->Range;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Eye, End, WeaponChannel, Q);
		const FVector Impact = bHit ? Hit.ImpactPoint : End;
		if (ABMCharacter* V = bHit ? Cast<ABMCharacter>(Hit.GetActor()) : nullptr)
		{
			if (V->bAlive && V->Team != Team)
			{
				const bool bHead = Hit.ImpactPoint.Z > V->GetActorLocation().Z + 62.f;
				float Dmg = W->Damage * FMath::Pow(W->Falloff, Hit.Distance / 1000.f);
				if (bHead) Dmg *= W->HeadMul;
				V->ApplyHit(Dmg, this, bHead, *W);
			}
		}
		MulticastShotFx(Eye + FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::Y) * 20.f - FVector(0, 0, 15.f), Impact, static_cast<uint8>(Team));
	}
	Recoil = FMath::Min(6.f, Recoil + W->Kick * 0.5f);
	if (IsHumanControlled()) ClientKick(W->Kick * 0.6f);
}

void ABMCharacter::Melee(const FBMWeaponDef& W)
{
	FireCd = 1.f / W.Rate;
	FHitResult Hit; const FVector Eye = EyeLocation(), End = Eye + GetControlRotation().Vector() * W.Range;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BMStab), true, this);
	if (GetWorld()->SweepSingleByChannel(Hit, Eye, End, FQuat::Identity, WeaponChannel, FCollisionShape::MakeSphere(25.f), Q))
		if (ABMCharacter* V = Cast<ABMCharacter>(Hit.GetActor()))
			if (V->bAlive && V->Team != Team)
			{
				const bool bBack = FVector::DotProduct(V->GetActorForwardVector(), GetActorForwardVector()) > 0.5f;
				V->ApplyHit(W.Damage * (bBack ? 2.5f : 1.f), this, false, W);
			}
}

void ABMCharacter::ApplyHit(float Damage, ABMCharacter* Attacker, bool bHead, const FBMWeaponDef& Def)
{
	if (!bAlive) return;
	float Lost = Damage;
	if (Armor > 0.f) { const float Used = FMath::Min(Armor, Damage * (1.f - Def.Pen) * 0.5f); Armor -= Used; Lost = Damage - Used * 2.f; }
	Health -= Lost;
	if (Attacker && Attacker->IsHumanControlled()) Attacker->ClientHit(bHead);
	if (ABMBotController* Bot = Cast<ABMBotController>(GetController())) Bot->OnHurt(Attacker);
	if (Health <= 0.f) Die(Attacker, &Def, bHead);
}

void ABMCharacter::Die(ABMCharacter* Killer, const FBMWeaponDef* Def, bool bHead)
{
	if (!bAlive) return;
	bAlive = false; Health = 0.f; bWantFire = bWantUse = false; ActionType = 0; bReloading = false;
	GetCharacterMovement()->DisableMovement();
	OnRep_Alive();
	if (ABMGameMode* Mode = GM()) Mode->NotifyKill(this, Killer, Def, bHead);
}

void ABMCharacter::UseAbility()
{
	const AGameStateBase* GS = GetWorld()->GetGameState();
	if (!bAlive || !GS) return;
	const float Now = GS->GetServerWorldTimeSeconds();
	if (AbilityReadyTime > Now) return;
	if (Team == EBMTeam::Vampire)
	{
		AbilityReadyTime = Now + 12.f;
		FVector Dir = GetVelocity().Size2D() > 50.f ? GetVelocity().GetSafeNormal2D() : FRotator(0, GetControlRotation().Yaw, 0).Vector();
		MulticastDash(Dir * 2300.f);
	}
	else
	{
		AbilityReadyTime = Now + 20.f; bHaste = true; HasteLeft = 4.f;
		for (TActorIterator<ABMCharacter> It(GetWorld()); It; ++It)
			if (It->bAlive && It->Team != Team) It->RevealUntil = Now + 3.5f;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// RPCs
// ---------------------------------------------------------------------------------------------------------------
void ABMCharacter::ServerSetFiring_Implementation(bool bFire) { bWantFire = bFire; }
void ABMCharacter::ServerSetUse_Implementation(bool bUse) { bWantUse = bUse; }
void ABMCharacter::ServerReload_Implementation() { StartReload(); }
bool ABMCharacter::ServerSelectSlot_Validate(uint8 NewSlot) { return NewSlot <= 2; }
void ABMCharacter::ServerSelectSlot_Implementation(uint8 NewSlot) { SelectSlot(NewSlot); }
void ABMCharacter::ServerAbility_Implementation() { UseAbility(); }
bool ABMCharacter::ServerBuy_Validate(uint8 ShopIndex) { return ShopIndex < 32; }
void ABMCharacter::ServerBuy_Implementation(uint8 ShopIndex)
{
	if (ABMGameMode* Mode = GM()) ClientBuyResult(Mode->TryBuy(this, ShopIndex));
}

void ABMCharacter::MulticastShotFx_Implementation(FVector Start, FVector End, uint8 ShooterTeam)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (UBMFxSubsystem* Fx = GetWorld()->GetSubsystem<UBMFxSubsystem>())
		Fx->Tracer(Start, End, ShooterTeam == 0 ? FLinearColor(1.f, 0.2f, 0.25f) : FLinearColor(0.5f, 0.75f, 1.f));
}

void ABMCharacter::MulticastDash_Implementation(FVector Impulse) { LaunchCharacter(Impulse, true, false); }

void ABMCharacter::ClientHit_Implementation(bool bHead)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (ABMHUD* H = Cast<ABMHUD>(PC->GetHUD())) H->HitMarker(bHead);
}

void ABMCharacter::ClientKick_Implementation(float PitchDeg)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FRotator R = PC->GetControlRotation(); R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch) + PitchDeg, -88.f, 88.f); PC->SetControlRotation(R);
	}
}

void ABMCharacter::ClientBuyResult_Implementation(const FString& Message)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (ABMHUD* H = Cast<ABMHUD>(PC->GetHUD())) H->Notice(Message.IsEmpty() ? TEXT("Purchased") : Message);
}
