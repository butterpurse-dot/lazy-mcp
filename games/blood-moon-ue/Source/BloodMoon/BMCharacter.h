#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
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
	void LocalInput(float Dt);
	void ServerTick(float Dt);
	void Fire();
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
	float FireCd = 0.f, ReloadLeft = 0.f, Recoil = 0.f, ActionTime = 0.f, HasteLeft = 0.f;
	// client-only edge detection
	bool bSentFire = false, bSentUse = false;
};
