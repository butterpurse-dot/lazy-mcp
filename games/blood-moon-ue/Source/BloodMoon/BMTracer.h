#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "BMTracer.generated.h"

class UMaterialInstanceDynamic;

/** Bullet trail. Pooled and reused (never spawned per shot); ticks only while visible. Cosmetic, never replicated. */
UCLASS()
class BLOODMOON_API ABMTracer : public AActor
{
	GENERATED_BODY()
public:
	ABMTracer();
	virtual void Tick(float Dt) override;
	void Activate(const FVector& Start, const FVector& End, const FLinearColor& Color);
private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Mid;
	float TimeLeft = 0.f;
};

/** Fixed-size tracer pool per game world (not created on dedicated servers). */
UCLASS()
class BLOODMOON_API UBMFxSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	void Tracer(const FVector& Start, const FVector& End, const FLinearColor& Color);
private:
	static constexpr int32 PoolSize = 48;
	UPROPERTY() TArray<TObjectPtr<ABMTracer>> Pool;
	int32 Next = 0;
};
