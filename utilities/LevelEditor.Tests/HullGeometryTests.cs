using SpaceHulksLevelEditor;
using Xunit;

namespace LevelEditor.Tests;

public class HullGeometryTests
{
    private const float Cell = 2f;

    /// <summary>
    /// Build a hull mask and window set for testing.
    /// Grid is (h+2) x (w+2) with 1-based indexing, border cells are false.
    /// </summary>
    private static (bool[,] hull, HashSet<(int, int, WallDir)> winSet) MakeGrid(
        int w, int h, int[,] insideCells, (int gx, int gy, WallDir dir)[]? windows = null)
    {
        var hull = new bool[h + 2, w + 2]; // border stays false
        for (int y = 0; y < insideCells.GetLength(0); y++)
            for (int x = 0; x < insideCells.GetLength(1); x++)
                if (insideCells[y, x] == 1)
                    hull[y + 1, x + 1] = true;

        var winSet = new HashSet<(int, int, WallDir)>();
        if (windows != null)
            foreach (var w2 in windows)
                winSet.Add(w2);

        return (hull, winSet);
    }

    // ─── Basic exterior wall tests ───────────────────────────────

    [Fact]
    public void SingleCell_NoWindow_NoChamfer_EmitsFourWalls()
    {
        // Single hull cell at (1,1), surrounded by outside
        var (hull, winSet) = MakeGrid(1, 1, new[,] { { 1 } });

        var walls = HullGeometry.ComputeExteriorWalls(hull, winSet, 1, 1, Cell, 0, 0);

        Assert.Equal(4, walls.Count);
        // All walls at cell boundaries (no inset, no chamfer)
        Assert.All(walls, w => Assert.False(w.IsWindow));
    }

    [Fact]
    public void SingleCell_WithInset_WallsAreInset()
    {
        var (hull, winSet) = MakeGrid(1, 1, new[,] { { 1 } });
        float f = 0.5f;

        var walls = HullGeometry.ComputeExteriorWalls(hull, winSet, 1, 1, Cell, 0, f);

        // North wall should be at z = 0 + f = 0.5
        var north = walls.First(w => w.Z0 == w.Z1 && w.Z0 < Cell / 2);
        Assert.Equal(f, north.Z0, 3);
    }

    [Fact]
    public void SingleCell_WithChamfer_EmitsEightSegments()
    {
        // 4 walls + 4 chamfer diagonals
        var (hull, winSet) = MakeGrid(1, 1, new[,] { { 1 } });

        var walls = HullGeometry.ComputeExteriorWalls(hull, winSet, 1, 1, Cell, 0.5f, 0);

        Assert.Equal(8, walls.Count); // 4 walls + 4 chamfer diags
    }

    // ─── Window face tests ───────────────────────────────────────

    [Fact]
    public void WindowFace_IsFlush_NoInset()
    {
        // Single inside cell at (1,1). Window wall cell at (1,0) facing South.
        var hull = new bool[3, 3];
        hull[1, 1] = true;
        var winSet = new HashSet<(int, int, WallDir)> { (1, 0, WallDir.South) };

        var walls = HullGeometry.ComputeExteriorWalls(hull, winSet, 1, 1, Cell, 0, 1.0f);

        // With f=1.0 and Cell=2.0, use a smaller inset to avoid overlap
        // Actually, just verify the wall exists and is flush
        Assert.NotEmpty(walls);
        // The north wall (window) should be at z=0, south at z=2-f=1
        // But with f=1 all sides, ex0=1, ex1=1, so N/S walls have zero width -> skipped
        // Use a smaller f instead:
        walls = HullGeometry.ComputeExteriorWalls(hull, winSet, 1, 1, Cell, 0, 0.5f);
        var north = walls.Where(w => Math.Abs(w.Z0 - w.Z1) < 0.01f).OrderBy(w => w.Z0).First();
        // Window face should be flush at z=0 (not inset to 0.5)
        Assert.Equal(0f, north.Z0, 3);
        Assert.True(north.IsWindow);
    }

    [Fact]
    public void WindowFace_UsesWindowTexture()
    {
        var hull = new bool[3, 3];
        hull[1, 1] = true;
        var winSet = new HashSet<(int, int, WallDir)> { (1, 0, WallDir.South) };

        var walls = HullGeometry.ComputeExteriorWalls(hull, winSet, 1, 1, Cell, 0, 0);

        var north = walls.First(w => w.Z0 == w.Z1 && w.Z0 < Cell);
        Assert.True(north.IsWindow);
    }

    [Fact]
    public void NonWindowFace_UsesNormalTexture()
    {
        var hull = new bool[5, 5];
        hull[2, 2] = true;
        var winSet = new HashSet<(int, int, WallDir)>();

        var walls = HullGeometry.ComputeExteriorWalls(hull, winSet, 2, 2, Cell, 0, 0);

        Assert.All(walls, w => Assert.False(w.IsWindow));
    }

    // ─── Window + adjacent cell alignment ────────────────────────

    [Fact]
    public void CellAdjacentToWindow_NorthFaceFlush()
    {
        // Layout (5x5 hull array, 3x3 grid):
        //   Row 1: [ ] [W] [ ]   W = window cell (2,1) with WallDir.South
        //   Row 2: [ ] [H] [H]   H = hull inside
        //   Row 3: [ ] [ ] [ ]
        // Cell (3,2): oN=true (north neighbor (3,1) is outside), oW=false (west neighbor (2,2) is inside)
        // Diagonal NW = (2,1) is a window cell -> fN should be 0
        var hull = new bool[5, 5];
        hull[2, 2] = true; // cell (2,2) inside
        hull[2, 3] = true; // cell (3,2) inside
        var winSet = new HashSet<(int, int, WallDir)> { (2, 1, WallDir.South) };
        float f = 0.5f; // realistic fractional inset

        var walls = HullGeometry.ComputeExteriorWalls(hull, winSet, 3, 2, Cell, 0, f);

        // North face of cell (3,2): z0 = (2-1)*Cell = 2.0
        // Should be flush at 2.0, NOT inset to 2.5
        float z0 = (2 - 1) * Cell;
        var northWalls = walls.Where(w => Math.Abs(w.Z0 - w.Z1) < 0.01f && w.Z0 < z0 + f).ToList();
        Assert.NotEmpty(northWalls);
        var north = northWalls.OrderBy(w => w.Z0).First();
        Assert.Equal(z0, north.Z0, 3); // flush, not z0 + f
    }

    [Fact]
    public void CellAdjacentToWindow_OtherSide_NorthFaceFlush()
    {
        // Window at (2,1) with South face. Inside cells at (2,2) and (1,2).
        // Cell (1,2) has oN=true. Window cell (2,1) is diagonal NE -> fN should be 0
        var hull = new bool[5, 5];
        hull[2, 2] = true;
        hull[2, 1] = true; // cell (1,2) inside
        var winSet = new HashSet<(int, int, WallDir)> { (2, 1, WallDir.South) };
        float f = 0.5f;

        var walls = HullGeometry.ComputeExteriorWalls(hull, winSet, 1, 2, Cell, 0, f);

        float z0 = (2 - 1) * Cell;
        var northWalls = walls.Where(w => Math.Abs(w.Z0 - w.Z1) < 0.01f && w.Z0 < z0 + f).ToList();
        Assert.NotEmpty(northWalls);
        var north = northWalls.OrderBy(w => w.Z0).First();
        Assert.Equal(z0, north.Z0, 3);
    }

    [Fact]
    public void CellAdjacentToWindow_SideWallExtendsFull()
    {
        // The east wall of cell (2,2) should extend from z0 (flush) to z1
        // because the window is at diagonal NE=(3,1), fN=0
        // So the east wall should start at z0, not z0+f
        var hull = new bool[5, 5];
        hull[2, 2] = true; // cell with window on north
        hull[2, 3] = true; // adjacent cell to east
        var winSet = new HashSet<(int, int, WallDir)> { (2, 1, WallDir.South) };
        float f = 0.5f;

        var walls = HullGeometry.ComputeExteriorWalls(hull, winSet, 2, 2, Cell, 0, f);

        // The window face (north) should be at z=2.0 (flush)
        var north = walls.Where(w => Math.Abs(w.Z0 - w.Z1) < 0.01f).OrderBy(w => w.Z0).First();
        Assert.Equal((2 - 1) * Cell, north.Z0, 3);
        Assert.True(north.IsWindow);
    }

    // ─── Convex corner suppression at windows ────────────────────

    [Fact]
    public void ConvexCorner_SuppressedAtWindow_NoNWChaumferDiagonal()
    {
        // Single cell at (1,1), all sides exposed -> 4 convex corners normally.
        // Window at (0,0) diagonal NW -> NW corner suppressed, other 3 remain.
        var hull = new bool[3, 3];
        hull[1, 1] = true;
        var winSet = new HashSet<(int, int, WallDir)> { (0, 0, WallDir.South) };

        var walls = HullGeometry.ComputeExteriorWalls(hull, winSet, 1, 1, Cell, 0.5f, 0);

        var diagonals = walls.Where(w => Math.Abs(w.X0 - w.X1) > 0.01f && Math.Abs(w.Z0 - w.Z1) > 0.01f).ToList();
        Assert.Equal(3, diagonals.Count); // NE, SW, SE remain - NW suppressed
    }

    [Fact]
    public void ConvexCorner_NotSuppressed_WhenNoDiagonalWindow()
    {
        var hull = new bool[5, 5];
        hull[2, 3] = true;
        var winSet = new HashSet<(int, int, WallDir)>(); // no windows

        var walls = HullGeometry.ComputeExteriorWalls(hull, winSet, 3, 2, Cell, 0.5f, 0);

        // Should have chamfer diagonals at all 4 corners
        var diagonals = walls.Where(w => Math.Abs(w.X0 - w.X1) > 0.01f && Math.Abs(w.Z0 - w.Z1) > 0.01f).ToList();
        Assert.Equal(4, diagonals.Count);
    }

    // ─── Continuous wall alignment ───────────────────────────────

    // ─── Window connector tests ────────────────────────────────

    [Fact]
    public void WindowConnector_FillsGapBetweenFlushAndInset()
    {
        // Window at (2,1) facing South. Inside cells at (2,2) and (3,2).
        // Cell (3,2) is adjacent - its west wall doesn't exist (neighbor (2,2) is inside).
        // But the window face at (2,2) is flush at z=2.0, while (3,2)'s north face is inset.
        // A connector wall should fill the gap at x=4.0 (boundary between cells 2 and 3)
        // from z=2.0 (flush) to z=2.5 (inset).
        var hull = new bool[5, 5];
        hull[2, 2] = true;
        hull[2, 3] = true;
        var winSet = new HashSet<(int, int, WallDir)> { (2, 1, WallDir.South) };
        float f = 0.5f;

        var connectors = HullGeometry.ComputeWindowConnectors(hull, winSet, 3, 3, Cell, f);

        // Should have a connector at x=2.0 (right side of window cell gx=2 -> gx*Cell=4? no, (gx-1)*Cell=2, gx*Cell=4)
        // Actually window cell is (2,1). gx=2, so right boundary = 2*Cell = 4.0
        // The connector should be a vertical segment (same X, different Z)
        Assert.NotEmpty(connectors);
        var conn = connectors.First(c => Math.Abs(c.X0 - c.X1) < 0.01f); // vertical wall
        Assert.Equal(f, Math.Abs(conn.Z1 - conn.Z0), 3); // spans the inset gap
    }

    [Fact]
    public void WindowConnector_EmitsWhenNeighborInsideHull()
    {
        // Realistic layout: hull expanded through walls.
        // Window wall cell (3,2) facing South. Inside cells: (2,2), (3,2), (4,2), (3,3), (2,3), (4,3)
        // The wall cell (3,2) is hull-inside due to expansion.
        // Inside cell (3,3) renders the window. Adjacent (2,3) and (4,3) are also inside.
        // Window's left wall neighbor (2,2) IS hull-inside.
        var hull = new bool[6, 6]; // 4x4 grid
        hull[2, 2] = true; hull[2, 3] = true; hull[2, 4] = true;
        hull[3, 2] = true; hull[3, 3] = true; hull[3, 4] = true;
        var winSet = new HashSet<(int, int, WallDir)> { (3, 2, WallDir.South) };
        float f = 0.5f;

        var connectors = HullGeometry.ComputeWindowConnectors(hull, winSet, 4, 4, Cell, f);

        // Should generate connectors to bridge the gap
        Assert.NotEmpty(connectors);
        // Each connector should span exactly f
        Assert.All(connectors, c =>
            Assert.Equal(f, Math.Max(Math.Abs(c.Z1 - c.Z0), Math.Abs(c.X1 - c.X0)), 3));
    }

    [Fact]
    public void WindowConnector_NoConnectorWhenNoInset()
    {
        var hull = new bool[5, 5];
        hull[2, 2] = true;
        hull[2, 3] = true;
        var winSet = new HashSet<(int, int, WallDir)> { (2, 1, WallDir.South) };

        var connectors = HullGeometry.ComputeWindowConnectors(hull, winSet, 3, 3, Cell, 0);

        Assert.Empty(connectors);
    }

    [Fact]
    public void WindowCell_And_AdjacentCell_NorthWallsAlign()
    {
        // Two adjacent inside cells. Left one has window on north, right one doesn't.
        // Both north walls should be at same Z (flush).
        var hull = new bool[5, 5];
        hull[2, 2] = true; // has window north
        hull[2, 3] = true; // adjacent, no window
        var winSet = new HashSet<(int, int, WallDir)> { (2, 1, WallDir.North) };

        var wallsLeft = HullGeometry.ComputeExteriorWalls(hull, winSet, 2, 2, Cell, 0, 1.0f);
        var wallsRight = HullGeometry.ComputeExteriorWalls(hull, winSet, 3, 2, Cell, 0, 1.0f);

        float z0 = (2 - 1) * Cell;
        var northLeft = wallsLeft.First(w => w.Z0 == w.Z1 && w.Z0 <= z0 + 0.01f);
        var northRight = wallsRight.First(w => w.Z0 == w.Z1 && w.Z0 <= z0 + 1.01f);

        // Both should be at z0 (flush), not z0 + f
        Assert.Equal(z0, northLeft.Z0, 3);
        Assert.Equal(z0, northRight.Z0, 3);
    }
}
