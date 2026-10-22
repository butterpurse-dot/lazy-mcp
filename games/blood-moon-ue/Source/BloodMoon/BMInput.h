#pragma once
#include "CoreMinimal.h"

class UInputAction;
class UInputMappingContext;

/**
 * Enhanced Input actions and the default mapping context, created in code (no editor assets needed).
 * Keyboard/mouse and gamepad are mapped side by side, so the game is playable on PC and with a controller.
 *
 *  Action        KB/M                Gamepad
 *  Move          WASD                Left stick
 *  Look          Mouse               Right stick (dead zone + exponential curve + aim friction)
 *  Fire          LMB                 Right trigger
 *  Jump/Confirm  Space / Enter       A / Cross
 *  Use (hold)    E                   X / Square        plant / cleanse
 *  Reload        R                   B / Circle
 *  Ability       F                   Right bumper
 *  Walk (hold)   Left Shift          Left stick click
 *  Next weapon   Mouse wheel         Y / Triangle
 *  Armory        B                   Left bumper
 *  Scoreboard    Tab (hold)          View / Share (hold)
 *  Menu up/down  Arrow keys          D-pad
 *  Slots / buy   1..9                -
 */
struct FBMInputSet
{
	UInputAction* Move = nullptr;
	UInputAction* LookMouse = nullptr;
	UInputAction* LookStick = nullptr;
	UInputAction* Fire = nullptr;
	UInputAction* Jump = nullptr;
	UInputAction* Confirm = nullptr;
	UInputAction* Use = nullptr;
	UInputAction* Reload = nullptr;
	UInputAction* Ability = nullptr;
	UInputAction* Walk = nullptr;
	UInputAction* NextWeapon = nullptr;
	UInputAction* Armory = nullptr;
	UInputAction* Scoreboard = nullptr;
	UInputAction* MenuNav = nullptr;
	UInputAction* Number[9] = {};
	UInputMappingContext* Context = nullptr;
};

namespace BMInput
{
	/** Built once on first use and rooted for the process lifetime (a few tiny UObjects). */
	const FBMInputSet& Get();
}
