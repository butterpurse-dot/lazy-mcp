#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BMTypes.h"
#include "BMArena.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 * "Crimson Cathedral": built at runtime from a tile grid (so no hand-authored level asset is required).
 * Also owns the grid pathfinding used by bots. Units: 1 tile = 150 cm.
 */
UCLASS()
class BLOODMOON_API ABMArena : public AActor
{
	GENERATED_BODY()
public:
	ABMArena();
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	static constexpr float Cell = 150.f, WallH = 360.f, CrateH = 130.f;

	bool IsSolid(int32 Cx, int32 Cy) const;
	FIntPoint ToCell(const FVector& P) const { return FIntPoint(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell)); }
	FVector CellCenter(int32 Cx, int32 Cy) const { return FVector((Cx + 0.5f) * Cell, (Cy + 0.5f) * Cell, 0.f); }

	/** 'A' / 'B' if the point is inside a bomb site, else 0. */
	TCHAR SiteAt(const FVector& P) const;
	FVector SiteCenter(TCHAR Site) const;
	FTransform GetSpawn(EBMTeam Team, int32 Index) const;
	/** Random free point in the hold region 'A', 'B' or 'M' (mid). */
	FVector RandomHold(TCHAR Region, FRandomStream& Rng) const;
	/** A* over free tiles (8-way, no corner cutting), string-pulled. Returns false if unreachable. */
	bool FindPath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const;
	bool HitsWall(const FVector& P, float Radius) const;

private:
	void BuildGrid();
	void BuildMeshes();
	void BuildLights();
	bool Walkable(const FVector& A, const FVector& B, float R) const;
	FIntPoint NearestFree(FIntPoint C) const;

	TArray<FString> Rows;
	int32 W = 0, H = 0;

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Walls;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Pillars;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Crates;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Floor;
};
