#include "BMSigil.h"
#include "BMGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABMSigil::ABMSigil()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	SetRootComponent(Disc);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Disc->SetStaticMesh(Cyl.Object);
	Disc->SetWorldScale3D(FVector(3.f, 3.f, 0.05f));
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Disc); Glow->SetMobility(EComponentMobility::Movable);
	Glow->SetLightColor(FLinearColor(1.f, 0.05f, 0.12f)); Glow->SetAttenuationRadius(1800.f); Glow->SetRelativeLocation(FVector(0, 0, 2000.f));
}

void ABMSigil::Tick(float Dt)
{
	Super::Tick(Dt);
	const ABMGameState* GS = GetWorld()->GetGameState<ABMGameState>();
	const float Frac = GS ? FMath::Clamp(GS->SigilRemaining() / 35.f, 0.f, 1.f) : 1.f;
	const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * (4.f + (1.f - Frac) * 14.f));
	Glow->SetIntensity(3000.f + 30000.f * Pulse);
	Disc->AddLocalRotation(FRotator(0.f, 40.f * Dt, 0.f));
	if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Disc->GetMaterial(0))) M->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.05f, 0.12f) * (2.f + 8.f * Pulse));
	else Disc->CreateAndSetMaterialInstanceDynamic(0);
}
