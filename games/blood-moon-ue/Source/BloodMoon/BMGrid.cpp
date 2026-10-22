#include "BMGrid.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>

namespace
{
	struct FProp { char Ch; int32_t X, Y; bool bMirror; };
	const FBMRect Carve[] = {
		{18, 2, 29, 7}, {18, 32, 29, 37}, {3, 3, 14, 12}, {33, 3, 44, 12}, {14, 4, 17, 7}, {30, 4, 33, 7}, {21, 8, 26, 31},
		{3, 13, 8, 36}, {39, 13, 44, 36}, {9, 33, 17, 36}, {30, 33, 38, 36}, {9, 20, 20, 23}, {27, 20, 38, 23}, {10, 13, 12, 19}, {35, 13, 37, 19},
	};
	const FProp Props[] = {
		{'c', 4, 10, true}, {'c', 5, 10, true}, {'c', 12, 5, true}, {'c', 12, 6, true}, {'c', 11, 11, true}, {'c', 8, 3, true},
		{'c', 5, 17, true}, {'c', 6, 17, true}, {'c', 4, 24, true}, {'c', 5, 24, true}, {'c', 6, 29, true}, {'c', 7, 29, true},
		{'c', 14, 21, true}, {'c', 14, 22, true}, {'c', 13, 34, true}, {'c', 13, 35, true},
		{'p', 22, 13, true}, {'p', 22, 27, true}, {'p', 10, 8, true},
		{'c', 23, 10, false}, {'c', 24, 10, false}, {'c', 23, 16, false}, {'c', 24, 16, false}, {'c', 23, 25, false}, {'c', 24, 25, false}, {'c', 23, 29, false}, {'c', 24, 29, false},
	};
	const FBMRect SiteA = {5, 5, 9, 9}, SiteB = {38, 5, 42, 9};
	const FBMRect HoldA = {3, 3, 14, 12}, HoldB = {33, 3, 44, 12}, HoldM = {21, 8, 26, 12};
	const FBMCell SpawnV[] = {{20, 36}, {23, 36}, {26, 36}, {20, 34}, {23, 34}, {26, 34}, {28, 35}};
	const FBMCell SpawnW[] = {{20, 3}, {23, 3}, {26, 3}, {20, 5}, {23, 5}, {26, 5}, {28, 4}};
	const int32_t Dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
	inline bool InRect(const FBMRect& R, FBMCell C) { return C.X >= R.X1 && C.X <= R.X2 && C.Y >= R.Y1 && C.Y <= R.Y2; }
}

FBMGrid::FBMGrid() : Tiles(W * H, Wall), NearWall(W * H, 0)
{
	for (const FBMRect& R : Carve)
		for (int32_t Y = R.Y1; Y <= R.Y2; ++Y) for (int32_t X = R.X1; X <= R.X2; ++X) Tiles[Y * W + X] = Floor;
	for (const FProp& P : Props)
	{
		Tiles[P.Y * W + P.X] = P.Ch;
		if (P.bMirror) Tiles[P.Y * W + (W - 1 - P.X)] = P.Ch;
	}
	for (int32_t Y = 0; Y < H; ++Y)
		for (int32_t X = 0; X < W; ++X)
			if (!Solid(X, Y))
				for (const auto& D : Dirs) if (Solid(X + D[0], Y + D[1])) { NearWall[Y * W + X] = 1; break; }
}

FBMCell FBMGrid::ToCell(float X, float Y) { return {static_cast<int32_t>(std::floor(X / Cell)), static_cast<int32_t>(std::floor(Y / Cell))}; }

char FBMGrid::SiteAt(float X, float Y) const
{
	const FBMCell C = ToCell(X, Y);
	return InRect(SiteA, C) ? 'A' : InRect(SiteB, C) ? 'B' : 0;
}

FBMPoint FBMGrid::SiteCenter(char Site) const
{
	const FBMRect& R = Site == 'A' ? SiteA : SiteB;
	return {(R.X1 + R.X2 + 1) * 0.5f * Cell, (R.Y1 + R.Y2 + 1) * 0.5f * Cell};
}

const FBMRect& FBMGrid::HoldRect(char Region) { return Region == 'A' ? HoldA : Region == 'B' ? HoldB : HoldM; }

int32_t FBMGrid::NumSpawns(int32_t Team) { return Team == 0 ? int32_t(sizeof(SpawnV) / sizeof(SpawnV[0])) : int32_t(sizeof(SpawnW) / sizeof(SpawnW[0])); }

FBMCell FBMGrid::Spawn(int32_t Team, int32_t Index)
{
	const int32_t N = NumSpawns(Team), I = ((Index % N) + N) % N;
	return Team == 0 ? SpawnV[I] : SpawnW[I];
}

bool FBMGrid::HitsWall(float X, float Y, float R) const
{
	const FBMCell Lo = ToCell(X - R, Y - R), Hi = ToCell(X + R, Y + R);
	for (int32_t Cy = Lo.Y; Cy <= Hi.Y; ++Cy)
		for (int32_t Cx = Lo.X; Cx <= Hi.X; ++Cx)
		{
			if (!Solid(Cx, Cy)) continue;
			const float Nx = std::clamp(X, Cx * Cell, Cx * Cell + Cell), Ny = std::clamp(Y, Cy * Cell, Cy * Cell + Cell);
			if ((X - Nx) * (X - Nx) + (Y - Ny) * (Y - Ny) < R * R) return true;
		}
	return false;
}

bool FBMGrid::Walkable(FBMPoint A, FBMPoint B, float R) const
{
	const float Len = std::hypot(B.X - A.X, B.Y - A.Y);
	const int32_t N = std::max(1, static_cast<int32_t>(std::ceil(Len / 35.f)));
	for (int32_t I = 1; I <= N; ++I)
	{
		const float T = float(I) / N;
		if (HitsWall(A.X + (B.X - A.X) * T, A.Y + (B.Y - A.Y) * T, R)) return false;
	}
	return true;
}

bool FBMGrid::NearestFree(FBMCell C, FBMCell& Out) const
{
	if (!Solid(C.X, C.Y)) { Out = C; return true; }
	for (int32_t R = 1; R < 8; ++R)
		for (int32_t Dy = -R; Dy <= R; ++Dy)
			for (int32_t Dx = -R; Dx <= R; ++Dx)
				if (std::max(std::abs(Dx), std::abs(Dy)) == R && !Solid(C.X + Dx, C.Y + Dy)) { Out = {C.X + Dx, C.Y + Dy}; return true; }
	return false;
}

bool FBMGrid::FindPath(FBMPoint From, FBMPoint To, std::vector<FBMPoint>& Out, float Radius) const
{
	Out.clear();
	FBMCell S, G;
	if (!NearestFree(ToCell(From.X, From.Y), S) || !NearestFree(ToCell(To.X, To.Y), G)) return false;
	const int32_t N = W * H, Start = S.Y * W + S.X, Goal = G.Y * W + G.X;
	std::vector<float> Cost(N, 1e30f);
	std::vector<int32_t> Prev(N, -1);
	std::vector<uint8_t> Closed(N, 0);
	auto Heur = [&](int32_t I) { const int32_t Dx = std::abs(I % W - G.X), Dy = std::abs(I / W - G.Y); return float(std::max(Dx, Dy)) + 0.414f * float(std::min(Dx, Dy)); };
	using FNode = std::pair<float, int32_t>; // (f, index), min-heap
	std::priority_queue<FNode, std::vector<FNode>, std::greater<FNode>> Open;
	Cost[Start] = 0.f; Open.push({Heur(Start), Start});
	while (!Open.empty())
	{
		const int32_t Cur = Open.top().second; Open.pop();
		if (Closed[Cur]) continue;
		Closed[Cur] = 1;
		if (Cur == Goal) break;
		const int32_t Cx = Cur % W, Cy = Cur / W;
		for (const auto& D : Dirs)
		{
			const int32_t Nx = Cx + D[0], Ny = Cy + D[1];
			if (Solid(Nx, Ny)) continue;
			if (D[0] && D[1] && (Solid(Cx + D[0], Cy) || Solid(Cx, Cy + D[1]))) continue;
			const int32_t Ni = Ny * W + Nx;
			if (Closed[Ni]) continue;
			const float C = Cost[Cur] + ((D[0] && D[1]) ? 1.414f : 1.f) + (NearWall[Ni] ? 0.6f : 0.f);
			if (C < Cost[Ni]) { Cost[Ni] = C; Prev[Ni] = Cur; Open.push({C + Heur(Ni), Ni}); }
		}
	}
	if (!Closed[Goal]) return false;
	std::vector<FBMPoint> Pts;
	for (int32_t I = Goal; I != Start && I != -1; I = Prev[I]) Pts.push_back(Center(I % W, I / W));
	std::reverse(Pts.begin(), Pts.end());
	FBMPoint Cur = From;
	for (size_t I = 0; I < Pts.size();)
	{
		size_t J = Pts.size() - 1;
		while (J > I && !Walkable(Cur, Pts[J], Radius)) --J;
		Out.push_back(Pts[J]); Cur = Pts[J]; I = J + 1;
	}
	return true;
}
