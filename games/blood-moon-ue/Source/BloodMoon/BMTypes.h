#pragma once
#include "CoreMinimal.h"
#include "BMTypes.generated.h"

UENUM(BlueprintType)
enum class EBMTeam : uint8 { Vampire = 0, Werewolf = 1, None = 2 };

UENUM(BlueprintType)
enum class EBMPhase : uint8 { Warmup, Freeze, Live, RoundEnd, MatchOver };

USTRUCT(BlueprintType)
struct FBMAmmo
{
	GENERATED_BODY()
	UPROPERTY() int32 Mag = 0;
	UPROPERTY() int32 Reserve = 0;
};

inline EBMTeam BMOther(EBMTeam T) { return T == EBMTeam::Vampire ? EBMTeam::Werewolf : EBMTeam::Vampire; }
inline int32 BMIdx(EBMTeam T) { return static_cast<int32>(T); }
