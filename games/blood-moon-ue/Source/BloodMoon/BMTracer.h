#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BMTracer.generated.h"

/** Short-lived bullet trail. Cosmetic only, never replicated (spawned by a multicast on every machine). */
UCLASS()
class BLOODMOON_API ABMTracer : public AActor
{
	GENERATED_BODY()
public:
	ABMTracer();
	void Init(const FVector& Start, const FVector& End, const FLinearColor& Color);
private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
};
