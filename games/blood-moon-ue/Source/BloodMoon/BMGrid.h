#pragma once
// Engine-free tile map + navigation for "Crimson Cathedral" (port of config/map.js + core/grid.js).
// Uses only the C++ standard library so it is unit-tested with plain clang (Tools/grid_selftest.cpp)
// and can run on a dedicated server unchanged. Units: centimetres; X = east, Y = south; 1 tile = 150 cm.
#include <cstdint>
#include <vector>

struct FBMPoint { float X = 0.f, Y = 0.f; };
struct FBMRect { int32_t X1, Y1, X2, Y2; };
struct FBMCell { int32_t X, Y; };

class FBMGrid
{
public:
	static constexpr int32_t W = 48, H = 40;
	static constexpr float Cell = 150.f, WallH = 360.f, CrateH = 130.f;
	enum ETile : char { Floor = '.', Wall = '#', Pillar = 'p', Crate = 'c' };

	FBMGrid();

	char Tile(int32_t Cx, int32_t Cy) const { return In(Cx, Cy) ? Tiles[Cy * W + Cx] : Wall; }
	bool Solid(int32_t Cx, int32_t Cy) const { return Tile(Cx, Cy) != Floor; }
	static FBMCell ToCell(float X, float Y);
	static FBMPoint Center(int32_t Cx, int32_t Cy) { return {(Cx + 0.5f) * Cell, (Cy + 0.5f) * Cell}; }

	/** 'A' / 'B' when the point is inside a bomb site, else 0. */
	char SiteAt(float X, float Y) const;
	FBMPoint SiteCenter(char Site) const;
	static const FBMRect& HoldRect(char Region); // 'A', 'B' or 'M' (mid)
	/** Spawn cell for a team (0 Vampire, 1 Werewolf); wraps when Index exceeds the list. */
	static FBMCell Spawn(int32_t Team, int32_t Index);
	static int32_t NumSpawns(int32_t Team);

	bool HitsWall(float X, float Y, float Radius) const;
	bool Walkable(FBMPoint A, FBMPoint B, float Radius) const;
	/** A* (8-way, no corner cutting, wall-hugging penalty) then string-pulled. Returns false if unreachable. */
	bool FindPath(FBMPoint From, FBMPoint To, std::vector<FBMPoint>& Out, float Radius = 50.f) const;

private:
	static bool In(int32_t Cx, int32_t Cy) { return Cx >= 0 && Cy >= 0 && Cx < W && Cy < H; }
	bool NearestFree(FBMCell C, FBMCell& Out) const;
	std::vector<char> Tiles;
	std::vector<uint8_t> NearWall; // free tile touching a solid one
};
