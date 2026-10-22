#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BMSigil.generated.h"

/** The planted Blood Sigil: a glowing rune disc. Fuse/cleanse logic lives in ABMGameMode; this is presentation. */
UCLASS()
class BLOODMOON_API ABMSigil : public AActor
{
	GENERATED_BODY()
public:
	ABMSigil();
	virtual void Tick(float Dt) override;
private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Disc;
	UPROPERTY() TObjectPtr<class UPointLightComponent> Glow;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> Mid;
};
