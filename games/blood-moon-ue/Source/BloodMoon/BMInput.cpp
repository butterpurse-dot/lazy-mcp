#include "BMInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

namespace
{
	UInputAction* MakeAction(const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(GetTransientPackage(), FName(Name));
		A->ValueType = Type;
		A->AddToRoot();
		return A;
	}

	template <typename T> T* Mod(UInputMappingContext* Ctx) { return NewObject<T>(Ctx); }

	void Map(UInputMappingContext* Ctx, UInputAction* A, const FKey& Key, bool bSwizzle = false, bool bNegate = false)
	{
		FEnhancedActionKeyMapping& M = Ctx->MapKey(A, Key);
		if (bSwizzle) { UInputModifierSwizzleAxis* S = Mod<UInputModifierSwizzleAxis>(Ctx); S->Order = EInputAxisSwizzle::YXZ; M.Modifiers.Add(S); }
		if (bNegate) M.Modifiers.Add(Mod<UInputModifierNegate>(Ctx));
	}

	void MapStick(UInputMappingContext* Ctx, UInputAction* A, const FKey& Key, bool bCurve)
	{
		FEnhancedActionKeyMapping& M = Ctx->MapKey(A, Key);
		UInputModifierDeadZone* Dz = Mod<UInputModifierDeadZone>(Ctx);
		Dz->LowerThreshold = 0.15f; Dz->UpperThreshold = 1.f; Dz->Type = EDeadZoneType::Radial;
		M.Modifiers.Add(Dz);
		if (bCurve)
		{
			UInputModifierResponseCurveExponential* C = Mod<UInputModifierResponseCurveExponential>(Ctx);
			C->CurveExponent = FVector(2.f, 2.f, 1.f); // fine aim near centre, fast turns at full deflection
			M.Modifiers.Add(C);
		}
	}

	FBMInputSet Build()
	{
		FBMInputSet S;
		using V = EInputActionValueType;
		S.Move = MakeAction(TEXT("BM_Move"), V::Axis2D);
		S.LookMouse = MakeAction(TEXT("BM_LookMouse"), V::Axis2D);
		S.LookStick = MakeAction(TEXT("BM_LookStick"), V::Axis2D);
		S.Fire = MakeAction(TEXT("BM_Fire"), V::Boolean);
		S.Jump = MakeAction(TEXT("BM_Jump"), V::Boolean);
		S.Confirm = MakeAction(TEXT("BM_Confirm"), V::Boolean);
		S.Use = MakeAction(TEXT("BM_Use"), V::Boolean);
		S.Reload = MakeAction(TEXT("BM_Reload"), V::Boolean);
		S.Ability = MakeAction(TEXT("BM_Ability"), V::Boolean);
		S.Walk = MakeAction(TEXT("BM_Walk"), V::Boolean);
		S.NextWeapon = MakeAction(TEXT("BM_NextWeapon"), V::Boolean);
		S.Armory = MakeAction(TEXT("BM_Armory"), V::Boolean);
		S.Scoreboard = MakeAction(TEXT("BM_Scoreboard"), V::Boolean);
		S.MenuNav = MakeAction(TEXT("BM_MenuNav"), V::Axis1D);
		static const TCHAR* NumNames[9] = {TEXT("BM_Num1"), TEXT("BM_Num2"), TEXT("BM_Num3"), TEXT("BM_Num4"), TEXT("BM_Num5"), TEXT("BM_Num6"), TEXT("BM_Num7"), TEXT("BM_Num8"), TEXT("BM_Num9")};
		for (int32 I = 0; I < 9; ++I) S.Number[I] = MakeAction(NumNames[I], V::Boolean);

		UInputMappingContext* C = NewObject<UInputMappingContext>(GetTransientPackage(), TEXT("BM_Default"));
		C->AddToRoot();
		S.Context = C;
		// Move: D = +X, A = -X, W = +Y, S = -Y
		Map(C, S.Move, EKeys::D); Map(C, S.Move, EKeys::A, false, true);
		Map(C, S.Move, EKeys::W, true); Map(C, S.Move, EKeys::S, true, true);
		MapStick(C, S.Move, EKeys::Gamepad_Left2D, false);
		Map(C, S.LookMouse, EKeys::Mouse2D);
		MapStick(C, S.LookStick, EKeys::Gamepad_Right2D, true);
		Map(C, S.Fire, EKeys::LeftMouseButton); Map(C, S.Fire, EKeys::Gamepad_RightTrigger);
		Map(C, S.Jump, EKeys::SpaceBar); Map(C, S.Jump, EKeys::Gamepad_FaceButton_Bottom);
		Map(C, S.Confirm, EKeys::Enter); Map(C, S.Confirm, EKeys::Gamepad_FaceButton_Bottom);
		Map(C, S.Use, EKeys::E); Map(C, S.Use, EKeys::Gamepad_FaceButton_Left);
		Map(C, S.Reload, EKeys::R); Map(C, S.Reload, EKeys::Gamepad_FaceButton_Right);
		Map(C, S.Ability, EKeys::F); Map(C, S.Ability, EKeys::Gamepad_RightShoulder);
		Map(C, S.Walk, EKeys::LeftShift); Map(C, S.Walk, EKeys::Gamepad_LeftThumbstick);
		Map(C, S.NextWeapon, EKeys::MouseScrollUp); Map(C, S.NextWeapon, EKeys::MouseScrollDown); Map(C, S.NextWeapon, EKeys::Gamepad_FaceButton_Top);
		Map(C, S.Armory, EKeys::B); Map(C, S.Armory, EKeys::Gamepad_LeftShoulder);
		Map(C, S.Scoreboard, EKeys::Tab); Map(C, S.Scoreboard, EKeys::Gamepad_Special_Left);
		// MenuNav: +1 = down the list, -1 = up
		Map(C, S.MenuNav, EKeys::Down); Map(C, S.MenuNav, EKeys::Gamepad_DPad_Down);
		Map(C, S.MenuNav, EKeys::Up, false, true); Map(C, S.MenuNav, EKeys::Gamepad_DPad_Up, false, true);
		static const FKey Digits[9] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
		for (int32 I = 0; I < 9; ++I) Map(C, S.Number[I], Digits[I]);
		return S;
	}
}

const FBMInputSet& BMInput::Get()
{
	static const FBMInputSet Set = Build();
	return Set;
}
