#pragma once
#include "CoreMinimal.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;

/**
 * Shared base materials, each exposing a vector parameter "Color". Project materials are generated once by
 * Content/Python/init_unreal.py; until they exist we fall back to the engine's BasicShapeMaterial so nothing crashes.
 * Loaded assets are rooted once and reused: no per-frame loads, one MID per component.
 */
namespace BMMaterials
{
	UMaterialInterface* Lit();      // "Color" -> base colour
	UMaterialInterface* Emissive(); // "Color" -> emissive (tracers, sigil)
	UMaterialInstanceDynamic* Apply(UPrimitiveComponent* Comp, UMaterialInterface* Base, const FLinearColor& Color);
}
