namespace SpaceHulksLevelEditor;

/// <summary>
/// Pure geometry logic for exterior hull walls, extracted for testability.
/// </summary>
public static class HullGeometry
{
    public record struct WallSegment(float X0, float Z0, float X1, float Z1, bool IsWindow);

    /// <summary>
    /// Compute exterior wall segments for a single hull-inside cell.
    /// Returns the wall segments that should be emitted (position only, no rendering).
    /// </summary>
    public static List<WallSegment> ComputeExteriorWalls(
        bool[,] hullInside,
        HashSet<(int gx, int gy, WallDir dir)> winSet,
        int gx, int gy,
        float cellSize, float chamfer, float fractionalInset)
    {
        var walls = new List<WallSegment>();
        float c = chamfer;
        float f = fractionalInset;

        float x0 = (gx - 1) * cellSize, x1 = gx * cellSize;
        float z0 = (gy - 1) * cellSize, z1 = gy * cellSize;

        bool oN = !hullInside[gy - 1, gx];
        bool oS = !hullInside[gy + 1, gx];
        bool oW = !hullInside[gy, gx - 1];
        bool oE = !hullInside[gy, gx + 1];

        // Per-face window check - adjacent outside cell is a window cell
        bool wN = oN && IsWinCell(winSet, gx, gy - 1);
        bool wS = oS && IsWinCell(winSet, gx, gy + 1);
        bool wW = oW && IsWinCell(winSet, gx - 1, gy);
        bool wE = oE && IsWinCell(winSet, gx + 1, gy);

        // Detect convex corners (skip when diagonal cell is a window)
        bool cNW = oN && oW && c > 0 && !IsWinCell(winSet, gx - 1, gy - 1);
        bool cNE = oN && oE && c > 0 && !IsWinCell(winSet, gx + 1, gy - 1);
        bool cSW = oS && oW && c > 0 && !IsWinCell(winSet, gx - 1, gy + 1);
        bool cSE = oS && oE && c > 0 && !IsWinCell(winSet, gx + 1, gy + 1);

        // Inset exposed faces (0 for window faces)
        float fN = wN ? 0 : f, fS = wS ? 0 : f, fW = wW ? 0 : f, fE = wE ? 0 : f;
        if (oN && fN > 0 && (IsWinCell(winSet, gx - 1, gy - 1) || IsWinCell(winSet, gx + 1, gy - 1))) fN = 0;
        if (oS && fS > 0 && (IsWinCell(winSet, gx - 1, gy + 1) || IsWinCell(winSet, gx + 1, gy + 1))) fS = 0;
        if (oW && fW > 0 && (IsWinCell(winSet, gx - 1, gy - 1) || IsWinCell(winSet, gx - 1, gy + 1))) fW = 0;
        if (oE && fE > 0 && (IsWinCell(winSet, gx + 1, gy - 1) || IsWinCell(winSet, gx + 1, gy + 1))) fE = 0;

        float ex0 = oW ? x0 + fW : x0;
        float ex1 = oE ? x1 - fE : x1;
        float ez0 = oN ? z0 + fN : z0;
        float ez1 = oS ? z1 - fS : z1;

        // North wall
        if (oN)
        {
            float nx0 = cNW ? ex0 + c : ex0;
            float nx1 = cNE ? ex1 - c : ex1;
            if (nx0 < nx1)
                walls.Add(new WallSegment(nx0, ez0, nx1, ez0, wN));
        }
        // South wall
        if (oS)
        {
            float sx0 = cSW ? ex0 + c : ex0;
            float sx1 = cSE ? ex1 - c : ex1;
            if (sx0 < sx1)
                walls.Add(new WallSegment(sx0, ez1, sx1, ez1, wS));
        }
        // West wall
        if (oW)
        {
            float wz0 = cNW ? ez0 + c : ez0;
            float wz1 = cSW ? ez1 - c : ez1;
            if (wz0 < wz1)
                walls.Add(new WallSegment(ex0, wz0, ex0, wz1, wW));
        }
        // East wall
        if (oE)
        {
            float wz0e = cNE ? ez0 + c : ez0;
            float wz1e = cSE ? ez1 - c : ez1;
            if (wz0e < wz1e)
                walls.Add(new WallSegment(ex1, wz0e, ex1, wz1e, wE));
        }

        // Diagonal chamfer walls at convex corners
        if (cNW) walls.Add(new WallSegment(ex0, ez0 + c, ex0 + c, ez0, false));
        if (cNE) walls.Add(new WallSegment(ex1 - c, ez0, ex1, ez0 + c, false));
        if (cSW) walls.Add(new WallSegment(ex0 + c, ez1, ex0, ez1 - c, false));
        if (cSE) walls.Add(new WallSegment(ex1, ez1 - c, ex1 - c, ez1, false));

        return walls;
    }

    /// <summary>
    /// Compute small connector walls where a flush window meets adjacent inset hull walls.
    /// These fill the gap between the window face (at cell boundary) and the inset neighbor.
    /// </summary>
    public static List<WallSegment> ComputeWindowConnectors(
        bool[,] hullInside,
        HashSet<(int gx, int gy, WallDir dir)> winSet,
        int w, int h,
        float cellSize, float fractionalInset)
    {
        var connectors = new List<WallSegment>();
        float f = fractionalInset;
        if (f <= 0) return connectors;

        foreach (var dir in new[] { WallDir.North, WallDir.South, WallDir.East, WallDir.West })
        {
            foreach (var (gx, gy, d) in winSet)
            {
                if (d != dir) continue;

                if (dir == WallDir.North || dir == WallDir.South)
                {
                    // Window on N/S face of wall cell (gx,gy)
                    // The inside cell is one step in the opposite direction
                    int iy = dir == WallDir.South ? gy + 1 : gy - 1;
                    if (iy < 1 || iy > h) continue;
                    if (!hullInside[iy, gx]) continue;

                    float wz = dir == WallDir.South ? gy * cellSize : (gy - 1) * cellSize;
                    float iz = dir == WallDir.South ? wz - f : wz + f;

                    // Left neighbor: hull cell to the left also has an exposed north/south face
                    // but it's inset by f - need a connector from flush to inset
                    if (gx - 1 >= 1 && hullInside[iy, gx - 1] && !hullInside[dir == WallDir.South ? iy + 1 : iy - 1, gx - 1]
                        && !IsWinCell(winSet, gx - 1, gy))
                    {
                        float bx = (gx - 1) * cellSize;
                        connectors.Add(new WallSegment(bx, Math.Min(wz, iz), bx, Math.Max(wz, iz), false));
                    }
                    // Right neighbor
                    if (gx + 1 <= w && hullInside[iy, gx + 1] && !hullInside[dir == WallDir.South ? iy + 1 : iy - 1, gx + 1]
                        && !IsWinCell(winSet, gx + 1, gy))
                    {
                        float bx = gx * cellSize;
                        connectors.Add(new WallSegment(bx, Math.Min(wz, iz), bx, Math.Max(wz, iz), false));
                    }
                }
                else // East or West
                {
                    int ix = dir == WallDir.East ? gx + 1 : gx - 1;
                    if (ix < 1 || ix > w) continue;
                    if (!hullInside[gy, ix]) continue;

                    float wx = dir == WallDir.East ? gx * cellSize : (gx - 1) * cellSize;
                    float ix2 = dir == WallDir.East ? wx - f : wx + f;

                    // Top neighbor
                    if (gy - 1 >= 1 && hullInside[gy - 1, ix] && !hullInside[gy - 1, dir == WallDir.East ? ix + 1 : ix - 1]
                        && !IsWinCell(winSet, gx, gy - 1))
                    {
                        float bz = (gy - 1) * cellSize;
                        connectors.Add(new WallSegment(Math.Min(wx, ix2), bz, Math.Max(wx, ix2), bz, false));
                    }
                    // Bottom neighbor
                    if (gy + 1 <= h && hullInside[gy + 1, ix] && !hullInside[gy + 1, dir == WallDir.East ? ix + 1 : ix - 1]
                        && !IsWinCell(winSet, gx, gy + 1))
                    {
                        float bz = gy * cellSize;
                        connectors.Add(new WallSegment(Math.Min(wx, ix2), bz, Math.Max(wx, ix2), bz, false));
                    }
                }
            }
        }
        return connectors;
    }

    public static bool IsWinCell(HashSet<(int gx, int gy, WallDir dir)> winSet, int gx, int gy)
    {
        return winSet.Contains((gx, gy, WallDir.North)) ||
               winSet.Contains((gx, gy, WallDir.South)) ||
               winSet.Contains((gx, gy, WallDir.East)) ||
               winSet.Contains((gx, gy, WallDir.West));
    }
}
