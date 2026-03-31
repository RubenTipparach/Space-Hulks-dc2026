namespace SpaceHulksLevelEditor;

public class GridPanel : Panel
{
    public FloorData? Floor { get; set; }
    public EditTool Tool { get; set; } = EditTool.Corridor;
    public EnemyType PlaceEnemyType { get; set; } = EnemyType.Lurker;
    public RoomType PlaceRoomType { get; set; } = RoomType.Bridge;
    public RoomType PlaceConsoleType { get; set; } = RoomType.Bridge;
    public int PlaceStairsDir { get; set; }
    public int RoomBrushW { get; set; } = 3;
    public int RoomBrushH { get; set; } = 3;

    public event Action? DataChanged;

    private const int CellSize = 28;
    private new const int Margin = 30;
    private bool _painting;
    private int _hoverGX = -1, _hoverGY = -1;


    private static readonly Dictionary<RoomType, Color> RoomColors = new()
    {
        [RoomType.Corridor] = Color.FromArgb(80, 80, 80),
        [RoomType.Bridge] = Color.FromArgb(34, 204, 238),
        [RoomType.Medbay] = Color.FromArgb(68, 204, 68),
        [RoomType.Weapons] = Color.FromArgb(204, 136, 34),
        [RoomType.Engines] = Color.FromArgb(204, 170, 34),
        [RoomType.Reactor] = Color.FromArgb(60, 200, 200),
        [RoomType.Shields] = Color.FromArgb(50, 160, 140),
        [RoomType.Cargo] = Color.FromArgb(100, 140, 60),
        [RoomType.Barracks] = Color.FromArgb(140, 80, 80),
    };

    public GridPanel()
    {
        DoubleBuffered = true;
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true);
    }

    private (int gx, int gy) ScreenToGrid(int sx, int sy)
    {
        int gx = (sx - Margin) / CellSize + 1;
        int gy = (sy - Margin) / CellSize + 1;
        return (gx, gy);
    }

    private bool InBounds(int gx, int gy) =>
        Floor != null && gx >= 1 && gx <= Floor.Width && gy >= 1 && gy <= Floor.Height;

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        if (Floor == null) return;
        var g = e.Graphics;
        g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.None;
        g.Clear(Color.FromArgb(20, 20, 25));

        int w = Floor.Width, h = Floor.Height;

        // Draw cells
        for (int gy = 1; gy <= h; gy++)
        {
            for (int gx = 1; gx <= w; gx++)
            {
                int px = Margin + (gx - 1) * CellSize;
                int py = Margin + (gy - 1) * CellSize;
                var rect = new Rectangle(px, py, CellSize - 1, CellSize - 1);

                int cell = Floor.Map[gy, gx];
                Color bg;
                if (cell == (int)CellType.Wall)
                    bg = Color.FromArgb(40, 40, 50);
                else if (cell == (int)CellType.Window)
                    bg = Color.FromArgb(60, 80, 120);
                else
                    bg = Color.FromArgb(100, 100, 110);

                // Room tint
                var room = Floor.RoomAt(gx, gy);
                if (room != null && cell != (int)CellType.Wall)
                {
                    var rc = RoomColors.GetValueOrDefault(room.Type, Color.Gray);
                    bg = Color.FromArgb(
                        Math.Min(255, bg.R / 2 + rc.R / 3),
                        Math.Min(255, bg.G / 2 + rc.G / 3),
                        Math.Min(255, bg.B / 2 + rc.B / 3));
                }

                using var brush = new SolidBrush(bg);
                g.FillRectangle(brush, rect);

                // Hover highlight
                if (gx == _hoverGX && gy == _hoverGY)
                {
                    using var hb = new SolidBrush(Color.FromArgb(40, 255, 255, 255));
                    g.FillRectangle(hb, rect);
                }
            }
        }

        // Draw room outlines
        foreach (var room in Floor.Rooms)
        {
            int px = Margin + (room.X - 1) * CellSize - 1;
            int py = Margin + (room.Y - 1) * CellSize - 1;
            int rw = room.Width * CellSize + 1;
            int rh = room.Height * CellSize + 1;
            var rc = RoomColors.GetValueOrDefault(room.Type, Color.Gray);
            using var pen = new Pen(rc, 2);
            g.DrawRectangle(pen, px, py, rw, rh);

            // Room label
            string label = room.Type.ToString().ToUpper();
            using var font = new Font("Consolas", 7);
            g.DrawString(label, font, Brushes.White, px + 3, py + 2);

            // Light indicator
            if (room.LightOn)
            {
                g.FillEllipse(Brushes.Yellow,
                    Margin + (room.CenterX - 1) * CellSize + CellSize / 2 - 3,
                    Margin + (room.CenterY - 1) * CellSize + 1, 6, 6);
            }
        }

        // Draw entities
        DrawEntities(g);

        // Grid coords
        using var coordFont = new Font("Consolas", 7);
        for (int x = 1; x <= w; x++)
            g.DrawString(x.ToString(), coordFont, Brushes.DimGray, Margin + (x - 1) * CellSize + 2, 2);
        for (int y = 1; y <= h; y++)
            g.DrawString(y.ToString(), coordFont, Brushes.DimGray, 2, Margin + (y - 1) * CellSize + 2);

        // Tool preview for room placement
        if (Tool == EditTool.Room && _hoverGX > 0 && _hoverGY > 0)
        {
            int px = Margin + (_hoverGX - 1) * CellSize;
            int py = Margin + (_hoverGY - 1) * CellSize;
            using var hb = new SolidBrush(Color.FromArgb(60, 100, 200, 255));
            g.FillRectangle(hb, px, py, RoomBrushW * CellSize, RoomBrushH * CellSize);
        }
    }

    private void DrawEntities(Graphics g)
    {
        if (Floor == null) return;

        // Spawn
        DrawMarker(g, Floor.SpawnGX, Floor.SpawnGY, Color.Lime, "S");

        // Up-stairs
        if (Floor.HasUp)
            DrawMarker(g, Floor.StairsGX, Floor.StairsGY, Color.Green, "UP");

        // Down-stairs
        if (Floor.HasDown)
            DrawMarker(g, Floor.DownGX, Floor.DownGY, Color.Blue, "DN");

        // Consoles
        foreach (var c in Floor.Consoles)
        {
            var rc = RoomColors.GetValueOrDefault(c.RoomType, Color.Gray);
            DrawMarker(g, c.GX, c.GY, rc, "C");
        }

        // Enemies
        foreach (var e in Floor.Enemies)
        {
            Color ec = e.EnemyType switch
            {
                EnemyType.Lurker => Color.FromArgb(100, 200, 100),
                EnemyType.Brute => Color.FromArgb(200, 80, 60),
                EnemyType.Spitter => Color.FromArgb(160, 80, 200),
                EnemyType.Hiveguard => Color.FromArgb(80, 120, 200),
                _ => Color.Gray,
            };
            DrawMarker(g, e.GX, e.GY, ec, e.EnemyType.ToString()[..1]);
        }

        // Loot
        foreach (var l in Floor.Loot)
            DrawMarker(g, l.GX, l.GY, Color.Gold, "$");
    }

    private void DrawMarker(Graphics g, int gx, int gy, Color color, string label)
    {
        if (!InBounds(gx, gy)) return;
        int px = Margin + (gx - 1) * CellSize + 2;
        int py = Margin + (gy - 1) * CellSize + 2;
        int sz = CellSize - 5;
        using var brush = new SolidBrush(Color.FromArgb(180, color));
        g.FillRectangle(brush, px, py, sz, sz);
        using var font = new Font("Consolas", 8, FontStyle.Bold);
        g.DrawString(label, font, Brushes.White, px + 1, py + 1);
    }

    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (Floor == null) return;
        var (gx, gy) = ScreenToGrid(e.X, e.Y);

        if (e.Button == MouseButtons.Right)
        {
            // Right-click: always erase to wall
            if (InBounds(gx, gy))
            {
                Floor.Map[gy, gx] = (int)CellType.Wall;
                // Remove entities at this cell
                Floor.Enemies.RemoveAll(en => en.GX == gx && en.GY == gy);
                Floor.Consoles.RemoveAll(c => c.GX == gx && c.GY == gy);
                Floor.Loot.RemoveAll(l => l.GX == gx && l.GY == gy);
                _painting = true;
                DataChanged?.Invoke();
                Invalidate();
            }
            return;
        }

        _painting = true;
        ApplyTool(gx, gy);
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        var (gx, gy) = ScreenToGrid(e.X, e.Y);
        _hoverGX = gx; _hoverGY = gy;

        if (_painting && e.Button == MouseButtons.Left &&
            (Tool == EditTool.Corridor || Tool == EditTool.Wall || Tool == EditTool.Window))
        {
            ApplyTool(gx, gy);
        }
        else if (_painting && e.Button == MouseButtons.Right)
        {
            if (InBounds(gx, gy))
            {
                Floor!.Map[gy, gx] = (int)CellType.Wall;
                DataChanged?.Invoke();
            }
        }

        Invalidate();
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
        base.OnMouseUp(e);
        _painting = false;
    }

    private void ApplyTool(int gx, int gy)
    {
        if (Floor == null || !InBounds(gx, gy)) return;

        switch (Tool)
        {
            case EditTool.Corridor:
                Floor.Map[gy, gx] = (int)CellType.Open;
                break;

            case EditTool.Wall:
                Floor.Map[gy, gx] = (int)CellType.Wall;
                break;

            case EditTool.Window:
                Floor.Map[gy, gx] = (int)CellType.Window;
                break;

            case EditTool.Room:
                AddRoom(gx, gy);
                break;

            case EditTool.Enemy:
                if (Floor.Map[gy, gx] == (int)CellType.Open)
                {
                    // Remove existing enemy at cell
                    Floor.Enemies.RemoveAll(en => en.GX == gx && en.GY == gy);
                    Floor.Enemies.Add(new EntityData
                    {
                        GX = gx, GY = gy, EnemyType = PlaceEnemyType,
                        Name = $"{PlaceEnemyType}".ToUpper()
                    });
                }
                break;

            case EditTool.Console:
                if (Floor.Map[gy, gx] == (int)CellType.Open)
                {
                    Floor.Consoles.RemoveAll(c => c.GX == gx && c.GY == gy);
                    Floor.Consoles.Add(new ConsoleData
                    {
                        GX = gx, GY = gy, RoomType = PlaceConsoleType
                    });
                }
                break;

            case EditTool.Spawn:
                Floor.SpawnGX = gx;
                Floor.SpawnGY = gy;
                if (Floor.Map[gy, gx] == (int)CellType.Wall)
                    Floor.Map[gy, gx] = (int)CellType.Open;
                break;

            case EditTool.StairsUp:
                Floor.HasUp = true;
                Floor.StairsGX = gx;
                Floor.StairsGY = gy;
                Floor.StairsDir = PlaceStairsDir;
                if (Floor.Map[gy, gx] == (int)CellType.Wall)
                    Floor.Map[gy, gx] = (int)CellType.Open;
                break;

            case EditTool.StairsDown:
                Floor.HasDown = true;
                Floor.DownGX = gx;
                Floor.DownGY = gy;
                Floor.DownDir = PlaceStairsDir;
                if (Floor.Map[gy, gx] == (int)CellType.Wall)
                    Floor.Map[gy, gx] = (int)CellType.Open;
                break;

            case EditTool.Loot:
                if (Floor.Map[gy, gx] == (int)CellType.Open)
                {
                    Floor.Loot.RemoveAll(l => l.GX == gx && l.GY == gy);
                    Floor.Loot.Add(new LootData { GX = gx, GY = gy });
                }
                break;

            case EditTool.Eraser:
                Floor.Enemies.RemoveAll(en => en.GX == gx && en.GY == gy);
                Floor.Consoles.RemoveAll(c => c.GX == gx && c.GY == gy);
                Floor.Loot.RemoveAll(l => l.GX == gx && l.GY == gy);
                if (Floor.HasUp && Floor.StairsGX == gx && Floor.StairsGY == gy)
                    Floor.HasUp = false;
                if (Floor.HasDown && Floor.DownGX == gx && Floor.DownGY == gy)
                    Floor.HasDown = false;
                break;
        }

        DataChanged?.Invoke();
        Invalidate();
    }

    private void AddRoom(int gx, int gy)
    {
        if (Floor == null) return;

        // Check overlap with existing rooms (1-tile margin)
        if (Floor.RoomOverlaps(gx, gy, RoomBrushW, RoomBrushH))
            return;

        var room = new RoomData
        {
            X = gx, Y = gy, Width = RoomBrushW, Height = RoomBrushH,
            Type = PlaceRoomType, LightOn = true,
        };

        // Carve the room
        for (int py = room.Y; py < room.Y + room.Height; py++)
            for (int px = room.X; px < room.X + room.Width; px++)
                if (InBounds(px, py))
                    Floor.Map[py, px] = (int)CellType.Open;

        Floor.Rooms.Add(room);
    }

    public void DeleteRoomAt(int gx, int gy)
    {
        if (Floor == null) return;
        var room = Floor.RoomAt(gx, gy);
        if (room != null)
        {
            Floor.Rooms.Remove(room);
            DataChanged?.Invoke();
            Invalidate();
        }
    }
}

public enum EditTool
{
    Corridor,
    Wall,
    Window,
    Room,
    Enemy,
    Console,
    Spawn,
    StairsUp,
    StairsDown,
    Loot,
    Eraser,
}
