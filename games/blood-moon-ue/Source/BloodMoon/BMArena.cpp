#include "BMArena.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Algo/Reverse.h"

namespace
{
	constexpr int32 MapW = 48, MapH = 40;
	struct FRect { int32 X1, Y1, X2, Y2; };
	struct FProp { TCHAR Ch; int32 X, Y; bool bMirror; };

	const FRect Carve[] = {
		{18, 2, 29, 7}, {18, 32, 29, 37}, {3, 3, 14, 12}, {33, 3, 44, 12}, {14, 4, 17, 7}, {30, 4, 33, 7}, {21, 8, 26, 31},
		{3, 13, 8, 36}, {39, 13, 44, 36}, {9, 33, 17, 36}, {30, 33, 38, 36}, {9, 20, 20, 23}, {27, 20, 38, 23}, {10, 13, 12, 19}, {35, 13, 37, 19},
	};
	const FProp Props[] = {
		{'c', 4, 10, true}, {'c', 5, 10, true}, {'c', 12, 5, true}, {'c', 12, 6, true}, {'c', 11, 11, true}, {'c', 8, 3, true},
		{'c', 5, 17, true}, {'c', 6, 17, true}, {'c', 4, 24, true}, {'c', 5, 24, true}, {'c', 6, 29, true}, {'c', 7, 29, true},
		{'c', 14, 21, true}, {'c', 14, 22, true}, {'c', 13, 34, true}, {'c', 13, 35, true},
		{'p', 22, 13, true}, {'p', 22, 27, true}, {'p', 10, 8, true},
		{'c', 23, 10, false}, {'c', 24, 10, false}, {'c', 23, 16, false}, {'c', 24, 16, false}, {'c', 23, 25, false}, {'c', 24, 25, false}, {'c', 23, 29, false}, {'c', 24, 29, false},
	};
	const FRect SiteA = {5, 5, 9, 9}, SiteB = {38, 5, 42, 9};
	const FRect HoldA = {3, 3, 14, 12}, HoldB = {33, 3, 44, 12}, HoldM = {21, 8, 26, 12};
	const FIntPoint SpawnW[] = {{20, 3}, {23, 3}, {26, 3}, {20, 5}, {23, 5}, {26, 5}, {28, 4}};
	const FIntPoint SpawnV[] = {{20, 36}, {23, 36}, {26, 36}, {20, 34}, {23, 34}, {26, 34}, {28, 35}};
	struct FTorch { float X, Y; FColor Color; };
	const FTorch Torches[] = {{7, 7, FColor(255, 154, 60)}, {40, 7, FColor(255, 154, 60)}, {23.5f, 4, FColor(111, 140, 255)}, {23.5f, 35, FColor(255, 60, 60)},
		{23.5f, 20, FColor(176, 108, 255)}, {5, 25, FColor(255, 106, 60)}, {42, 25, FColor(255, 106, 60)}};

}

ABMArena::ABMArena()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // geometry is deterministic: every machine builds the same arena locally
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto MakeISM = [&](const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* C = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Root); C->SetStaticMesh(Cube.Object); C->SetCanEverAffectNavigation(false);
		C->SetCollisionProfileName(TEXT("BlockAll")); C->SetMobility(EComponentMobility::Movable);
		return C;
	};
	Walls = MakeISM(TEXT("Walls")); Pillars = MakeISM(TEXT("Pillars")); Crates = MakeISM(TEXT("Crates"));
	Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	Floor->SetupAttachment(Root); Floor->SetStaticMesh(Cube.Object); Floor->SetCollisionProfileName(TEXT("BlockAll")); Floor->SetMobility(EComponentMobility::Movable);
}

void ABMArena::BeginPlay()
{
	Super::BeginPlay();
	BuildGrid(); BuildMeshes(); BuildLights();
}

void ABMArena::BuildGrid()
{
	TArray<TArray<TCHAR>> G; G.SetNum(MapH);
	for (auto& Row : G) { Row.Init('#', MapW); }
	for (const FRect& R : Carve) for (int32 Y = R.Y1; Y <= R.Y2; ++Y) for (int32 X = R.X1; X <= R.X2; ++X) G[Y][X] = '.';
	for (const FProp& P : Props) { G[P.Y][P.X] = P.Ch; if (P.bMirror) G[P.Y][MapW - 1 - P.X] = P.Ch; }
	W = MapW; H = MapH; Rows.Reset();
	for (const auto& Row : G) Rows.Add(FString(MapW, Row.GetData()));
}

bool ABMArena::IsSolid(int32 Cx, int32 Cy) const
{
	if (Cx < 0 || Cy < 0 || Cx >= W || Cy >= H) return true;
	return Rows[Cy][Cx] != '.';
}

void ABMArena::BuildMeshes()
{
	auto Tint = [this](UPrimitiveComponent* C, const FLinearColor& Col)
	{
		if (UMaterialInstanceDynamic* M = C->CreateAndSetMaterialInstanceDynamic(0)) M->SetVectorParameterValue(TEXT("Color"), Col);
	};
	auto Add = [](UInstancedStaticMeshComponent* C, int32 X, int32 Y, float Height)
	{
		// Engine cube is 100 cm with its pivot at the centre.
		C->AddInstance(FTransform(FRotator::ZeroRotator, FVector((X + 0.5f) * Cell, (Y + 0.5f) * Cell, Height * 0.5f), FVector(Cell / 100.f, Cell / 100.f, Height / 100.f)));
	};
	auto TouchesFree = [this](int32 X, int32 Y)
	{
		for (int32 Dy = -1; Dy <= 1; ++Dy) for (int32 Dx = -1; Dx <= 1; ++Dx) if (!IsSolid(X + Dx, Y + Dy)) return true;
		return false;
	};
	for (int32 Y = 0; Y < H; ++Y) for (int32 X = 0; X < W; ++X)
	{
		const TCHAR Ch = Rows[Y][X];
		if (Ch == '#' && TouchesFree(X, Y)) Add(Walls, X, Y, WallH);
		else if (Ch == 'p') Add(Pillars, X, Y, WallH);
		else if (Ch == 'c') Add(Crates, X, Y, CrateH);
	}
	Floor->SetRelativeLocation(FVector(W * Cell * 0.5f, H * Cell * 0.5f, -5.f));
	Floor->SetRelativeScale3D(FVector(W * Cell / 100.f, H * Cell / 100.f, 0.1f));
	Tint(Walls, FLinearColor(0.26f, 0.09f, 0.11f)); Tint(Pillars, FLinearColor(0.12f, 0.11f, 0.16f));
	Tint(Crates, FLinearColor(0.30f, 0.19f, 0.09f)); Tint(Floor, FLinearColor(0.10f, 0.10f, 0.13f));
}

void ABMArena::BuildLights()
{
	// Moonlight, atmosphere and fog; point lights for torches. Everything is Movable (Lumen, no baked lighting).
	UDirectionalLightComponent* Moon = NewObject<UDirectionalLightComponent>(this);
	Moon->SetMobility(EComponentMobility::Movable); Moon->SetIntensity(1.2f); Moon->SetLightColor(FLinearColor(1.f, 0.55f, 0.5f));
	Moon->SetupAttachment(GetRootComponent()); Moon->SetRelativeRotation(FRotator(-35.f, 110.f, 0.f)); Moon->RegisterComponent();
	USkyAtmosphereComponent* Sky = NewObject<USkyAtmosphereComponent>(this); Sky->SetupAttachment(GetRootComponent()); Sky->RegisterComponent();
	USkyLightComponent* SkyL = NewObject<USkyLightComponent>(this);
	SkyL->SetMobility(EComponentMobility::Movable); SkyL->bRealTimeCapture = true; SkyL->SetIntensity(0.35f); SkyL->SetupAttachment(GetRootComponent()); SkyL->RegisterComponent();
	UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(this);
	Fog->SetFogDensity(0.008f); Fog->SetFogInscatteringColor(FLinearColor(0.12f, 0.05f, 0.1f)); Fog->SetupAttachment(GetRootComponent()); Fog->RegisterComponent();
	for (const FTorch& T : Torches)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetMobility(EComponentMobility::Movable); L->SetLightColor(FLinearColor(T.Color)); L->SetIntensity(9000.f); L->SetAttenuationRadius(2600.f);
		L->SetupAttachment(GetRootComponent()); L->SetRelativeLocation(FVector(T.X * Cell, T.Y * Cell, 240.f)); L->RegisterComponent();
	}
}

static void ZoneRect(TCHAR Site, int32& X1, int32& Y1, int32& X2, int32& Y2)
{
	const FRect& R = Site == 'A' ? SiteA : SiteB; X1 = R.X1; Y1 = R.Y1; X2 = R.X2; Y2 = R.Y2;
}

TCHAR ABMArena::SiteAt(const FVector& P) const
{
	const FIntPoint C = ToCell(P);
	const TCHAR Sites[2] = {'A', 'B'};
	for (const TCHAR S : Sites)
	{
		int32 X1, Y1, X2, Y2; ZoneRect(S, X1, Y1, X2, Y2);
		if (C.X >= X1 && C.X <= X2 && C.Y >= Y1 && C.Y <= Y2) return S;
	}
	return 0;
}

FVector ABMArena::SiteCenter(TCHAR Site) const
{
	int32 X1, Y1, X2, Y2; ZoneRect(Site, X1, Y1, X2, Y2);
	return FVector((X1 + X2 + 1) * 0.5f * Cell, (Y1 + Y2 + 1) * 0.5f * Cell, 0.f);
}

FTransform ABMArena::GetSpawn(EBMTeam Team, int32 Index) const
{
	const bool bW = Team == EBMTeam::Werewolf;
	const FIntPoint P = bW ? SpawnW[Index % UE_ARRAY_COUNT(SpawnW)] : SpawnV[Index % UE_ARRAY_COUNT(SpawnV)];
	// Werewolves start north and face south (+Y); Vampires start south and face north (-Y).
	return FTransform(FRotator(0.f, bW ? 90.f : -90.f, 0.f), CellCenter(P.X, P.Y) + FVector(0, 0, 100.f));
}

FVector ABMArena::RandomHold(TCHAR Region, FRandomStream& Rng) const
{
	const FRect& R = Region == 'A' ? HoldA : Region == 'B' ? HoldB : HoldM;
	for (int32 I = 0; I < 40; ++I)
	{
		const int32 X = Rng.RandRange(R.X1, R.X2), Y = Rng.RandRange(R.Y1, R.Y2);
		if (!IsSolid(X, Y)) return CellCenter(X, Y);
	}
	return CellCenter((R.X1 + R.X2) / 2, (R.Y1 + R.Y2) / 2);
}

bool ABMArena::HitsWall(const FVector& P, float R) const
{
	for (int32 Y = FMath::FloorToInt((P.Y - R) / Cell); Y <= FMath::FloorToInt((P.Y + R) / Cell); ++Y)
		for (int32 X = FMath::FloorToInt((P.X - R) / Cell); X <= FMath::FloorToInt((P.X + R) / Cell); ++X)
		{
			if (!IsSolid(X, Y)) continue;
			const float Nx = FMath::Clamp(P.X, X * Cell, X * Cell + Cell), Ny = FMath::Clamp(P.Y, Y * Cell, Y * Cell + Cell);
			if (FMath::Square(P.X - Nx) + FMath::Square(P.Y - Ny) < R * R) return true;
		}
	return false;
}

bool ABMArena::Walkable(const FVector& A, const FVector& B, float R) const
{
	const float Len = FVector::Dist2D(A, B);
	const int32 N = FMath::Max(1, FMath::CeilToInt(Len / 35.f));
	for (int32 I = 1; I <= N; ++I) if (HitsWall(FMath::Lerp(A, B, float(I) / N), R)) return false;
	return true;
}

FIntPoint ABMArena::NearestFree(FIntPoint C) const
{
	if (!IsSolid(C.X, C.Y)) return C;
	for (int32 R = 1; R < 8; ++R)
		for (int32 Dy = -R; Dy <= R; ++Dy) for (int32 Dx = -R; Dx <= R; ++Dx)
			if (FMath::Max(FMath::Abs(Dx), FMath::Abs(Dy)) == R && !IsSolid(C.X + Dx, C.Y + Dy)) return FIntPoint(C.X + Dx, C.Y + Dy);
	return FIntPoint(-1, -1);
}

bool ABMArena::FindPath(const FVector& From, const FVector& To, TArray<FVector>& Out) const
{
	Out.Reset();
	const FIntPoint S = NearestFree(ToCell(From)), G = NearestFree(ToCell(To));
	if (S.X < 0 || G.X < 0) return false;
	const int32 N = W * H, Start = S.Y * W + S.X, Goal = G.Y * W + G.X;
	TArray<float> Cost; Cost.Init(TNumericLimits<float>::Max(), N);
	TArray<int32> Prev; Prev.Init(-1, N);
	TArray<uint8> Closed; Closed.Init(0, N);
	TArray<int32> Open; Open.Add(Start); Cost[Start] = 0.f;
	auto Heur = [&](int32 I) { const int32 Dx = FMath::Abs(I % W - G.X), Dy = FMath::Abs(I / W - G.Y); return FMath::Max(Dx, Dy) + 0.414f * FMath::Min(Dx, Dy); };
	static const int32 Dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
	while (Open.Num())
	{
		int32 BestIdx = 0;
		for (int32 I = 1; I < Open.Num(); ++I) if (Cost[Open[I]] + Heur(Open[I]) < Cost[Open[BestIdx]] + Heur(Open[BestIdx])) BestIdx = I;
		const int32 Cur = Open[BestIdx]; Open.RemoveAtSwap(BestIdx);
		if (Closed[Cur]) continue;
		Closed[Cur] = 1;
		if (Cur == Goal) break;
		const int32 Cx = Cur % W, Cy = Cur / W;
		for (const auto& D : Dirs)
		{
			const int32 Nx = Cx + D[0], Ny = Cy + D[1];
			if (IsSolid(Nx, Ny)) continue;
			if (D[0] && D[1] && (IsSolid(Cx + D[0], Cy) || IsSolid(Cx, Cy + D[1]))) continue;
			const int32 Ni = Ny * W + Nx;
			if (Closed[Ni]) continue;
			bool bNearWall = false;
			for (const auto& E : Dirs) if (IsSolid(Nx + E[0], Ny + E[1])) { bNearWall = true; break; }
			const float C = Cost[Cur] + ((D[0] && D[1]) ? 1.414f : 1.f) + (bNearWall ? 0.6f : 0.f);
			if (C < Cost[Ni]) { Cost[Ni] = C; Prev[Ni] = Cur; Open.Add(Ni); }
		}
	}
	if (!Closed[Goal]) return false;
	TArray<FVector> Pts;
	for (int32 I = Goal; I != Start && I != -1; I = Prev[I]) Pts.Add(CellCenter(I % W, I / W));
	Algo::Reverse(Pts);
	FVector Cur = From; int32 I = 0;
	while (I < Pts.Num())
	{
		int32 J = Pts.Num() - 1;
		while (J > I && !Walkable(Cur, Pts[J], 50.f)) --J;
		Out.Add(Pts[J]); Cur = Pts[J]; I = J + 1;
	}
	return true;
}
