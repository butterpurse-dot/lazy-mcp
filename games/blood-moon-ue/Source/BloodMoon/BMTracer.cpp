#include "BMTracer.h"
#include "BMMaterials.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABMTracer::ABMTracer()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetMobility(EComponentMobility::Movable);
	SetActorHiddenInGame(true);
}

void ABMTracer::Activate(const FVector& Start, const FVector& End, const FLinearColor& Color)
{
	const FVector D = End - Start;
	const float Len = D.Size();
	if (Len < 20.f) return;
	if (!Mid) Mid = BMMaterials::Apply(Mesh, BMMaterials::Emissive(), Color);
	else Mid->SetVectorParameterValue(TEXT("Color"), Color);
	SetActorLocationAndRotation(Start + D * 0.5f, D.Rotation());
	SetActorScale3D(FVector(Len / 100.f, 0.02f, 0.02f));
	SetActorHiddenInGame(false);
	TimeLeft = 0.07f;
	SetActorTickEnabled(true);
}

void ABMTracer::Tick(float Dt)
{
	Super::Tick(Dt);
	if ((TimeLeft -= Dt) > 0.f) return;
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
}

bool UBMFxSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && W->IsGameWorld() && W->GetNetMode() != NM_DedicatedServer;
}

void UBMFxSubsystem::Tracer(const FVector& Start, const FVector& End, const FLinearColor& Color)
{
	if (Pool.Num() < PoolSize)
	{
		FActorSpawnParameters P; P.ObjectFlags |= RF_Transient;
		if (ABMTracer* T = GetWorld()->SpawnActor<ABMTracer>(ABMTracer::StaticClass(), FTransform::Identity, P)) Pool.Add(T);
	}
	if (Pool.Num() == 0) return;
	Next = (Next + 1) % Pool.Num();
	if (ABMTracer* T = Pool[Next]) T->Activate(Start, End, Color * 8.f);
}
