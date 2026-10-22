#include "BMArena.h"
#include "BMMaterials.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	struct FTorch { float X, Y; FColor Color; };
	const FTorch Torches[] = {{7, 7, FColor(255, 154, 60)}, {40, 7, FColor(255, 154, 60)}, {23.5f, 4, FColor(111, 140, 255)}, {23.5f, 35, FColor(255, 60, 60)},
		{23.5f, 20, FColor(176, 108, 255)}, {5, 25, FColor(255, 106, 60)}, {42, 25, FColor(255, 106, 60)}};
}

ABMArena::ABMArena()
{
	PrimaryActorTick.bCanEverTick = false;
	// Replicated only so remote clients get the actor; its geometry is deterministic and built locally on each machine.
	bReplicates = true; bAlwaysRelevant = true; SetReplicatingMovement(false); SetNetUpdateFrequency(1.f);
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto MakeISM = [&](const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* C = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Root); C->SetStaticMesh(Cube.Object); C->SetCanEverAffectNavigation(false);
		C->SetCollisionProfileName(TEXT("BlockAll")); C->SetMobility(EComponentMobility::Static);
		return C;
	};
	Walls = MakeISM(TEXT("Walls")); Pillars = MakeISM(TEXT("Pillars")); Crates = MakeISM(TEXT("Crates"));
	Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	Floor->SetupAttachment(Root); Floor->SetStaticMesh(Cube.Object); Floor->SetCollisionProfileName(TEXT("BlockAll")); Floor->SetMobility(EComponentMobility::Static);
}

void ABMArena::BeginPlay()
{
	Super::BeginPlay();
	BuildMeshes(); // collision is needed everywhere (server authority + client movement prediction)
	if (GetNetMode() != NM_DedicatedServer) BuildLights();
}

void ABMArena::BuildMeshes()
{
	auto Add = [](UInstancedStaticMeshComponent* C, int32 X, int32 Y, float Height)
	{
		// Engine cube is 100 cm with its pivot at the centre.
		C->AddInstance(FTransform(FRotator::ZeroRotator, FVector((X + 0.5f) * Cell, (Y + 0.5f) * Cell, Height * 0.5f), FVector(Cell / 100.f, Cell / 100.f, Height / 100.f)));
	};
	auto TouchesFree = [this](int32 X, int32 Y)
	{
		for (int32 Dy = -1; Dy <= 1; ++Dy) for (int32 Dx = -1; Dx <= 1; ++Dx) if (!Map.Solid(X + Dx, Y + Dy)) return true;
		return false;
	};
	for (int32 Y = 0; Y < FBMGrid::H; ++Y) for (int32 X = 0; X < FBMGrid::W; ++X)
	{
		switch (Map.Tile(X, Y))
		{
		case FBMGrid::Wall: if (TouchesFree(X, Y)) Add(Walls, X, Y, FBMGrid::WallH); break; // hidden interior walls are skipped
		case FBMGrid::Pillar: Add(Pillars, X, Y, FBMGrid::WallH); break;
		case FBMGrid::Crate: Add(Crates, X, Y, FBMGrid::CrateH); break;
		default: break;
		}
	}
	Floor->SetRelativeLocation(FVector(FBMGrid::W * Cell * 0.5f, FBMGrid::H * Cell * 0.5f, -5.f));
	Floor->SetRelativeScale3D(FVector(FBMGrid::W * Cell / 100.f, FBMGrid::H * Cell / 100.f, 0.1f));
	BMMaterials::Apply(Walls, BMMaterials::Lit(), FLinearColor(0.26f, 0.09f, 0.11f));
	BMMaterials::Apply(Pillars, BMMaterials::Lit(), FLinearColor(0.12f, 0.11f, 0.16f));
	BMMaterials::Apply(Crates, BMMaterials::Lit(), FLinearColor(0.30f, 0.19f, 0.09f));
	BMMaterials::Apply(Floor, BMMaterials::Lit(), FLinearColor(0.10f, 0.10f, 0.13f));
}

void ABMArena::BuildLights()
{
	// Night: a red moon (not an atmosphere "sun", which would render a daytime sky), sky light, fog and torches.
	// Everything is Movable so Lumen handles GI; nothing is baked.
	UDirectionalLightComponent* Moon = NewObject<UDirectionalLightComponent>(this);
	Moon->SetMobility(EComponentMobility::Movable); Moon->SetIntensity(3.f); Moon->SetLightColor(FLinearColor(1.f, 0.55f, 0.5f));
	Moon->SetAtmosphereSunLight(false);
	Moon->SetupAttachment(GetRootComponent()); Moon->SetRelativeRotation(FRotator(-35.f, 110.f, 0.f)); Moon->RegisterComponent();
	USkyLightComponent* SkyL = NewObject<USkyLightComponent>(this);
	SkyL->SetMobility(EComponentMobility::Movable); SkyL->bRealTimeCapture = true; SkyL->SetIntensity(0.35f); SkyL->SetupAttachment(GetRootComponent()); SkyL->RegisterComponent();
	UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(this);
	Fog->SetFogDensity(0.008f); Fog->SetFogInscatteringColor(FLinearColor(0.12f, 0.05f, 0.1f)); Fog->SetupAttachment(GetRootComponent()); Fog->RegisterComponent();
	for (const FTorch& T : Torches)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetMobility(EComponentMobility::Movable); L->SetLightColor(FLinearColor(T.Color)); L->SetIntensity(9000.f); L->SetAttenuationRadius(2600.f);
		L->SetCastShadows(false); // 7 shadow-casting point lights is a large GPU cost for little visual gain
		L->SetupAttachment(GetRootComponent()); L->SetRelativeLocation(FVector(T.X * Cell, T.Y * Cell, 240.f)); L->RegisterComponent();
	}
}

FTransform ABMArena::GetSpawn(EBMTeam Team, int32 Index) const
{
	const bool bW = Team == EBMTeam::Werewolf;
	const FBMCell C = FBMGrid::Spawn(bW ? 1 : 0, Index);
	const FBMPoint P = FBMGrid::Center(C.X, C.Y);
	// Werewolves start north and face south (+Y); Vampires start south and face north (-Y).
	return FTransform(FRotator(0.f, bW ? 90.f : -90.f, 0.f), FVector(P.X, P.Y, 100.f));
}

FVector ABMArena::RandomHold(TCHAR Region, FRandomStream& Rng) const
{
	const FBMRect& R = FBMGrid::HoldRect(static_cast<char>(Region));
	for (int32 I = 0; I < 40; ++I)
	{
		const int32 X = Rng.RandRange(R.X1, R.X2), Y = Rng.RandRange(R.Y1, R.Y2);
		if (!Map.Solid(X, Y)) { const FBMPoint P = FBMGrid::Center(X, Y); return FVector(P.X, P.Y, 0.f); }
	}
	const FBMPoint P = FBMGrid::Center((R.X1 + R.X2) / 2, (R.Y1 + R.Y2) / 2);
	return FVector(P.X, P.Y, 0.f);
}

bool ABMArena::FindPath(const FVector& From, const FVector& To, TArray<FVector>& Out) const
{
	Out.Reset();
	std::vector<FBMPoint> Pts;
	if (!Map.FindPath({float(From.X), float(From.Y)}, {float(To.X), float(To.Y)}, Pts)) return false;
	Out.Reserve(static_cast<int32>(Pts.size()));
	for (const FBMPoint& P : Pts) Out.Add(FVector(P.X, P.Y, 0.f));
	return true;
}
