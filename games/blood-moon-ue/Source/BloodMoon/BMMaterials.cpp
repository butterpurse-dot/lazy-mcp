#include "BMMaterials.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

static UMaterialInterface* LoadRooted(const TCHAR* Path, const TCHAR* Fallback)
{
	UMaterialInterface* M = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
	if (!M) M = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, Fallback, nullptr, LOAD_NoWarn | LOAD_Quiet));
	if (M) M->AddToRoot();
	return M;
}

UMaterialInterface* BMMaterials::Lit()
{
	static UMaterialInterface* M = LoadRooted(TEXT("/Game/Materials/M_BMLit.M_BMLit"), TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	return M;
}

UMaterialInterface* BMMaterials::Emissive()
{
	static UMaterialInterface* M = LoadRooted(TEXT("/Game/Materials/M_BMEmissive.M_BMEmissive"), TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	return M;
}

UMaterialInstanceDynamic* BMMaterials::Apply(UPrimitiveComponent* Comp, UMaterialInterface* Base, const FLinearColor& Color)
{
	if (!Comp || !Base) return nullptr;
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Comp);
	Mid->SetVectorParameterValue(TEXT("Color"), Color);
	for (int32 I = 0; I < FMath::Max(1, Comp->GetNumMaterials()); ++I) Comp->SetMaterial(I, Mid);
	return Mid;
}
