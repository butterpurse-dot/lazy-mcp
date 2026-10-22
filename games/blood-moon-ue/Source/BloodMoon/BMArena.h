#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BMGrid.h"
#include "BMTypes.h"
#include "BMArena.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 * Unreal wrapper around FBMGrid (engine-free, unit-tested map + navigation): builds collision/visual geometry and
 * lights for the placeholder arena and exposes the grid queries in FVector form for the game mode and bots.
 */
UCLASS()
class BLOODMOON_API ABMArena : public AActor
{
	GENERATED_BODY()
public:
	ABMArena();
	virtual void BeginPlay() override;

	static constexpr float Cell = FBMGrid::Cell;
	const FBMGrid& Grid() const { return Map; }

	bool IsSolid(int32 Cx, int32 Cy) const { return Map.Solid(Cx, Cy); }
	TCHAR SiteAt(const FVector& P) const { return static_cast<TCHAR>(Map.SiteAt(P.X, P.Y)); }
	FVector SiteCenter(TCHAR Site) const { const FBMPoint C = Map.SiteCenter(static_cast<char>(Site)); return FVector(C.X, C.Y, 0.f); }
	FTransform GetSpawn(EBMTeam Team, int32 Index) const;
	FVector RandomHold(TCHAR Region, FRandomStream& Rng) const;
	bool FindPath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const;
	bool HitsWall(const FVector& P, float Radius) const { return Map.HitsWall(P.X, P.Y, Radius); }

private:
	void BuildMeshes();
	void BuildLights();

	FBMGrid Map; // built in the constructor: valid before any query, identical on every machine
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Walls;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Pillars;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Crates;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Floor;
};
