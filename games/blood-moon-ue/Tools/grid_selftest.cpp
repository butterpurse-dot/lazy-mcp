// clang++ -std=c++17 -I../Source/BloodMoon grid_selftest.cpp ../Source/BloodMoon/BMGrid.cpp
#include "BMGrid.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); std::exit(1); } } while (0)

int main()
{
	const FBMGrid G;
	// borders are solid; out of range reads as wall
	for (int X = 0; X < FBMGrid::W; ++X) { CHECK(G.Solid(X, 0)); CHECK(G.Solid(X, FBMGrid::H - 1)); }
	CHECK(G.Solid(-1, 5) && G.Solid(5, 999));
	// left/right symmetry (fair map)
	for (int Y = 0; Y < FBMGrid::H; ++Y) for (int X = 0; X < FBMGrid::W; ++X) CHECK(G.Tile(X, Y) == G.Tile(FBMGrid::W - 1 - X, Y) || (X >= 23 && X <= 24));
	// sites are free of props and recognised
	for (char S : {'A', 'B'})
	{
		const FBMPoint C = G.SiteCenter(S);
		CHECK(G.SiteAt(C.X, C.Y) == S);
		const FBMCell Cc = FBMGrid::ToCell(C.X, C.Y);
		for (int Dy = -2; Dy <= 2; ++Dy) for (int Dx = -2; Dx <= 2; ++Dx) CHECK(!G.Solid(Cc.X + Dx, Cc.Y + Dy));
	}
	CHECK(G.SiteAt(FBMGrid::Center(23, 20).X, FBMGrid::Center(23, 20).Y) == 0);
	// every spawn is free, clear of walls for the 42 cm capsule, and reaches both sites with walkable legs
	for (int T = 0; T < 2; ++T)
		for (int I = 0; I < FBMGrid::NumSpawns(T); ++I)
		{
			const FBMCell Sc = FBMGrid::Spawn(T, I);
			CHECK(!G.Solid(Sc.X, Sc.Y));
			const FBMPoint P = FBMGrid::Center(Sc.X, Sc.Y);
			CHECK(!G.HitsWall(P.X, P.Y, 42.f));
			for (char S : {'A', 'B'})
			{
				std::vector<FBMPoint> Path;
				CHECK(G.FindPath(P, G.SiteCenter(S), Path));
				CHECK(!Path.empty());
				FBMPoint Prev = P;
				for (const FBMPoint& Q : Path) { CHECK(G.Walkable(Prev, Q, 45.f)); Prev = Q; }
				const FBMPoint E = Path.back(), Goal = G.SiteCenter(S);
				CHECK(std::hypot(E.X - Goal.X, E.Y - Goal.Y) < FBMGrid::Cell);
			}
		}
	// every hold region contains free tiles
	for (char R : {'A', 'B', 'M'})
	{
		const FBMRect& Rc = FBMGrid::HoldRect(R); int Free = 0;
		for (int Y = Rc.Y1; Y <= Rc.Y2; ++Y) for (int X = Rc.X1; X <= Rc.X2; ++X) Free += !G.Solid(X, Y);
		CHECK(Free > 10);
	}
	// spawn index wraps safely (incl. negative)
	CHECK(FBMGrid::Spawn(0, 99).X >= 0 && FBMGrid::Spawn(1, -3).X >= 0);
	// a goal inside a wall snaps to the nearest free tile instead of failing
	{ std::vector<FBMPoint> Path; CHECK(G.FindPath(FBMGrid::Center(23, 35), FBMGrid::Center(0, 0), Path)); }
	// movement blocked by walls; crate tiles are solid for movement
	CHECK(G.HitsWall(FBMGrid::Center(23, 10).X, FBMGrid::Center(23, 10).Y, 10.f));
	CHECK(!G.Walkable(FBMGrid::Center(23, 12), FBMGrid::Center(23, 8), 45.f));
	// performance budget: bots repath at most every few seconds; 1000 cross-map paths must stay well under a frame each
	const auto T0 = std::chrono::steady_clock::now();
	std::vector<FBMPoint> Path;
	for (int I = 0; I < 1000; ++I) G.FindPath(FBMGrid::Center(5, 35), FBMGrid::Center(42, 5), Path);
	const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count() / 1000.0;
	std::printf("avg cross-map path: %.3f ms\n", Ms);
	CHECK(Ms < 2.0);
	std::puts("BMGrid self-test passed");
	return 0;
}
