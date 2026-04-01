using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.WinForms;
using OpenTK.Windowing.Common;
using System.Runtime.InteropServices;

namespace SpaceHulksLevelEditor;

public class Preview3DPanel : Panel
{
    public FloorData? Floor { get; set; }
    public bool ShowExterior { get; set; }
    public ShipType ShipType { get; set; } = ShipType.Human;

    private GLControl? _gl;
    private bool _glReady;

    // Shader
    private int _prog, _mvpLoc, _useTexLoc;

    // Geometry
    private int _vao, _vbo;

    // GL textures (keyed by name)
    private readonly Dictionary<string, int> _glTex = new();

    // Camera
    private float _yaw = 45f, _pitch = 55f, _dist = 30f;
    private float _tgtX, _tgtY, _tgtZ;
    private const float Cell = 2f, WallH = 2f;
    private const float MinPitch = 10f, MaxPitch = 89f;
    private const float MinDist = 5f, MaxDist = 120f;

    // Input
    private bool _orbiting, _panning;
    private Point _lastMouse;

    // Timer
    private System.Windows.Forms.Timer _timer;

    // Per-frame batches: GL texture id -> vertex floats
    private readonly Dictionary<int, List<float>> _batches = new();

    // Colors
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

    // ── Constructor ─────────────────────────────────────────────

    public Preview3DPanel()
    {
        DoubleBuffered = true;
        _timer = new System.Windows.Forms.Timer { Interval = 16 }; // ~60fps
        _timer.Tick += (_, _) => _gl?.Invalidate();
    }

    private void EnsureGL()
    {
        if (_gl != null) return;
        try
        {
            _gl = new GLControl();
            _gl.Dock = DockStyle.Fill;
            _gl.Load += (_, _) => InitGL();
            _gl.Paint += (_, _) => Render();
            _gl.Resize += (_, _) => { if (_glReady) GL.Viewport(0, 0, _gl.ClientSize.Width, _gl.ClientSize.Height); };
            _gl.MouseDown += OnGLMouse;
            _gl.MouseMove += OnGLMouseMove;
            _gl.MouseUp += (_, _) => { _orbiting = false; _panning = false; };
            _gl.MouseWheel += OnGLWheel;
            _gl.KeyDown += OnGLKey;
            Controls.Add(_gl);
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"GLControl failed: {ex.Message}");
        }
    }

    public void StartPreview()
    {
        if (Floor == null) return;
        TextureCache.EnsureLoaded();
        EnsureGL(); // create GL control lazily on first 3D preview
        _tgtX = Floor.Width * Cell / 2f;
        _tgtZ = Floor.Height * Cell / 2f;
        _dist = Math.Max(Floor.Width, Floor.Height) * Cell * 0.8f;
        _timer.Start();
        _gl?.Focus();
    }

    public void StopPreview() => _timer.Stop();

    // ── GL Init ─────────────────────────────────────────────────

    private const string VertSrc = @"#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aCol;
uniform mat4 uMVP;
out vec2 vUV; out vec4 vCol;
void main(){
    gl_Position = uMVP * vec4(aPos,1.0);
    vUV = aUV; vCol = aCol;
}";

    private const string FragSrc = @"#version 330 core
in vec2 vUV; in vec4 vCol;
uniform sampler2D uTex;
uniform int uUseTex;
out vec4 oColor;
void main(){
    vec4 t = uUseTex!=0 ? texture(uTex, vUV) : vec4(1.0);
    oColor = t * vCol;
}";

    private void InitGL()
    {
        int vs = GL.CreateShader(ShaderType.VertexShader);
        GL.ShaderSource(vs, VertSrc);
        GL.CompileShader(vs);

        int fs = GL.CreateShader(ShaderType.FragmentShader);
        GL.ShaderSource(fs, FragSrc);
        GL.CompileShader(fs);

        _prog = GL.CreateProgram();
        GL.AttachShader(_prog, vs);
        GL.AttachShader(_prog, fs);
        GL.LinkProgram(_prog);
        GL.DeleteShader(vs);
        GL.DeleteShader(fs);

        _mvpLoc = GL.GetUniformLocation(_prog, "uMVP");
        _useTexLoc = GL.GetUniformLocation(_prog, "uUseTex");

        _vao = GL.GenVertexArray();
        _vbo = GL.GenBuffer();
        GL.BindVertexArray(_vao);
        GL.BindBuffer(BufferTarget.ArrayBuffer, _vbo);
        // pos(3) + uv(2) + color(4) = 9 floats = 36 bytes
        GL.VertexAttribPointer(0, 3, VertexAttribPointerType.Float, false, 36, 0);
        GL.EnableVertexAttribArray(0);
        GL.VertexAttribPointer(1, 2, VertexAttribPointerType.Float, false, 36, 12);
        GL.EnableVertexAttribArray(1);
        GL.VertexAttribPointer(2, 4, VertexAttribPointerType.Float, false, 36, 20);
        GL.EnableVertexAttribArray(2);
        GL.BindVertexArray(0);

        GL.ClearColor(0.07f, 0.07f, 0.11f, 1f);
        GL.Enable(EnableCap.DepthTest);
        GL.DepthFunc(DepthFunction.Lequal); // allow coplanar exterior faces
        GL.Disable(EnableCap.CullFace);

        _glReady = true;
        LoadGLTextures();
    }

    // ── GL Textures ─────────────────────────────────────────────

    private void LoadGLTextures()
    {
        _glTex["wall_a"] = UploadTex(TextureCache.WallA);
        _glTex["wall_a_win"] = UploadTex(TextureCache.WallAWindow);
        _glTex["floor"] = UploadTex(TextureCache.Floor);
        _glTex["bricks"] = UploadTex(TextureCache.Bricks);
        _glTex["stone"] = UploadTex(TextureCache.Stone);
        _glTex["ext_wall"] = UploadTex(TextureCache.ExteriorWall);
        _glTex["ext_win"] = UploadTex(TextureCache.ExteriorWindow);
        _glTex["alien_ext"] = UploadTex(TextureCache.AlienExterior);
        _glTex["alien_ext_win"] = UploadTex(TextureCache.AlienExteriorWindow);
        _glTex["roof"] = UploadTex(TextureCache.Ceiling);
    }

    private static int UploadTex(Bitmap? bmp)
    {
        if (bmp == null) return 0;
        int id = GL.GenTexture();
        GL.BindTexture(TextureTarget.Texture2D, id);
        var bits = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height),
            System.Drawing.Imaging.ImageLockMode.ReadOnly,
            System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba,
            bmp.Width, bmp.Height, 0, PixelFormat.Bgra, PixelType.UnsignedByte, bits.Scan0);
        bmp.UnlockBits(bits);
        GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Nearest);
        GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Nearest);
        GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureWrapS, (int)TextureWrapMode.Repeat);
        GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureWrapT, (int)TextureWrapMode.Repeat);
        return id;
    }

    private int Tex(string name) => _glTex.GetValueOrDefault(name, 0);

    // ── Camera ──────────────────────────────────────────────────

    private Vector3 CamPos()
    {
        float yr = MathHelper.DegreesToRadians(_yaw);
        float pr = MathHelper.DegreesToRadians(_pitch);
        float cp = MathF.Cos(pr);
        return new Vector3(
            _tgtX + _dist * cp * MathF.Sin(yr),
            _tgtY + _dist * MathF.Sin(pr),
            _tgtZ + _dist * cp * MathF.Cos(yr));
    }

    private Matrix4 MVP()
    {
        var eye = CamPos();
        var target = new Vector3(_tgtX, _tgtY, _tgtZ);
        var view = Matrix4.LookAt(eye, target, Vector3.UnitY);
        float aspect = _gl != null && _gl.ClientSize.Height > 0
            ? (float)_gl.ClientSize.Width / _gl.ClientSize.Height : 1f;
        var proj = Matrix4.CreatePerspectiveFieldOfView(
            MathHelper.DegreesToRadians(50f), aspect, 0.1f, 300f);
        return view * proj;
    }

    // ── Geometry Building ───────────────────────────────────────

    private static bool IsWallLike(int c) => c == (int)CellType.Wall || c == (int)CellType.Window;

    private void Vert(List<float> b, float x, float y, float z, float u, float v, Color c)
    {
        b.Add(x); b.Add(y); b.Add(z);
        b.Add(u); b.Add(v);
        b.Add(c.R / 255f); b.Add(c.G / 255f); b.Add(c.B / 255f); b.Add(c.A / 255f);
    }

    private void Quad(int tex,
        float x0, float y0, float z0, float u0, float v0,
        float x1, float y1, float z1, float u1, float v1,
        float x2, float y2, float z2, float u2, float v2,
        float x3, float y3, float z3, float u3, float v3,
        Color col)
    {
        if (!_batches.TryGetValue(tex, out var b))
        {
            b = new List<float>(4096);
            _batches[tex] = b;
        }
        Vert(b, x0, y0, z0, u0, v0, col);
        Vert(b, x1, y1, z1, u1, v1, col);
        Vert(b, x2, y2, z2, u2, v2, col);
        Vert(b, x0, y0, z0, u0, v0, col);
        Vert(b, x2, y2, z2, u2, v2, col);
        Vert(b, x3, y3, z3, u3, v3, col);
    }

    // Emit a wall-side quad (vertical face) with proper UVs
    private void WallQuad(int tex,
        float x0, float y0, float z0, float x1, float y1, float z1,
        float x2, float y2, float z2, float x3, float y3, float z3, Color col)
    {
        // v0,v1 = bottom edge, v2,v3 = top edge
        Quad(tex, x0, y0, z0, 0, 1, x1, y1, z1, 1, 1,
                  x2, y2, z2, 1, 0, x3, y3, z3, 0, 0, col);
    }

    // Emit a horizontal quad (floor/ceiling/top) with proper UVs
    private void FlatQuad(int tex,
        float x0, float y, float z0, float x1, float z1, Color col)
    {
        Quad(tex, x0, y, z0, 0, 0, x1, y, z0, 1, 0,
                  x1, y, z1, 1, 1, x0, y, z1, 0, 1, col);
    }

    private void BuildGeometry()
    {
        foreach (var b in _batches.Values) b.Clear();
        if (Floor == null) return;

        int w = Floor.Width, h = Floor.Height;
        bool alien = ShipType == ShipType.Alien;
        int wallTex = alien ? Tex("bricks") : Tex("wall_a");
        int winTex = alien ? Tex("bricks") : Tex("wall_a_win");
        int floorTex = alien ? Tex("stone") : Tex("floor");

        for (int gy = 1; gy <= h; gy++)
        {
            for (int gx = 1; gx <= w; gx++)
            {
                float x0 = (gx - 1) * Cell, x1 = gx * Cell;
                float z0 = (gy - 1) * Cell, z1 = gy * Cell;
                int ct = Floor.Map[gy, gx];

                if (IsWallLike(ct))
                {
                    bool isWin = ct == (int)CellType.Window;
                    var room = Floor.RoomAt(gx, gy);
                    Color wc = Color.FromArgb(160, 155, 165);
                    if (room != null && RoomColors.ContainsKey(room.Type))
                        wc = Lerp(wc, RoomColors[room.Type], 0.3f);

                    int sideTex = isWin ? winTex : wallTex;

                    // Side faces (only if adjacent is open)
                    Color sc1 = Darken(wc, 0.6f), sc2 = Darken(wc, 0.75f);

                    if (gy > 1 && !IsWallLike(Floor.Map[gy - 1, gx]))
                        WallQuad(sideTex, x0, 0, z0, x1, 0, z0, x1, WallH, z0, x0, WallH, z0, sc1);
                    if (gy < h && !IsWallLike(Floor.Map[gy + 1, gx]))
                        WallQuad(sideTex, x1, 0, z1, x0, 0, z1, x0, WallH, z1, x1, WallH, z1, sc1);
                    if (gx > 1 && !IsWallLike(Floor.Map[gy, gx - 1]))
                        WallQuad(sideTex, x0, 0, z1, x0, 0, z0, x0, WallH, z0, x0, WallH, z1, sc2);
                    if (gx < w && !IsWallLike(Floor.Map[gy, gx + 1]))
                        WallQuad(sideTex, x1, 0, z0, x1, 0, z1, x1, WallH, z1, x1, WallH, z0, sc2);
                }
                else
                {
                    // Floor
                    var room = Floor.RoomAt(gx, gy);
                    Color fc = Color.FromArgb(130, 130, 125);
                    if (room != null && RoomColors.ContainsKey(room.Type))
                        fc = Lerp(fc, RoomColors[room.Type], 0.25f);
                    FlatQuad(floorTex, x0, 0, z0, x1, z1, fc);
                }
            }
        }

        // Exterior
        if (ShowExterior)
            BuildExterior(w, h, alien);

        // Entity markers
        BuildEntities();
    }

    private void BuildExterior(int w, int h, bool alien)
    {
        if (Floor == null) return;
        int extW = alien ? Tex("alien_ext") : Tex("ext_wall");
        int extWin = alien ? Tex("alien_ext_win") : Tex("ext_win");
        Color ec = alien ? Color.FromArgb(120, 60, 80) : Color.FromArgb(60, 80, 130);
        Color es = Darken(ec, 0.7f);
        const float ExtH = WallH;

        // Build "inside" mask: flood fill from spawn through open cells,
        // then expand to include adjacent walls. This ensures corridors
        // and rooms are inside even if they touch the grid edge.
        bool[,] inside = new bool[h + 2, w + 2];
        var queue = new Queue<(int, int)>();

        // Seed from spawn + all open cells (any reachable open cell is inside)
        int sx = Floor.SpawnGX, sy = Floor.SpawnGY;
        if (sx >= 1 && sx <= w && sy >= 1 && sy <= h && !IsWallLike(Floor.Map[sy, sx]))
        {
            inside[sy, sx] = true;
            queue.Enqueue((sy, sx));
        }
        // Also seed from all room centers as fallback
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

        // Expand: walls adjacent to reachable cells are also inside
        for (int gy = 1; gy <= h; gy++)
            for (int gx = 1; gx <= w; gx++)
                if (IsWallLike(Floor.Map[gy, gx]) && !inside[gy, gx])
                    if ((gy > 1 && inside[gy - 1, gx]) || (gy < h && inside[gy + 1, gx]) ||
                        (gx > 1 && inside[gy, gx - 1]) || (gx < w && inside[gy, gx + 1]))
                        inside[gy, gx] = true;

        // Convex hull per row: for each row, find leftmost and rightmost
        // inside cell, then mark everything between as inside (fills concavities)
        for (int gy = 1; gy <= h; gy++)
        {
            int left = w + 1, right = 0;
            for (int gx = 1; gx <= w; gx++)
                if (inside[gy, gx]) { left = Math.Min(left, gx); right = Math.Max(right, gx); }
            if (left <= right)
                for (int gx = left; gx <= right; gx++)
                    inside[gy, gx] = true;
        }
        // Also per column to fill vertical concavities
        for (int gx = 1; gx <= w; gx++)
        {
            int top = h + 1, bottom = 0;
            for (int gy = 1; gy <= h; gy++)
                if (inside[gy, gx]) { top = Math.Min(top, gy); bottom = Math.Max(bottom, gy); }
            if (top <= bottom)
                for (int gy = top; gy <= bottom; gy++)
                    inside[gy, gx] = true;
        }

        // Draw exterior walls at boundaries (inside cell next to non-inside)
        // Winding: CW from outside (matches GL.FrontFace(CW))
        for (int gy = 1; gy <= h; gy++)
        {
            for (int gx = 1; gx <= w; gx++)
            {
                if (!inside[gy, gx]) continue;

                bool isWin = Floor.Map[gy, gx] == (int)CellType.Window;
                int ft = isWin ? extWin : extW;
                float x0 = (gx - 1) * Cell, x1 = gx * Cell;
                float z0 = (gy - 1) * Cell, z1 = gy * Cell;

                // Exterior faces: reversed winding (CW from outside)
                // North face (visible from north)
                if (!inside[gy - 1, gx])
                    WallQuad(ft, x1, 0, z0, x0, 0, z0, x0, ExtH, z0, x1, ExtH, z0, es);
                // South face (visible from south)
                if (!inside[gy + 1, gx])
                    WallQuad(ft, x0, 0, z1, x1, 0, z1, x1, ExtH, z1, x0, ExtH, z1, es);
                // West face (visible from west)
                if (!inside[gy, gx - 1])
                    WallQuad(ft, x0, 0, z0, x0, 0, z1, x0, ExtH, z1, x0, ExtH, z0, ec);
                // East face (visible from east)
                if (!inside[gy, gx + 1])
                    WallQuad(ft, x1, 0, z1, x1, 0, z0, x1, ExtH, z0, x1, ExtH, z1, ec);
            }
        }
    }

    private void BuildEntities()
    {
        if (Floor == null) return;
        float pad = 0.3f;
        Marker(Floor.SpawnGX, Floor.SpawnGY, Color.Lime, 0.15f, pad);
        if (Floor.HasUp) Marker(Floor.StairsGX, Floor.StairsGY, Color.Green, 0.45f, pad);
        if (Floor.HasDown) Marker(Floor.DownGX, Floor.DownGY, Color.RoyalBlue, 0.45f, pad);
        foreach (var e in Floor.Enemies)
            Marker(e.GX, e.GY, EnemyColors.GetValueOrDefault(e.EnemyType, Color.Gray), 0.3f, pad * 0.8f);
        foreach (var c in Floor.Consoles)
            Marker(c.GX, c.GY, RoomColors.GetValueOrDefault(c.RoomType, Color.Teal), 0.15f, pad * 0.6f);
        foreach (var l in Floor.Loot) Marker(l.GX, l.GY, Color.Gold, 0.3f, pad * 0.7f);
        foreach (var o in Floor.Officers) Marker(o.GX, o.GY, Color.White, 0.4f, pad * 0.7f);
        foreach (var n in Floor.Npcs) Marker(n.GX, n.GY, Color.Cyan, 0.4f, pad * 0.7f);
    }

    private void Marker(int gx, int gy, Color c, float mh, float pad)
    {
        float x0 = (gx - 1) * Cell + pad, x1 = gx * Cell - pad;
        float z0 = (gy - 1) * Cell + pad, z1 = gy * Cell - pad;
        // Top
        FlatQuad(0, x0, mh, z0, x1, z1, c);
        // Front
        Color d1 = Darken(c, 0.7f);
        WallQuad(0, x0, 0, z1, x1, 0, z1, x1, mh, z1, x0, mh, z1, d1);
        // Right
        Color d2 = Darken(c, 0.8f);
        WallQuad(0, x1, 0, z0, x1, 0, z1, x1, mh, z1, x1, mh, z0, d2);
    }

    // ── Rendering ───────────────────────────────────────────────

    private void Render()
    {
        if (!_glReady || _gl == null) return;
        _gl.MakeCurrent();

        GL.Viewport(0, 0, _gl.ClientSize.Width, _gl.ClientSize.Height);
        GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit);

        if (Floor == null) { _gl.SwapBuffers(); return; }

        BuildGeometry();

        GL.UseProgram(_prog);
        var mvp = MVP();
        GL.UniformMatrix4(_mvpLoc, false, ref mvp);
        GL.BindVertexArray(_vao);

        foreach (var (texId, data) in _batches)
        {
            if (data.Count == 0) continue;
            int vertCount = data.Count / 9;

            GL.BindBuffer(BufferTarget.ArrayBuffer, _vbo);
            float[] arr = data.ToArray();
            GL.BufferData(BufferTarget.ArrayBuffer, arr.Length * sizeof(float), arr, BufferUsageHint.StreamDraw);

            if (texId > 0)
            {
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, texId);
                GL.Uniform1(_useTexLoc, 1);
            }
            else
            {
                GL.Uniform1(_useTexLoc, 0);
            }

            GL.DrawArrays(PrimitiveType.Triangles, 0, vertCount);
        }

        GL.BindVertexArray(0);
        GL.UseProgram(0);

        _gl.SwapBuffers();
    }

    // ── Input ───────────────────────────────────────────────────

    private void OnGLMouse(object? s, MouseEventArgs e)
    {
        _gl?.Focus();
        if (e.Button == MouseButtons.Left) _orbiting = true;
        if (e.Button == MouseButtons.Right || e.Button == MouseButtons.Middle) _panning = true;
        _lastMouse = e.Location;
    }

    private void OnGLMouseMove(object? s, MouseEventArgs e)
    {
        int dx = e.X - _lastMouse.X, dy = e.Y - _lastMouse.Y;
        if (_orbiting)
        {
            _yaw += dx * 0.5f;
            _pitch = Math.Clamp(_pitch + dy * 0.5f, MinPitch, MaxPitch);
        }
        else if (_panning)
        {
            float yr = MathHelper.DegreesToRadians(_yaw);
            float ps = _dist * 0.003f;
            _tgtX -= (MathF.Cos(yr) * dx + MathF.Sin(yr) * dy) * ps;
            _tgtZ -= (-MathF.Sin(yr) * dx + MathF.Cos(yr) * dy) * ps;
        }
        _lastMouse = e.Location;
    }

    private void OnGLWheel(object? s, MouseEventArgs e)
    {
        _dist *= e.Delta > 0 ? 0.9f : 1.1f;
        _dist = Math.Clamp(_dist, MinDist, MaxDist);
    }

    private void OnGLKey(object? s, KeyEventArgs e)
    {
        if (e.KeyCode == Keys.Escape)
        {
            StopPreview();
            (Parent?.Parent as MainForm)?.Exit3DPreview();
        }
        e.Handled = true;
    }

    // ── Paint fallback (for HUD text over GL) ───────────────────

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        if (_gl == null || !_glReady)
        {
            using var f = new Font("Consolas", 11);
            e.Graphics.DrawString("OpenGL not available. Press F5.", f, Brushes.White, 10, 10);
        }
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
