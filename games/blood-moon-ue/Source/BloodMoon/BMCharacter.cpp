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
#include "BMInput.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<float> CVarSens(TEXT("bm.Sens"), 0.07f, TEXT("Mouse sensitivity in degrees per count"));
static TAutoConsoleVariable<int32> CVarInvertMouse(TEXT("bm.InvertMouseY"), 0, TEXT("1 = invert mouse pitch"));
static TAutoConsoleVariable<int32> CVarInvertStick(TEXT("bm.InvertStickY"), 0, TEXT("1 = invert gamepad pitch"));
static constexpr ECollisionChannel WeaponChannel = ECC_GameTraceChannel1;

static FBMGunSpec SpecOf(const FBMWeaponDef& W) { return FBMGunSpec{W.Rate, W.bAuto, W.bMelee, W.Mag, W.Reload}; }

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
	GunState.Equip(0.4f); bReloading = false; RevealUntil = 0.f; AbilityReadyTime = 0.f; ActionType = 0; ActionProgress = 0.f;
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

bool ABMCharacter::IsMovementLocked() const
{
	const ABMGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABMGameState>() : nullptr;
	const bool bFrozen = !GS || GS->Phase == EBMPhase::Freeze || GS->Phase == EBMPhase::MatchOver || GS->Phase == EBMPhase::Warmup;
	return !bAlive || bFrozen || ActionType != 0;
}

bool ABMCharacter::CanJumpInternal_Implementation() const
{
	return Super::CanJumpInternal_Implementation() && !IsMovementLocked();
}

void ABMCharacter::UpdateSpeed()
{
	// Runs on server and clients from replicated state, so the lock is authoritative and predicted identically.
	const float Base = Team == EBMTeam::Werewolf ? 530.f : 570.f;
	const FBMWeaponDef* W = CurrentDef();
	GetCharacterMovement()->MaxWalkSpeed = IsMovementLocked() ? 0.f : Base * (W ? W->MoveMul : 1.f) * (bHaste ? 1.25f : 1.f);
}

// ---------------------------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------------------------
void ABMCharacter::Tick(float Dt)
{
	Super::Tick(Dt);
	UpdateSpeed();
	if (HasAuthority()) ServerTick(Dt);
}

// ---------------------------------------------------------------------------------------------------------------
// Local input (Enhanced Input, see BMInput.h). Runs only on the owning client.
// ---------------------------------------------------------------------------------------------------------------
static TAutoConsoleVariable<float> CVarStickRate(TEXT("bm.StickRate"), 200.f, TEXT("Gamepad look speed in degrees per second at full deflection"));

void ABMCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->RemoveMappingContext(BMInput::Get().Context); // idempotent across respawns
			Sub->AddMappingContext(BMInput::Get().Context, 0);
		}
}

void ABMCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	UEnhancedInputComponent* EI = Cast<UEnhancedInputComponent>(Input);
	if (!ensureMsgf(EI, TEXT("Enhanced Input is not the default input component - check Config/DefaultInput.ini"))) return;
	const FBMInputSet& A = BMInput::Get();
	using T = ETriggerEvent;
	EI->BindAction(A.Move, T::Triggered, this, &ABMCharacter::OnMove);
	EI->BindAction(A.LookMouse, T::Triggered, this, &ABMCharacter::OnLookMouse);
	EI->BindAction(A.LookStick, T::Triggered, this, &ABMCharacter::OnLookStick);
	EI->BindAction(A.Fire, T::Started, this, &ABMCharacter::OnFireStart);
	EI->BindAction(A.Fire, T::Completed, this, &ABMCharacter::OnFireStop);
	EI->BindAction(A.Use, T::Started, this, &ABMCharacter::OnUseStart);
	EI->BindAction(A.Use, T::Completed, this, &ABMCharacter::OnUseStop);
	EI->BindAction(A.Jump, T::Started, this, &ABMCharacter::OnJump);
	EI->BindAction(A.Confirm, T::Started, this, &ABMCharacter::OnConfirm);
	EI->BindAction(A.Reload, T::Started, this, &ABMCharacter::OnReload);
	EI->BindAction(A.Ability, T::Started, this, &ABMCharacter::OnAbility);
	EI->BindAction(A.Walk, T::Started, this, &ABMCharacter::OnWalkStart);
	EI->BindAction(A.Walk, T::Completed, this, &ABMCharacter::OnWalkStop);
	EI->BindAction(A.NextWeapon, T::Started, this, &ABMCharacter::OnNextWeapon);
	EI->BindAction(A.Armory, T::Started, this, &ABMCharacter::OnArmory);
	EI->BindAction(A.Scoreboard, T::Started, this, &ABMCharacter::OnBoardStart);
	EI->BindAction(A.Scoreboard, T::Completed, this, &ABMCharacter::OnBoardStop);
	EI->BindAction(A.MenuNav, T::Started, this, &ABMCharacter::OnMenuNav);
	for (int32 I = 0; I < 9; ++I) EI->BindAction(A.Number[I], T::Started, this, &ABMCharacter::OnNumber, I);
}

ABMHUD* ABMCharacter::LocalHUD() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	return PC ? Cast<ABMHUD>(PC->GetHUD()) : nullptr;
}

bool ABMCharacter::CanMoveLocally() const { return !IsMovementLocked(); }

void ABMCharacter::OnMove(const FInputActionValue& V)
{
	if (!CanMoveLocally()) return;
	const FVector2D In = V.Get<FVector2D>();
	const FRotationMatrix Yaw(FRotator(0.f, GetControlRotation().Yaw, 0.f));
	const float Scale = bWalkHeld ? 0.5f : 1.f;
	AddMovementInput(Yaw.GetUnitAxis(EAxis::X), In.Y * Scale);
	AddMovementInput(Yaw.GetUnitAxis(EAxis::Y), In.X * Scale);
}

void ABMCharacter::OnLookMouse(const FInputActionValue& V)
{
	if (!bAlive) return;
	FVector2D D = V.Get<FVector2D>() * CVarSens.GetValueOnGameThread(); // mouse counts -> degrees, frame-rate independent
	if (CVarInvertMouse.GetValueOnGameThread()) D.Y = -D.Y;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FRotator R = PC->GetControlRotation();
		R.Yaw += D.X; R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch) + D.Y, -88.f, 88.f);
		PC->SetControlRotation(R);
	}
}

float ABMCharacter::AimFriction() const
{
	// Controller-only aim slowdown when the crosshair is over a visible enemy (standard console-shooter assist; no snapping).
	const FVector Eye = EyeLocation(), Aim = GetControlRotation().Vector();
	for (TActorIterator<ABMCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->bAlive || It->Team == Team) continue;
		const FVector To = It->GetActorLocation() + FVector(0, 0, 30) - Eye;
		const float Dist = To.Size();
		if (Dist > 6000.f || FVector::DotProduct(Aim, To / Dist) < FMath::Cos(FMath::DegreesToRadians(4.f))) continue;
		FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(BMFriction), false, this);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + To, ECC_GameTraceChannel1, Q) || Hit.GetActor() == *It) return 0.45f;
	}
	return 1.f;
}

void ABMCharacter::OnLookStick(const FInputActionValue& V)
{
	if (!bAlive) return;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;
	const float Rate = CVarStickRate.GetValueOnGameThread() * GetWorld()->GetDeltaSeconds() * AimFriction();
	FVector2D D = V.Get<FVector2D>() * Rate;
	if (CVarInvertStick.GetValueOnGameThread()) D.Y = -D.Y;
	FRotator R = PC->GetControlRotation();
	R.Yaw += D.X; R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch) + D.Y, -88.f, 88.f);
	PC->SetControlRotation(R);
}

void ABMCharacter::OnFire(bool bDown)
{
	const ABMHUD* HUD = LocalHUD();
	const bool bWant = bDown && bAlive && !(HUD && HUD->bBuyOpen);
	if (bWant == bFireHeld) return; // only send changes
	bFireHeld = bWant; ServerSetFiring(bWant);
}

void ABMCharacter::OnJump()
{
	const ABMHUD* HUD = LocalHUD();
	if (HUD && HUD->bBuyOpen) return; // A/Cross confirms the armory selection instead (OnConfirm)
	if (CanMoveLocally()) Jump();
}

void ABMCharacter::OnConfirm()
{
	ABMHUD* HUD = LocalHUD();
	if (HUD && HUD->bBuyOpen && bAlive) ServerBuy(static_cast<uint8>(HUD->BuyCursor));
}

void ABMCharacter::OnNextWeapon()
{
	if (!bAlive) return;
	for (int32 Step = 1; Step <= 3; ++Step)
	{
		const uint8 S = static_cast<uint8>((Slot + Step) % 3);
		if (Loadout[S] != NAME_None) { ServerSelectSlot(S); return; }
	}
}

void ABMCharacter::OnArmory()
{
	if (ABMHUD* HUD = LocalHUD())
	{
		HUD->bBuyOpen = !HUD->bBuyOpen && bAlive;
		HUD->BuyCursor = 0;
		if (HUD->bBuyOpen && bFireHeld) OnFire(false); // never keep shooting behind the menu
	}
}

void ABMCharacter::OnBoardStart() { if (ABMHUD* HUD = LocalHUD()) HUD->bBoard = true; }
void ABMCharacter::OnBoardStop() { if (ABMHUD* HUD = LocalHUD()) HUD->bBoard = false; }

void ABMCharacter::OnMenuNav(const FInputActionValue& V)
{
	ABMHUD* HUD = LocalHUD();
	if (!HUD || !HUD->bBuyOpen) return;
	const int32 N = BMWeapons::Shop(Team).Num();
	HUD->BuyCursor = (HUD->BuyCursor + (V.Get<float>() > 0.f ? 1 : -1) + N) % N;
}

void ABMCharacter::OnNumber(int32 Index)
{
	if (!bAlive) return;
	if (ABMHUD* HUD = LocalHUD(); HUD && HUD->bBuyOpen)
	{
		if (Index < BMWeapons::Shop(Team).Num()) { HUD->BuyCursor = Index; ServerBuy(static_cast<uint8>(Index)); }
	}
	else if (Index < 3) ServerSelectSlot(static_cast<uint8>(Index));
}

void ABMCharacter::ServerTick(float Dt)
{
	if (!bAlive) return;
	const ABMGameState* GS = GetWorld()->GetGameState<ABMGameState>();
	ABMGameMode* Mode = GM();
	if (!GS || !Mode) return;
	const bool bCanAct = GS->Phase == EBMPhase::Live || GS->Phase == EBMPhase::RoundEnd;
	Recoil = FMath::Max(0.f, Recoil - 10.f * Dt);
	if (bHaste && (HasteLeft -= Dt) <= 0.f) { bHaste = false; }
	if (const FBMWeaponDef* W = CurrentDef())
	{
		FBMAmmo& A = Ammo[Slot];
		int32_t Mag = A.Mag, Reserve = A.Reserve;
		const EBMGunEvent E = GunState.Step(SpecOf(*W), Mag, Reserve, Dt, bWantFire, bCanAct && ActionType == 0);
		A.Mag = Mag; A.Reserve = Reserve; bReloading = GunState.bReloading;
		if (E == EBMGunEvent::Shot) FireShot(*W);
		else if (E == EBMGunEvent::Swing) Melee(*W);
	}

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
	Slot = NewSlot; GunState.Equip(); bReloading = false;
}

void ABMCharacter::StartReload()
{
	const FBMWeaponDef* W = CurrentDef();
	if (!W || !bAlive) return;
	if (GunState.StartReload(SpecOf(*W), Ammo[Slot].Mag, Ammo[Slot].Reserve)) bReloading = true;
}

void ABMCharacter::FireShot(const FBMWeaponDef& Def)
{
	const FBMWeaponDef* W = &Def; // ammo and timing were already handled by GunState

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
	const ABMGameState* GS = GetWorld()->GetGameState<ABMGameState>();
	if (!bAlive || !GS || ActionType != 0) return;
	if (GS->Phase != EBMPhase::Live && GS->Phase != EBMPhase::RoundEnd) return; // no abilities in the buy phase
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
