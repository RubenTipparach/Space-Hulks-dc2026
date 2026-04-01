using System.Drawing.Drawing2D;

namespace SpaceHulksLevelEditor;

public class Preview3DPanel : Panel
{
    public FloorData? Floor { get; set; }
    public bool ShowExterior { get; set; }
    public ShipType ShipType { get; set; } = ShipType.Human;

    // Orbit camera
    private float _orbitYaw = 45f;    // degrees
    private float _orbitPitch = 55f;  // degrees (angle from horizontal)
    private float _orbitDist = 30f;   // distance from target
    private float _targetX, _targetZ; // look-at point (world coords)
    private float _targetY = 0f;

    private const float CellSize = 2.0f;
    private const float WallH = 2.0f;
    private const float MinPitch = 10f;
    private const float MaxPitch = 89f;
    private const float MinDist = 5f;
    private const float MaxDist = 80f;

    // Render
    private Bitmap? _buffer;
    private float[] _zBuf = Array.Empty<float>();
    private System.Windows.Forms.Timer _tickTimer;

    // Input
    private bool _orbiting, _panning;
    private Point _lastMouse;

    // Colors
    private static readonly Color BgColor = Color.FromArgb(18, 18, 28);
    private static readonly Color WallTop = Color.FromArgb(160, 155, 165);
    private static readonly Color FloorCol = Color.FromArgb(130, 130, 125);
    private static readonly Color CeilCol = Color.FromArgb(145, 145, 155);
    private static readonly Color GridLine = Color.FromArgb(40, 40, 50);

    private static readonly Dictionary<RoomType, Color> RoomColors = new()
    {
        [RoomType.Corridor] = Color.FromArgb(130, 130, 125),
        [RoomType.Bridge] = Color.FromArgb(80, 160, 180),
        [RoomType.Medbay] = Color.FromArgb(80, 160, 90),
        [RoomType.Weapons] = Color.FromArgb(180, 130, 70),
        [RoomType.Engines] = Color.FromArgb(170, 155, 70),
        [RoomType.Reactor] = Color.FromArgb(70, 165, 165),
        [RoomType.Shields] = Color.FromArgb(65, 140, 125),
        [RoomType.Cargo] = Color.FromArgb(110, 135, 80),
        [RoomType.Barracks] = Color.FromArgb(145, 95, 95),
        [RoomType.Teleporter] = Color.FromArgb(160, 90, 200),
    };

    private static readonly Dictionary<EnemyType, Color> EnemyColors = new()
    {
        [EnemyType.Lurker] = Color.FromArgb(80, 220, 80),
        [EnemyType.Brute] = Color.FromArgb(220, 70, 50),
        [EnemyType.Spitter] = Color.FromArgb(170, 70, 210),
        [EnemyType.Hiveguard] = Color.FromArgb(70, 110, 210),
    };

    // Render quad with optional texture
    private record struct RenderQuad(
        PointF[] Pts, Color Color, float Depth,
        Bitmap? Tex = null, bool IsVertical = false,
        float Brightness = 1f, Color Tint = default);

    public Preview3DPanel()
    {
        DoubleBuffered = true;
        SetStyle(ControlStyles.Selectable | ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true);
        TabStop = true;

        _tickTimer = new System.Windows.Forms.Timer { Interval = 33 }; // ~30fps
        _tickTimer.Tick += (_, _) => { RenderFrame(); Invalidate(); };
    }

    public void StartPreview()
    {
        if (Floor == null) return;
        TextureCache.EnsureLoaded();
        _targetX = Floor.Width * CellSize / 2f;
        _targetZ = Floor.Height * CellSize / 2f;
        _orbitDist = Math.Max(Floor.Width, Floor.Height) * CellSize * 0.8f;
        _tickTimer.Start();
        Focus();
    }

    public void StopPreview() => _tickTimer.Stop();

    private static bool IsWallLike(int cellType) =>
        cellType == (int)CellType.Wall || cellType == (int)CellType.Window;

    // ── Camera math ─────────────────────────────────────────────
    private (float x, float y, float z) GetCameraPos()
    {
        float yr = _orbitYaw * MathF.PI / 180f;
        float pr = _orbitPitch * MathF.PI / 180f;
        float cp = MathF.Cos(pr);
        return (
            _targetX + _orbitDist * cp * MathF.Sin(yr),
            _targetY + _orbitDist * MathF.Sin(pr),
            _targetZ + _orbitDist * cp * MathF.Cos(yr)
        );
    }

    // Project world point to screen (returns screen x, y, depth)
    private (float sx, float sy, float depth) Project(float wx, float wy, float wz,
        float camX, float camY, float camZ, int W, int H)
    {
        float yr = _orbitYaw * MathF.PI / 180f;
        float pr = _orbitPitch * MathF.PI / 180f;

        // View-space transform (orbit camera)
        float dx = wx - camX, dy = wy - camY, dz = wz - camZ;

        // Rotate around Y (yaw)
        float sy2 = MathF.Sin(yr), cy2 = MathF.Cos(yr);
        float rx = dx * cy2 - dz * sy2;
        float rz = dx * sy2 + dz * cy2;
        float ry = dy;

        // Rotate around X (pitch)
        float sp = MathF.Sin(pr), cp = MathF.Cos(pr);
        float ry2 = ry * cp - rz * sp;
        float rz2 = ry * sp + rz * cp;

        // Perspective
        float near = 0.5f;
        float depth = -rz2;
        if (depth < near) depth = near;

        float fov = 50f * MathF.PI / 180f;
        float scale = (H * 0.5f) / MathF.Tan(fov / 2f);
        float screenX = W / 2f + rx * scale / depth;
        float screenY = H / 2f - ry2 * scale / depth;

        return (screenX, screenY, depth);
    }

    private void RenderFrame()
    {
        if (Floor == null) return;
        int W = Width, H = Height;
        if (W < 10 || H < 10) return;

        if (_buffer == null || _buffer.Width != W || _buffer.Height != H)
        {
            _buffer?.Dispose();
            _buffer = new Bitmap(W, H, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            _zBuf = new float[W * H];
        }

        Array.Fill(_zBuf, float.MaxValue);

        var (camX, camY, camZ) = GetCameraPos();

        using var g = Graphics.FromImage(_buffer);
        g.SmoothingMode = SmoothingMode.None;
        g.InterpolationMode = InterpolationMode.NearestNeighbor;
        g.PixelOffsetMode = PixelOffsetMode.Half;
        g.Clear(BgColor);

        // Collect quads, sort by depth (painter's algorithm)
        var quads = new List<RenderQuad>();

        int w = Floor.Width, h = Floor.Height;

        for (int gy = 1; gy <= h; gy++)
        {
            for (int gx = 1; gx <= w; gx++)
            {
                float x0 = (gx - 1) * CellSize;
                float x1 = gx * CellSize;
                float z0 = (gy - 1) * CellSize;
                float z1 = gy * CellSize;

                int cellType = Floor.Map[gy, gx];

                if (IsWallLike(cellType))
                {
                    bool isWindow = cellType == (int)CellType.Window;

                    // Wall/Window block — draw top face + visible side faces
                    var room = Floor.RoomAt(gx, gy);
                    Color wallCol = WallTop;
                    Color roomTint = default;
                    if (room != null && RoomColors.ContainsKey(room.Type))
                    {
                        wallCol = Lerp(WallTop, RoomColors[room.Type], 0.3f);
                        roomTint = RoomColors[room.Type];
                    }

                    Bitmap? topTex = TextureCache.WallA;
                    Bitmap? sideTex = isWindow ? TextureCache.WallAWindow : TextureCache.WallA;

                    // Top face
                    var topPts = ProjectQuad(x0, WallH, z0, x1, WallH, z0, x1, WallH, z1, x0, WallH, z1,
                        camX, camY, camZ, W, H, out float topD);
                    if (topPts != null)
                        quads.Add(new RenderQuad(topPts, wallCol, topD, topTex, false, 1f, roomTint));

                    // Side faces (only draw if adjacent cell is not wall-like)
                    Color sideCol = Darken(wallCol, 0.6f);
                    Color sideCol2 = Darken(wallCol, 0.75f);

                    // North
                    if (gy > 1 && !IsWallLike(Floor.Map[gy - 1, gx]))
                    {
                        var pts = ProjectQuad(x0, 0, z0, x1, 0, z0, x1, WallH, z0, x0, WallH, z0,
                            camX, camY, camZ, W, H, out float d);
                        if (pts != null) quads.Add(new RenderQuad(pts, sideCol, d, sideTex, true, 0.6f, roomTint));
                    }
                    // South
                    if (gy < h && !IsWallLike(Floor.Map[gy + 1, gx]))
                    {
                        var pts = ProjectQuad(x1, 0, z1, x0, 0, z1, x0, WallH, z1, x1, WallH, z1,
                            camX, camY, camZ, W, H, out float d);
                        if (pts != null) quads.Add(new RenderQuad(pts, sideCol, d, sideTex, true, 0.6f, roomTint));
                    }
                    // West
                    if (gx > 1 && !IsWallLike(Floor.Map[gy, gx - 1]))
                    {
                        var pts = ProjectQuad(x0, 0, z1, x0, 0, z0, x0, WallH, z0, x0, WallH, z1,
                            camX, camY, camZ, W, H, out float d);
                        if (pts != null) quads.Add(new RenderQuad(pts, sideCol2, d, sideTex, true, 0.75f, roomTint));
                    }
                    // East
                    if (gx < w && !IsWallLike(Floor.Map[gy, gx + 1]))
                    {
                        var pts = ProjectQuad(x1, 0, z0, x1, 0, z1, x1, WallH, z1, x1, WallH, z0,
                            camX, camY, camZ, W, H, out float d);
                        if (pts != null) quads.Add(new RenderQuad(pts, sideCol2, d, sideTex, true, 0.75f, roomTint));
                    }
                }
                else
                {
                    // Open cell — floor
                    var room = Floor.RoomAt(gx, gy);
                    Color fc = FloorCol;
                    Color roomTint = default;
                    if (room != null && RoomColors.ContainsKey(room.Type))
                    {
                        fc = Lerp(FloorCol, RoomColors[room.Type], 0.25f);
                        roomTint = RoomColors[room.Type];
                    }

                    var floorPts = ProjectQuad(x0, 0, z0, x1, 0, z0, x1, 0, z1, x0, 0, z1,
                        camX, camY, camZ, W, H, out float fd);
                    if (floorPts != null)
                        quads.Add(new RenderQuad(floorPts, fc, fd, TextureCache.Floor, false, 1f, roomTint));
                }
            }
        }

        // Exterior hull (outward-facing boundary quads)
        if (ShowExterior)
            AddExteriorQuads(quads, camX, camY, camZ, W, H);

        // Entity markers (small raised quads)
        AddEntityQuads(quads, camX, camY, camZ, W, H);

        // Sort far to near (painter's)
        quads.Sort((a, b) => b.Depth.CompareTo(a.Depth));

        // Draw
        foreach (var q in quads)
        {
            if (q.Tex != null)
            {
                DrawTexturedQuad(g, q);
            }
            else
            {
                using var brush = new SolidBrush(q.Color);
                g.FillPolygon(brush, q.Pts);
            }

            using var pen = new Pen(Darken(q.Color, 0.7f), 1);
            g.DrawPolygon(pen, q.Pts);
        }
    }

    private void DrawTexturedQuad(Graphics g, RenderQuad q)
    {
        // Map texture to quad screen coordinates via affine transform
        // Vertical faces (wall sides): pts[3]=topLeft, pts[2]=topRight, pts[0]=bottomLeft
        // Horizontal faces (floor/ceiling/top): pts[0]=corner0, pts[1]=corner1, pts[3]=corner3
        PointF[] mapPts = q.IsVertical
            ? new[] { q.Pts[3], q.Pts[2], q.Pts[0] }
            : new[] { q.Pts[0], q.Pts[1], q.Pts[3] };

        try
        {
            using var mat = new Matrix(
                new RectangleF(0, 0, q.Tex!.Width, q.Tex.Height), mapPts);
            using var texBrush = new TextureBrush(q.Tex, WrapMode.Clamp);
            texBrush.Transform = mat;
            g.FillPolygon(texBrush, q.Pts);
        }
        catch
        {
            // Degenerate affine transform (tiny quad), fall back to solid
            using var fb = new SolidBrush(q.Color);
            g.FillPolygon(fb, q.Pts);
            return;
        }

        // Darken overlay for side faces
        if (q.Brightness < 0.99f)
        {
            int alpha = (int)((1f - q.Brightness) * 180);
            using var dark = new SolidBrush(Color.FromArgb(alpha, 0, 0, 0));
            g.FillPolygon(dark, q.Pts);
        }

        // Room tint overlay
        if (q.Tint.A > 0)
        {
            using var tint = new SolidBrush(Color.FromArgb(50, q.Tint));
            g.FillPolygon(tint, q.Pts);
        }
    }

    private void AddExteriorQuads(List<RenderQuad> quads,
        float camX, float camY, float camZ, int W, int H)
    {
        if (Floor == null) return;
        int w = Floor.Width, h = Floor.Height;

        bool alien = ShipType == ShipType.Alien;
        Bitmap? extWall = alien ? TextureCache.AlienExterior : TextureCache.ExteriorWall;
        Bitmap? extWindow = alien ? TextureCache.AlienExteriorWindow : TextureCache.ExteriorWindow;
        Color extCol = alien ? Color.FromArgb(120, 60, 80) : Color.FromArgb(60, 80, 130);
        Color extSide = Darken(extCol, 0.7f);

        // Side panels on grid boundaries
        for (int gy = 1; gy <= h; gy++)
        {
            for (int gx = 1; gx <= w; gx++)
            {
                if (!IsWallLike(Floor.Map[gy, gx])) continue;

                bool isWindow = Floor.Map[gy, gx] == (int)CellType.Window;
                Bitmap? faceTex = isWindow ? extWindow : extWall;

                float x0 = (gx - 1) * CellSize;
                float x1 = gx * CellSize;
                float z0 = (gy - 1) * CellSize;
                float z1 = gy * CellSize;

                // North boundary
                if (gy == 1)
                {
                    var pts = ProjectQuad(x1, 0, z0, x0, 0, z0, x0, WallH, z0, x1, WallH, z0,
                        camX, camY, camZ, W, H, out float d);
                    if (pts != null) quads.Add(new RenderQuad(pts, extSide, d, faceTex, true, 0.65f));
                }
                // South boundary
                if (gy == h)
                {
                    var pts = ProjectQuad(x0, 0, z1, x1, 0, z1, x1, WallH, z1, x0, WallH, z1,
                        camX, camY, camZ, W, H, out float d);
                    if (pts != null) quads.Add(new RenderQuad(pts, extSide, d, faceTex, true, 0.65f));
                }
                // West boundary
                if (gx == 1)
                {
                    var pts = ProjectQuad(x0, 0, z0, x0, 0, z1, x0, WallH, z1, x0, WallH, z0,
                        camX, camY, camZ, W, H, out float d);
                    if (pts != null) quads.Add(new RenderQuad(pts, extCol, d, faceTex, true, 0.75f));
                }
                // East boundary
                if (gx == w)
                {
                    var pts = ProjectQuad(x1, 0, z1, x1, 0, z0, x1, WallH, z0, x1, WallH, z1,
                        camX, camY, camZ, W, H, out float d);
                    if (pts != null) quads.Add(new RenderQuad(pts, extCol, d, faceTex, true, 0.75f));
                }
            }
        }

        // Roof cap (top of hull)
        float roofY = WallH + 0.05f;
        for (int gy = 1; gy <= h; gy++)
        {
            for (int gx = 1; gx <= w; gx++)
            {
                if (!IsWallLike(Floor.Map[gy, gx])) continue;
                float x0 = (gx - 1) * CellSize, x1 = gx * CellSize;
                float z0 = (gy - 1) * CellSize, z1 = gy * CellSize;
                var pts = ProjectQuad(x0, roofY, z1, x1, roofY, z1, x1, roofY, z0, x0, roofY, z0,
                    camX, camY, camZ, W, H, out float d);
                if (pts != null) quads.Add(new RenderQuad(pts, extCol, d, extWall, false, 0.85f));
            }
        }

        // Floor cap (underside of hull)
        float floorY = -0.05f;
        for (int gy = 1; gy <= h; gy++)
        {
            for (int gx = 1; gx <= w; gx++)
            {
                if (!IsWallLike(Floor.Map[gy, gx])) continue;
                float x0 = (gx - 1) * CellSize, x1 = gx * CellSize;
                float z0 = (gy - 1) * CellSize, z1 = gy * CellSize;
                var pts = ProjectQuad(x0, floorY, z0, x1, floorY, z0, x1, floorY, z1, x0, floorY, z1,
                    camX, camY, camZ, W, H, out float d);
                if (pts != null) quads.Add(new RenderQuad(pts, Darken(extCol, 0.5f), d, extWall, false, 0.45f));
            }
        }
    }

    private void AddEntityQuads(List<RenderQuad> quads,
        float camX, float camY, float camZ, int W, int H)
    {
        if (Floor == null) return;
        float markerH = 0.15f;
        float pad = 0.3f;

        // Spawn
        AddMarker(quads, Floor.SpawnGX, Floor.SpawnGY, Color.Lime, markerH, pad, camX, camY, camZ, W, H);

        if (Floor.HasUp) AddMarker(quads, Floor.StairsGX, Floor.StairsGY, Color.Green, markerH * 3, pad, camX, camY, camZ, W, H);
        if (Floor.HasDown) AddMarker(quads, Floor.DownGX, Floor.DownGY, Color.RoyalBlue, markerH * 3, pad, camX, camY, camZ, W, H);

        foreach (var e in Floor.Enemies)
        {
            var ec = EnemyColors.GetValueOrDefault(e.EnemyType, Color.Gray);
            AddMarker(quads, e.GX, e.GY, ec, markerH * 2, pad * 0.8f, camX, camY, camZ, W, H);
        }

        foreach (var c in Floor.Consoles)
        {
            var rc = RoomColors.GetValueOrDefault(c.RoomType, Color.Teal);
            AddMarker(quads, c.GX, c.GY, rc, markerH, pad * 0.6f, camX, camY, camZ, W, H);
        }

        foreach (var l in Floor.Loot)
            AddMarker(quads, l.GX, l.GY, Color.Gold, markerH * 2, pad * 0.7f, camX, camY, camZ, W, H);

        // Officers (white diamond markers)
        foreach (var o in Floor.Officers)
            AddMarker(quads, o.GX, o.GY, Color.White, markerH * 2.5f, pad * 0.7f, camX, camY, camZ, W, H);

        // NPCs (cyan markers)
        foreach (var n in Floor.Npcs)
            AddMarker(quads, n.GX, n.GY, Color.Cyan, markerH * 2.5f, pad * 0.7f, camX, camY, camZ, W, H);
    }

    private void AddMarker(List<RenderQuad> quads,
        int gx, int gy, Color color, float h, float pad,
        float camX, float camY, float camZ, int W, int H)
    {
        float x0 = (gx - 1) * CellSize + pad;
        float x1 = gx * CellSize - pad;
        float z0 = (gy - 1) * CellSize + pad;
        float z1 = gy * CellSize - pad;

        // Top
        var top = ProjectQuad(x0, h, z0, x1, h, z0, x1, h, z1, x0, h, z1, camX, camY, camZ, W, H, out float td);
        if (top != null) quads.Add(new RenderQuad(top, color, td));

        // Front face
        var front = ProjectQuad(x0, 0, z1, x1, 0, z1, x1, h, z1, x0, h, z1, camX, camY, camZ, W, H, out float fd);
        if (front != null) quads.Add(new RenderQuad(front, Darken(color, 0.7f), fd));

        // Right face
        var right = ProjectQuad(x1, 0, z0, x1, 0, z1, x1, h, z1, x1, h, z0, camX, camY, camZ, W, H, out float rd);
        if (right != null) quads.Add(new RenderQuad(right, Darken(color, 0.8f), rd));
    }

    private PointF[]? ProjectQuad(
        float x0, float y0, float z0,
        float x1, float y1, float z1,
        float x2, float y2, float z2,
        float x3, float y3, float z3,
        float camX, float camY, float camZ, int W, int H,
        out float avgDepth)
    {
        var (sx0, sy0, d0) = Project(x0, y0, z0, camX, camY, camZ, W, H);
        var (sx1, sy1, d1) = Project(x1, y1, z1, camX, camY, camZ, W, H);
        var (sx2, sy2, d2) = Project(x2, y2, z2, camX, camY, camZ, W, H);
        var (sx3, sy3, d3) = Project(x3, y3, z3, camX, camY, camZ, W, H);

        avgDepth = (d0 + d1 + d2 + d3) / 4f;

        // Cull if behind camera
        if (d0 < 0.5f && d1 < 0.5f && d2 < 0.5f && d3 < 0.5f) return null;

        return new PointF[] {
            new(sx0, sy0), new(sx1, sy1), new(sx2, sy2), new(sx3, sy3)
        };
    }

    // ── Input ────────────────────────────────────────────────────

    protected override void OnMouseDown(MouseEventArgs e)
    {
        Focus();
        if (e.Button == MouseButtons.Left || e.Button == MouseButtons.Right)
        {
            _orbiting = e.Button == MouseButtons.Left;
            _panning = e.Button == MouseButtons.Right;
            _lastMouse = e.Location;
        }
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        if (_orbiting)
        {
            int dx = e.X - _lastMouse.X;
            int dy = e.Y - _lastMouse.Y;
            _orbitYaw += dx * 0.5f;
            _orbitPitch = Math.Clamp(_orbitPitch + dy * 0.5f, MinPitch, MaxPitch);
            _lastMouse = e.Location;
        }
        else if (_panning)
        {
            int dx = e.X - _lastMouse.X;
            int dy = e.Y - _lastMouse.Y;
            float yr = _orbitYaw * MathF.PI / 180f;
            float panSpeed = _orbitDist * 0.003f;
            _targetX -= (MathF.Cos(yr) * dx + MathF.Sin(yr) * dy) * panSpeed;
            _targetZ -= (-MathF.Sin(yr) * dx + MathF.Cos(yr) * dy) * panSpeed;
            _lastMouse = e.Location;
        }
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
        _orbiting = false;
        _panning = false;
    }

    protected override void OnMouseWheel(MouseEventArgs e)
    {
        _orbitDist *= e.Delta > 0 ? 0.9f : 1.1f;
        _orbitDist = Math.Clamp(_orbitDist, MinDist, MaxDist);
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.KeyCode == Keys.Escape)
        {
            StopPreview();
            (Parent?.Parent as MainForm)?.Exit3DPreview();
        }
        e.Handled = true;
    }

    protected override bool IsInputKey(Keys keyData) =>
        keyData == Keys.Escape || base.IsInputKey(keyData);

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        if (_buffer != null)
        {
            e.Graphics.InterpolationMode = InterpolationMode.NearestNeighbor;
            e.Graphics.DrawImage(_buffer, 0, 0, Width, Height);
        }
        else
        {
            using var font = new Font("Consolas", 11);
            e.Graphics.DrawString("Press F5 or View > 3D Preview", font, Brushes.White, 10, 10);
        }

        using var hudFont = new Font("Consolas", 9);
        e.Graphics.DrawString("LMB:Orbit  RMB:Pan  Scroll:Zoom  ESC:Exit",
            hudFont, Brushes.LightGray, 4, Height - 18);
    }

    // ── Helpers ──────────────────────────────────────────────────

    private static Color Darken(Color c, float f) =>
        Color.FromArgb(c.A, (int)(c.R * f), (int)(c.G * f), (int)(c.B * f));

    private static Color Lerp(Color a, Color b, float t) =>
        Color.FromArgb(255,
            (int)(a.R + (b.R - a.R) * t),
            (int)(a.G + (b.G - a.G) * t),
            (int)(a.B + (b.B - a.B) * t));
}
