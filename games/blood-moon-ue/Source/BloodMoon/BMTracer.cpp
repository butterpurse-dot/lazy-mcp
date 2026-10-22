#include "BMTracer.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABMTracer::ABMTracer()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 0.07f;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
}

void ABMTracer::Init(const FVector& Start, const FVector& End, const FLinearColor& Color)
{
	const FVector D = End - Start;
	const float Len = D.Size();
	if (Len < 20.f) { Destroy(); return; }
	SetActorLocationAndRotation(Start + D * 0.5f, D.Rotation());
	SetActorScale3D(FVector(Len / 100.f, 0.02f, 0.02f));
	if (UMaterialInstanceDynamic* M = Mesh->CreateAndSetMaterialInstanceDynamic(0)) M->SetVectorParameterValue(TEXT("Color"), Color * 20.f);
}
