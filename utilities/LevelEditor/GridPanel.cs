namespace SpaceHulksLevelEditor;

public class GridPanel : Panel
{
    public FloorData? Floor { get; set; }
    public EditTool Tool { get; set; } = EditTool.Corridor;
    public EnemyType PlaceEnemyType { get; set; } = EnemyType.Lurker;
    public RoomType PlaceRoomType { get; set; } = RoomType.Bridge;
    public RoomType PlaceConsoleType { get; set; } = RoomType.Bridge;
    public OfficerRank PlaceOfficerRank { get; set; } = OfficerRank.Ensign;
    public EnemyType PlaceOfficerCombatType { get; set; } = EnemyType.Brute;
    public ShipType ShipType { get; set; } = ShipType.Human;
    public int PlaceStairsDir { get; set; }
    public int RoomBrushW { get; set; } = 3;
    public int RoomBrushH { get; set; } = 3;

    public UndoManager Undo { get; } = new();
    public event Action? DataChanged;

    private const int CellSize = 28;
    private const int BaseMargin = 30;
    private bool _painting;
    private int _hoverGX = -1, _hoverGY = -1;

    // Pan offset (middle mouse drag)
    private int _panX, _panY;
    private bool _middlePanning;
    private Point _panStart;

    private int Margin => BaseMargin + _panX;
    private int MarginY => BaseMargin + _panY;

    public void ResetPan() { _panX = 0; _panY = 0; Invalidate(); }

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
        [RoomType.Teleporter] = Color.FromArgb(180, 100, 220),
    };

    public GridPanel()
    {
        DoubleBuffered = true;
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true);
    }

    private (int gx, int gy) ScreenToGrid(int sx, int sy)
    {
        int gx = (sx - Margin) / CellSize + 1;
        int gy = (sy - MarginY) / CellSize + 1;
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
        g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;
        g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.Half;
        g.Clear(Color.FromArgb(20, 20, 25));

        TextureCache.EnsureLoaded();
        bool alien = ShipType == ShipType.Alien;

        int w = Floor.Width, h = Floor.Height;

        // Draw cells
        for (int gy = 1; gy <= h; gy++)
        {
            for (int gx = 1; gx <= w; gx++)
            {
                int px = Margin + (gx - 1) * CellSize;
                int py = MarginY + (gy - 1) * CellSize;
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

                // Draw texture background then color overlay
                Bitmap? cellTex = cell switch
                {
                    (int)CellType.Wall => alien ? TextureCache.Bricks : TextureCache.WallA,
                    (int)CellType.Window => alien ? TextureCache.Bricks : TextureCache.WallAWindow,
                    _ => alien ? TextureCache.Stone : TextureCache.Floor,
                };

                if (cellTex != null)
                {
                    g.DrawImage(cellTex, rect);
                    using var overlay = new SolidBrush(Color.FromArgb(140, bg));
                    g.FillRectangle(overlay, rect);
                }
                else
                {
                    using var brush = new SolidBrush(bg);
                    g.FillRectangle(brush, rect);
                }

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
            int py = MarginY + (room.Y - 1) * CellSize - 1;
            int rw = room.Width * CellSize + 1;
            int rh = room.Height * CellSize + 1;
            var rc = RoomColors.GetValueOrDefault(room.Type, Color.Gray);
            using var pen = new Pen(rc, 2);
            g.DrawRectangle(pen, px, py, rw, rh);

            // Room label
            string label = room.Type.ToString().ToUpper();
            if (room.SubsystemHpMax > 0)
                label += $" [{room.SubsystemHp}/{room.SubsystemHpMax}]";
            using var font = new Font("Consolas", 7);
            g.DrawString(label, font, Brushes.White, px + 3, py + 2);

            // Light indicator
            if (room.LightOn)
            {
                g.FillEllipse(Brushes.Yellow,
                    Margin + (room.CenterX - 1) * CellSize + CellSize / 2 - 3,
                    MarginY + (room.CenterY - 1) * CellSize + 1, 6, 6);
            }
        }

        // Draw entities
        DrawEntities(g);

        // Draw exterior hull perimeter
        DrawExteriorPerimeter(g, w, h);

        // Grid coords
        using var coordFont = new Font("Consolas", 7);
        for (int x = 1; x <= w; x++)
            g.DrawString(x.ToString(), coordFont, Brushes.DimGray, Margin + (x - 1) * CellSize + 2, 2);
        for (int y = 1; y <= h; y++)
            g.DrawString(y.ToString(), coordFont, Brushes.DimGray, 2, MarginY + (y - 1) * CellSize + 2);

        // Tool preview for room placement
        if (Tool == EditTool.Room && _hoverGX > 0 && _hoverGY > 0)
        {
            int px = Margin + (_hoverGX - 1) * CellSize;
            int py = MarginY + (_hoverGY - 1) * CellSize;
            using var hb = new SolidBrush(Color.FromArgb(60, 100, 200, 255));
            g.FillRectangle(hb, px, py, RoomBrushW * CellSize, RoomBrushH * CellSize);
        }
    }

    private void DrawEntities(Graphics g)
    {
        if (Floor == null) return;

        DrawMarker(g, Floor.SpawnGX, Floor.SpawnGY, Color.Lime, "S");

        if (Floor.HasUp)
            DrawMarker(g, Floor.StairsGX, Floor.StairsGY, Color.Green, "UP");
        if (Floor.HasDown)
            DrawMarker(g, Floor.DownGX, Floor.DownGY, Color.Blue, "DN");

        foreach (var c in Floor.Consoles)
        {
            var rc = RoomColors.GetValueOrDefault(c.RoomType, Color.Gray);
            DrawMarker(g, c.GX, c.GY, rc, "C");
        }

        foreach (var en in Floor.Enemies)
        {
            Color ec = en.EnemyType switch
            {
                EnemyType.Lurker => Color.FromArgb(100, 200, 100),
                EnemyType.Brute => Color.FromArgb(200, 80, 60),
                EnemyType.Spitter => Color.FromArgb(160, 80, 200),
                EnemyType.Hiveguard => Color.FromArgb(80, 120, 200),
                _ => Color.Gray,
            };
            DrawMarker(g, en.GX, en.GY, ec, en.EnemyType.ToString()[..1]);
        }

        foreach (var l in Floor.Loot)
            DrawMarker(g, l.GX, l.GY, Color.Gold, "$");

        foreach (var o in Floor.Officers)
            DrawMarker(g, o.GX, o.GY, Color.White, "O");

        foreach (var n in Floor.Npcs)
            DrawMarker(g, n.GX, n.GY, Color.Cyan, "N");
    }

    private static bool IsWallLike(int c) => c == (int)CellType.Wall || c == (int)CellType.Window;

    private void ExtendSide(bool[,] inside, int w, int h, int line, int from, int to, bool horiz, int inDir)
    {
        if (Floor == null) return;
        bool any = false;
        for (int i = from; i <= to; i++)
        {
            int gy = horiz ? line : i, gx = horiz ? i : line;
            if (gy >= 1 && gy <= h && gx >= 1 && gx <= w && inside[gy, gx]) { any = true; break; }
        }
        if (!any) return;
        bool facesIn = false;
        for (int i = from; i <= to; i++)
        {
            int gy = horiz ? line + inDir : i, gx = horiz ? i : line + inDir;
            if (gy >= 1 && gy <= h && gx >= 1 && gx <= w && inside[gy, gx]) { facesIn = true; break; }
        }
        if (!facesIn) return;
        for (int i = from; i <= to; i++)
        {
            int gy = horiz ? line : i, gx = horiz ? i : line;
            if (gy >= 1 && gy <= h && gx >= 1 && gx <= w && IsWallLike(Floor!.Map[gy, gx]))
                inside[gy, gx] = true;
        }
    }

    private void DrawExteriorPerimeter(Graphics g, int w, int h)
    {
        if (Floor == null) return;

        // Same algorithm as Preview3DPanel.BuildExterior
        bool[,] inside = new bool[h + 2, w + 2];
        var queue = new Queue<(int, int)>();

        // Seed from spawn + room centers
        int sx = Floor.SpawnGX, sy = Floor.SpawnGY;
        if (sx >= 1 && sx <= w && sy >= 1 && sy <= h && !IsWallLike(Floor.Map[sy, sx]))
        {
            inside[sy, sx] = true;
            queue.Enqueue((sy, sx));
        }
        foreach (var room in Floor.Rooms)
        {
            int rcx = room.CenterX, rcy = room.CenterY;
            if (rcx >= 1 && rcx <= w && rcy >= 1 && rcy <= h && !inside[rcy, rcx] && !IsWallLike(Floor.Map[rcy, rcx]))
            {
                inside[rcy, rcx] = true;
                queue.Enqueue((rcy, rcx));
            }
        }

        // Flood fill through open cells
        while (queue.Count > 0)
        {
            var (cy, cx) = queue.Dequeue();
            int[] dy = { -1, 1, 0, 0 }, dx = { 0, 0, -1, 1 };
            for (int d = 0; d < 4; d++)
            {
                int ny = cy + dy[d], nx = cx + dx[d];
                if (ny < 1 || ny > h || nx < 1 || nx > w) continue;
                if (inside[ny, nx]) continue;
                if (IsWallLike(Floor.Map[ny, nx])) continue;
                inside[ny, nx] = true;
                queue.Enqueue((ny, nx));
            }
        }

        // Expand one layer of walls (collect first to avoid cascade)
        var toExpand = new List<(int y, int x)>();
        for (int gy = 1; gy <= h; gy++)
            for (int gx = 1; gx <= w; gx++)
                if (IsWallLike(Floor.Map[gy, gx]) && !inside[gy, gx])
                    if ((gy > 1 && inside[gy - 1, gx]) || (gy < h && inside[gy + 1, gx]) ||
                        (gx > 1 && inside[gy, gx - 1]) || (gx < w && inside[gy, gx + 1]))
                        toExpand.Add((gy, gx));
        foreach (var (ey, ex) in toExpand)
            inside[ey, ex] = true;

        // Extend room sides that face interior
        foreach (var room in Floor.Rooms)
        {
            int rx = room.X, ry = room.Y, rw = room.Width, rh = room.Height;
            ExtendSide(inside, w, h, ry - 1, rx, rx + rw - 1, true, -1);
            ExtendSide(inside, w, h, ry + rh, rx, rx + rw - 1, true, +1);
            ExtendSide(inside, w, h, rx - 1, ry, ry + rh - 1, false, -1);
            ExtendSide(inside, w, h, rx + rw, ry, ry + rh - 1, false, +1);
        }

        // Draw perimeter lines where inside meets non-inside
        using var pen = new Pen(Color.FromArgb(200, 40, 120, 255), 2);
        for (int gy = 1; gy <= h; gy++)
        {
            for (int gx = 1; gx <= w; gx++)
            {
                if (!inside[gy, gx]) continue;
                int px = Margin + (gx - 1) * CellSize;
                int py = MarginY + (gy - 1) * CellSize;

                if (!inside[gy - 1, gx]) // north edge
                    g.DrawLine(pen, px, py, px + CellSize, py);
                if (!inside[gy + 1, gx]) // south edge
                    g.DrawLine(pen, px, py + CellSize, px + CellSize, py + CellSize);
                if (!inside[gy, gx - 1]) // west edge
                    g.DrawLine(pen, px, py, px, py + CellSize);
                if (!inside[gy, gx + 1]) // east edge
                    g.DrawLine(pen, px + CellSize, py, px + CellSize, py + CellSize);
            }
        }
    }

    private void DrawMarker(Graphics g, int gx, int gy, Color color, string label)
    {
        if (!InBounds(gx, gy)) return;
        int px = Margin + (gx - 1) * CellSize + 2;
        int py = MarginY + (gy - 1) * CellSize + 2;
        int sz = CellSize - 5;
        using var brush = new SolidBrush(Color.FromArgb(180, color));
        g.FillRectangle(brush, px, py, sz, sz);
        using var font = new Font("Consolas", 8, FontStyle.Bold);
        g.DrawString(label, font, Brushes.White, px + 1, py + 1);
    }

    // ── Input ────────────────────────────────────────────

    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (Floor == null) return;

        if (e.Button == MouseButtons.Middle)
        {
            _middlePanning = true;
            _panStart = e.Location;
            return;
        }

        // Save state for undo before any edit
        Undo.SaveState(Floor);

        var (gx, gy) = ScreenToGrid(e.X, e.Y);

        if (e.Button == MouseButtons.Right)
        {
            if (!InBounds(gx, gy)) return;

            // Check for room/entity context menu first
            var room = Floor.RoomAt(gx, gy);
            var enemy = Floor.Enemies.FirstOrDefault(en => en.GX == gx && en.GY == gy);
            var officer = Floor.Officers.FirstOrDefault(o => o.GX == gx && o.GY == gy);
            var npc = Floor.Npcs.FirstOrDefault(n => n.GX == gx && n.GY == gy);

            if (room != null || enemy != null || officer != null || npc != null)
            {
                ShowContextMenu(gx, gy, room, enemy, officer, npc, e.Location);
                return;
            }

            // Default: paint wall
            Floor.Map[gy, gx] = (int)CellType.Wall;
            Floor.Consoles.RemoveAll(c => c.GX == gx && c.GY == gy);
            Floor.Loot.RemoveAll(l => l.GX == gx && l.GY == gy);
            _painting = true;
            DataChanged?.Invoke();
            Invalidate();
            return;
        }

        _painting = true;
        ApplyTool(gx, gy);
    }

    private void ShowContextMenu(int gx, int gy, RoomData? room,
        EntityData? enemy, OfficerData? officer, NpcData? npc, Point screenPos)
    {
        var menu = new ContextMenuStrip();
        menu.BackColor = Color.FromArgb(40, 40, 50);
        menu.ForeColor = Color.White;

        if (room != null)
        {
            menu.Items.Add($"--- Room: {room.Type} ---").Enabled = false;

            menu.Items.Add(room.LightOn ? "Turn Light Off" : "Turn Light On", null, (_, _) =>
            {
                room.LightOn = !room.LightOn;
                DataChanged?.Invoke(); Invalidate();
            });

            var typeMenu = new ToolStripMenuItem("Change Type");
            foreach (RoomType rt in Enum.GetValues<RoomType>())
            {
                var r = rt;
                var item = typeMenu.DropDownItems.Add(rt.ToString(), null, (_, _) =>
                {
                    room.Type = r;
                    DataChanged?.Invoke(); Invalidate();
                });
                if (rt == room.Type)
                    item.Font = new Font(item.Font, FontStyle.Bold);
            }
            menu.Items.Add(typeMenu);

            menu.Items.Add($"Set Subsystem HP (now {room.SubsystemHp}/{room.SubsystemHpMax})...", null, (_, _) =>
            {
                string? input = ShowInputDialog("Subsystem HP (hp/max):", $"{room.SubsystemHp}/{room.SubsystemHpMax}");
                if (input != null)
                {
                    var parts = input.Split('/');
                    if (parts.Length == 2 && int.TryParse(parts[0].Trim(), out int hp) && int.TryParse(parts[1].Trim(), out int max))
                    {
                        room.SubsystemHp = hp;
                        room.SubsystemHpMax = max;
                        DataChanged?.Invoke(); Invalidate();
                    }
                }
            });

            menu.Items.Add("Delete Room", null, (_, _) =>
            {
                Floor!.Rooms.Remove(room);
                DataChanged?.Invoke(); Invalidate();
            });

            menu.Items.Add(new ToolStripSeparator());
        }

        if (enemy != null)
        {
            menu.Items.Add($"--- Enemy: {enemy.Name} ({enemy.EnemyType}) ---").Enabled = false;
            menu.Items.Add("Rename...", null, (_, _) =>
            {
                string? name = ShowInputDialog("Enemy name:", enemy.Name);
                if (name != null) { enemy.Name = name; DataChanged?.Invoke(); Invalidate(); }
            });
            menu.Items.Add("Delete Enemy", null, (_, _) =>
            {
                Floor!.Enemies.Remove(enemy);
                DataChanged?.Invoke(); Invalidate();
            });
            menu.Items.Add(new ToolStripSeparator());
        }

        if (officer != null)
        {
            menu.Items.Add($"--- Officer: {officer.Name} ({officer.Rank}) ---").Enabled = false;
            menu.Items.Add("Rename...", null, (_, _) =>
            {
                string? name = ShowInputDialog("Officer name:", officer.Name);
                if (name != null) { officer.Name = name; DataChanged?.Invoke(); Invalidate(); }
            });

            var rankMenu = new ToolStripMenuItem("Change Rank");
            foreach (OfficerRank r in Enum.GetValues<OfficerRank>())
            {
                var rank = r;
                rankMenu.DropDownItems.Add(r.ToString(), null, (_, _) =>
                {
                    officer.Rank = rank;
                    DataChanged?.Invoke(); Invalidate();
                });
            }
            menu.Items.Add(rankMenu);

            var combatMenu = new ToolStripMenuItem($"Combat Type ({officer.CombatType})");
            foreach (EnemyType et in Enum.GetValues<EnemyType>())
            {
                if (et == EnemyType.None) continue;
                var ct = et;
                combatMenu.DropDownItems.Add(et.ToString(), null, (_, _) =>
                {
                    officer.CombatType = ct;
                    DataChanged?.Invoke(); Invalidate();
                });
            }
            menu.Items.Add(combatMenu);

            menu.Items.Add("Delete Officer", null, (_, _) =>
            {
                Floor!.Officers.Remove(officer);
                DataChanged?.Invoke(); Invalidate();
            });
            menu.Items.Add(new ToolStripSeparator());
        }

        if (npc != null)
        {
            menu.Items.Add($"--- NPC: {npc.Name} (Dialog {npc.DialogId}) ---").Enabled = false;
            menu.Items.Add("Rename...", null, (_, _) =>
            {
                string? name = ShowInputDialog("NPC name:", npc.Name);
                if (name != null) { npc.Name = name; DataChanged?.Invoke(); Invalidate(); }
            });
            menu.Items.Add("Set Dialog ID...", null, (_, _) =>
            {
                string? val = ShowInputDialog("Dialog ID:", npc.DialogId.ToString());
                if (val != null && int.TryParse(val, out int id))
                {
                    npc.DialogId = id;
                    DataChanged?.Invoke(); Invalidate();
                }
            });
            menu.Items.Add("Delete NPC", null, (_, _) =>
            {
                Floor!.Npcs.Remove(npc);
                DataChanged?.Invoke(); Invalidate();
            });
        }

        // Remove trailing separator if present
        if (menu.Items.Count > 0 && menu.Items[^1] is ToolStripSeparator)
            menu.Items.RemoveAt(menu.Items.Count - 1);

        menu.Show(this, screenPos);
    }

    private static string? ShowInputDialog(string prompt, string defaultValue)
    {
        var form = new Form
        {
            Text = prompt, Width = 320, Height = 140,
            FormBorderStyle = FormBorderStyle.FixedDialog,
            StartPosition = FormStartPosition.CenterParent,
            BackColor = Color.FromArgb(30, 30, 35),
            ForeColor = Color.White,
            MaximizeBox = false, MinimizeBox = false,
        };
        var lbl = new Label { Text = prompt, Left = 10, Top = 10, Width = 280, ForeColor = Color.White };
        var txt = new TextBox
        {
            Left = 10, Top = 32, Width = 280, Text = defaultValue,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White,
        };
        var btnOk = new Button
        {
            Text = "OK", Left = 140, Top = 62, Width = 70, DialogResult = DialogResult.OK,
            FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(50, 50, 60), ForeColor = Color.White,
        };
        var btnCancel = new Button
        {
            Text = "Cancel", Left = 220, Top = 62, Width = 70, DialogResult = DialogResult.Cancel,
            FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(50, 50, 60), ForeColor = Color.White,
        };
        form.Controls.AddRange(new Control[] { lbl, txt, btnOk, btnCancel });
        form.AcceptButton = btnOk;
        form.CancelButton = btnCancel;
        return form.ShowDialog() == DialogResult.OK ? txt.Text : null;
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);

        if (_middlePanning)
        {
            _panX += e.X - _panStart.X;
            _panY += e.Y - _panStart.Y;
            _panStart = e.Location;
            Invalidate();
            return;
        }

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
        _middlePanning = false;
    }

    // ── Tool application ─────────────────────────────────

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

            case EditTool.Officer:
                if (Floor.Map[gy, gx] == (int)CellType.Open)
                {
                    Floor.Officers.RemoveAll(o => o.GX == gx && o.GY == gy);
                    int oIdx = Floor.Officers.Count;
                    Floor.Officers.Add(new OfficerData
                    {
                        GX = gx, GY = gy,
                        Rank = PlaceOfficerRank,
                        CombatType = PlaceOfficerCombatType,
                        Name = $"{PlaceOfficerRank}".ToUpper() + $" OFFICER{oIdx:D2}",
                    });
                }
                break;

            case EditTool.Npc:
                if (Floor.Map[gy, gx] == (int)CellType.Open)
                {
                    Floor.Npcs.RemoveAll(n => n.GX == gx && n.GY == gy);
                    int nIdx = Floor.Npcs.Count;
                    Floor.Npcs.Add(new NpcData
                    {
                        GX = gx, GY = gy,
                        Name = $"NPC{nIdx:D2}",
                        DialogId = nIdx,
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
                Floor.Officers.RemoveAll(o => o.GX == gx && o.GY == gy);
                Floor.Npcs.RemoveAll(n => n.GX == gx && n.GY == gy);
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
        if (Floor.RoomOverlaps(gx, gy, RoomBrushW, RoomBrushH))
            return;

        var room = new RoomData
        {
            X = gx, Y = gy, Width = RoomBrushW, Height = RoomBrushH,
            Type = PlaceRoomType, LightOn = true,
        };

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
    Corridor, Wall, Window, Room,
    Enemy, Console, Officer, Npc,
    Spawn, StairsUp, StairsDown, Loot, Eraser,
}
