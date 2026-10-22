#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BMGun.h"
#include "BMTypes.h"
#include "BMCharacter.generated.h"

struct FBMWeaponDef;
class UCameraComponent;
class UStaticMeshComponent;
class ABMPlayerState;

/**
 * Player and bot pawn. Server-authoritative: the owning client only sends intents (fire/use/reload/slot/ability/buy);
 * the server runs weapon logic, damage and objectives. Bots call the same functions directly on the server.
 */
UCLASS()
class BLOODMOON_API ABMCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	ABMCharacter();
	virtual void Tick(float Dt) override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	virtual void PawnClientRestart() override;
	virtual bool CanJumpInternal_Implementation() const override;
	/** Frozen (buy phase / match over), planting or cleansing, or dead. Enforced on the server and predicted locally. */
	bool IsMovementLocked() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

	// ---- replicated state ----
	UPROPERTY(ReplicatedUsing = OnRep_Team) EBMTeam Team = EBMTeam::None;
	UPROPERTY(Replicated) float Health = 100.f;
	UPROPERTY(Replicated) float MaxHealth = 100.f;
	UPROPERTY(Replicated) float Armor = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_Alive) bool bAlive = true;
	UPROPERTY(Replicated) FName Loadout[3];
	UPROPERTY(Replicated) FBMAmmo Ammo[3];
	UPROPERTY(Replicated) uint8 Slot = 1;
	UPROPERTY(Replicated) bool bReloading = false;
	UPROPERTY(Replicated) uint8 ActionType = 0;       // 0 none, 1 plant, 2 cleanse
	UPROPERTY(Replicated) float ActionProgress = 0.f; // 0..1
	UPROPERTY(Replicated) float AbilityReadyTime = 0.f; // server time
	UPROPERTY(Replicated) float RevealUntil = 0.f;      // server time: shown on the enemy radar
	UPROPERTY(ReplicatedUsing = OnRep_Team) bool bHaste = false;

	// ---- API shared by player input and bots (server side) ----
	void InitLoadout(ABMPlayerState* PS);
	void SetFiring(bool bFire) { bWantFire = bFire; }
	void SetUse(bool bUse) { bWantUse = bUse; }
	void SelectSlot(uint8 NewSlot);
	void StartReload();
	void UseAbility();
	void ApplyHit(float Damage, ABMCharacter* Attacker, bool bHead, const FBMWeaponDef& Def);
	void Die(ABMCharacter* Killer, const FBMWeaponDef* Def, bool bHead);

	const FBMWeaponDef* CurrentDef() const;
	FVector EyeLocation() const { return GetPawnViewLocation(); }
	float BotExtraSpread = 0.f; // degrees, set by bot difficulty
	bool IsRevealed() const;

	// ---- client -> server intents ----
	UFUNCTION(Server, Reliable) void ServerSetFiring(bool bFire);
	UFUNCTION(Server, Reliable) void ServerSetUse(bool bUse);
	UFUNCTION(Server, Reliable) void ServerReload();
	UFUNCTION(Server, Reliable, WithValidation) void ServerSelectSlot(uint8 NewSlot);
	UFUNCTION(Server, Reliable) void ServerAbility();
	/** Index into BMWeapons::Shop(Team). Indices, not names: clients can't inject arbitrary FNames into the server's name table. */
	UFUNCTION(Server, Reliable, WithValidation) void ServerBuy(uint8 ShopIndex);

	// ---- server -> clients ----
	UFUNCTION(NetMulticast, Unreliable) void MulticastShotFx(FVector Start, FVector End, uint8 ShooterTeam);
	UFUNCTION(NetMulticast, Reliable) void MulticastDash(FVector Impulse);
	UFUNCTION(Client, Unreliable) void ClientHit(bool bHead);
	UFUNCTION(Client, Unreliable) void ClientKick(float PitchDeg);
	UFUNCTION(Client, Reliable) void ClientBuyResult(const FString& Message);

protected:
	UFUNCTION() void OnRep_Team();
	UFUNCTION() void OnRep_Alive();

private:
	// ---- local input (owning client only; everything gameplay-relevant goes to the server via RPC) ----
	void OnMove(const struct FInputActionValue& V);
	void OnLookMouse(const struct FInputActionValue& V);
	void OnLookStick(const struct FInputActionValue& V);
	void OnFire(bool bDown);
	void OnFireStart() { OnFire(true); }
	void OnFireStop() { OnFire(false); }
	void OnUseStart() { if (bAlive) ServerSetUse(true); }
	void OnUseStop() { ServerSetUse(false); }
	void OnJump();
	void OnConfirm();
	void OnReload() { if (bAlive) ServerReload(); }
	void OnAbility() { if (bAlive) ServerAbility(); }
	void OnWalkStart() { bWalkHeld = true; }
	void OnWalkStop() { bWalkHeld = false; }
	void OnNextWeapon();
	void OnArmory();
	void OnBoardStart();
	void OnBoardStop();
	void OnMenuNav(const struct FInputActionValue& V);
	void OnNumber(int32 Index);
	bool CanMoveLocally() const;
	float AimFriction() const;
	class ABMHUD* LocalHUD() const;
	void ServerTick(float Dt);
	void FireShot(const FBMWeaponDef& W);
	void Melee(const FBMWeaponDef& W);
	void ApplyVisuals();
	void UpdateSpeed();
	class ABMGameMode* GM() const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Cam;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Gun;
	EBMTeam VisualTeam = EBMTeam::None; // team the materials were built for (avoids re-creating MIDs)
	bool IsHumanControlled() const;

	// server-only
	bool bWantFire = false, bWantUse = false;
	FBMGun GunState; // trigger / rate / ammo / reload state machine of the active weapon (tested: Tools/gun_selftest.cpp)
	float Recoil = 0.f, ActionTime = 0.f, HasteLeft = 0.f;
	// client-only
	bool bWalkHeld = false, bFireHeld = false;
};
