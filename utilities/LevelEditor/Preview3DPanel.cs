using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.WinForms;
using OpenTK.Windowing.Common;
using System.Runtime.InteropServices;

namespace SpaceHulksLevelEditor;

public class Preview3DPanel : Panel
{
    public FloorData? Floor { get; set; }
    public LevelData? Level { get; set; }
    public bool ShowExterior { get; set; } = true;
    public bool ShowAllFloors { get; set; }
    public bool ShowWireframe { get; set; } = true;
    public bool ShowGhostFloors { get; set; }
    public bool ShowRoof { get; set; }
    public EditorState State { get; set; } = new();
    public int CurrentFloorIndex { get; set; }
    private const float FloorSpacing = WallH;

    // Ghost floors (transparent adjacent)
    private readonly Dictionary<int, List<float>> _ghostBatches = new();

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

    // Per-frame batches: GL texture id -> vertex floats (interior + exterior separate)
    private readonly Dictionary<int, List<float>> _batches = new();
    private readonly Dictionary<int, List<float>> _extBatches = new();

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
        GL.Enable(EnableCap.CullFace);
        GL.CullFace(CullFaceMode.Back);
        GL.FrontFace(FrontFaceDirection.Ccw);

        _glReady = true;
    }

    // ── GL Textures ─────────────────────────────────────────────

    private bool _glTexLoaded;
    private void EnsureGLTextures()
    {
        if (_glTexLoaded) return;
        TextureCache.EnsureLoaded();
        if (TextureCache.WallA == null) return; // not ready yet
        _glTexLoaded = true;
        System.Diagnostics.Debug.WriteLine($"[TEX] WallA={TextureCache.WallA?.Width}x{TextureCache.WallA?.Height} " +
            $"ExtWall={TextureCache.ExteriorWall?.Width}x{TextureCache.ExteriorWall?.Height}");
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

    private bool _emitToExt; // when true, quads go to _extBatches

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
        var dict = _emitToExt ? _extBatches : _batches;
        if (!dict.TryGetValue(tex, out var b))
        {
            b = new List<float>(4096);
            dict[tex] = b;
        }
        // CCW winding for GL front face
        Vert(b, x0, y0, z0, u0, v0, col);
        Vert(b, x2, y2, z2, u2, v2, col);
        Vert(b, x1, y1, z1, u1, v1, col);
        Vert(b, x0, y0, z0, u0, v0, col);
        Vert(b, x3, y3, z3, u3, v3, col);
        Vert(b, x2, y2, z2, u2, v2, col);
    }

    // Emit a triangle (3 verts) for diagonal chamfer walls
    private void Tri(int tex,
        float x0, float y0, float z0, float u0, float v0,
        float x1, float y1, float z1, float u1, float v1,
        float x2, float y2, float z2, float u2, float v2,
        Color col)
    {
        var dict = _emitToExt ? _extBatches : _batches;
        if (!dict.TryGetValue(tex, out var b))
        {
            b = new List<float>(4096);
            dict[tex] = b;
        }
        Vert(b, x0, y0, z0, u0, v0, col);
        Vert(b, x1, y1, z1, u1, v1, col);
        Vert(b, x2, y2, z2, u2, v2, col);
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

    // Compute hull inside mask (shared by interior + exterior)
    private bool[,]? _hullInside;

    private void ComputeHullMask(FloorData fl, int w, int h)
    {
        _hullInside = new bool[h + 2, w + 2];
        var queue = new Queue<(int, int)>();

        int sx = fl.SpawnGX, sy = fl.SpawnGY;
        if (sx >= 1 && sx <= w && sy >= 1 && sy <= h && !IsWallLike(fl.Map[sy, sx]))
        {
            _hullInside[sy, sx] = true;
            queue.Enqueue((sy, sx));
        }
        foreach (var room in fl.Rooms)
        {
            int rcx = room.CenterX, rcy = room.CenterY;
            if (rcx >= 1 && rcx <= w && rcy >= 1 && rcy <= h && !_hullInside[rcy, rcx] && !IsWallLike(fl.Map[rcy, rcx]))
            {
                _hullInside[rcy, rcx] = true;
                queue.Enqueue((rcy, rcx));
            }
        }

        while (queue.Count > 0)
        {
            var (cy, cx) = queue.Dequeue();
            int[] dy = { -1, 1, 0, 0 }, dx = { 0, 0, -1, 1 };
            for (int d = 0; d < 4; d++)
            {
                int ny = cy + dy[d], nx = cx + dx[d];
                if (ny < 1 || ny > h || nx < 1 || nx > w) continue;
                if (_hullInside[ny, nx]) continue;
                if (IsWallLike(fl.Map[ny, nx])) continue;
                _hullInside[ny, nx] = true;
                queue.Enqueue((ny, nx));
            }
        }

        // Expand N layers of walls (one pass per layer, collect-then-mark)
        for (int layer = 0; layer < (int)Math.Ceiling(State.HullPadding); layer++)
        {
            var toExpand = new List<(int y, int x)>();
            for (int gy = 1; gy <= h; gy++)
                for (int gx = 1; gx <= w; gx++)
                    if (IsWallLike(fl.Map[gy, gx]) && !_hullInside[gy, gx])
                        if ((gy > 1 && _hullInside[gy - 1, gx]) || (gy < h && _hullInside[gy + 1, gx]) ||
                            (gx > 1 && _hullInside[gy, gx - 1]) || (gx < w && _hullInside[gy, gx + 1]))
                            toExpand.Add((gy, gx));
            foreach (var (ey, ex) in toExpand)
                _hullInside[ey, ex] = true;
        }

        // Extend room sides
        foreach (var room in fl.Rooms)
        {
            int rx = room.X, ry = room.Y, rw = room.Width, rh = room.Height;
            ExtendRoomSide(fl, _hullInside, w, h, ry - 1, rx, rx + rw - 1, true, -1);
            ExtendRoomSide(fl, _hullInside, w, h, ry + rh, rx, rx + rw - 1, true, +1);
            ExtendRoomSide(fl, _hullInside, w, h, rx - 1, ry, ry + rh - 1, false, -1);
            ExtendRoomSide(fl, _hullInside, w, h, rx + rw, ry, ry + rh - 1, false, +1);
        }
    }

    private void ExtendRoomSide(FloorData fl, bool[,] inside, int w, int h, int line, int from, int to, bool horizontal, int inwardDir)
    {
        bool any = false;
        for (int i = from; i <= to; i++)
        {
            int gy = horizontal ? line : i, gx = horizontal ? i : line;
            if (gy >= 1 && gy <= h && gx >= 1 && gx <= w && inside[gy, gx]) { any = true; break; }
        }
        if (!any) return;
        // Check that this side faces interior (the opposite side of the wall from the room)
        // by checking if cells on the inward side are inside
        bool facesInward = false;
        for (int i = from; i <= to; i++)
        {
            int gy = horizontal ? line + inwardDir : i, gx = horizontal ? i : line + inwardDir;
            if (gy >= 1 && gy <= h && gx >= 1 && gx <= w && inside[gy, gx]) { facesInward = true; break; }
        }
        if (!facesInward) return;
        for (int i = from; i <= to; i++)
        {
            int gy = horizontal ? line : i, gx = horizontal ? i : line;
            if (gy >= 1 && gy <= h && gx >= 1 && gx <= w && IsWallLike(fl.Map[gy, gx]))
                inside[gy, gx] = true;
        }
    }

    // Y offset for current floor being built
    private float _yOff;

    private void BuildGeometry()
    {
        foreach (var b in _batches.Values) b.Clear();
        foreach (var b in _extBatches.Values) b.Clear();
        foreach (var b in _ghostBatches.Values) b.Clear();
        _emitToExt = false;
        if (Floor == null) return;

        bool alien = State.ShipType == ShipType.Alien;

        if (ShowAllFloors && Level != null && Level.Floors.Count > 1)
        {
            // All floors stacked vertically
            for (int fi = 0; fi < Level.Floors.Count; fi++)
            {
                _yOff = fi * FloorSpacing;
                BuildFloorGeometry(Level.Floors[fi], alien);
            }
        }
        else
        {
            _yOff = 0;
            BuildFloorGeometry(Floor, alien);

            // Ghost: all other floors transparent at their Y offset
            if (ShowGhostFloors && Level != null && Level.Floors.Count > 1)
            {
                int ci = CurrentFloorIndex;
                for (int fi = 0; fi < Level.Floors.Count; fi++)
                {
                    if (fi == ci) continue;
                    _yOff = (fi - ci) * FloorSpacing;
                    BuildGhostFloor(Level.Floors[fi], alien, 0.2f);
                }
                _yOff = 0;
            }
        }
    }

    private void BuildGhostFloor(FloorData floor, bool alien, float alpha)
    {
        int w = floor.Width, h = floor.Height;
        int floorTex = alien ? Tex("stone") : Tex("floor");
        int wallTex = alien ? Tex("bricks") : Tex("wall_a");

        for (int gy = 1; gy <= h; gy++)
        {
            for (int gx = 1; gx <= w; gx++)
            {
                float x0 = (gx - 1) * Cell, x1 = gx * Cell;
                float z0 = (gy - 1) * Cell, z1 = gy * Cell;
                float yb = _yOff, yt = _yOff + WallH;
                int ct = floor.Map[gy, gx];

                if (IsWallLike(ct))
                {
                    Color wc = Color.FromArgb((int)(alpha * 255), 100, 100, 110);
                    if (gy > 1 && !IsWallLike(floor.Map[gy - 1, gx]))
                        WallQuad(wallTex, x0, yb, z0, x1, yb, z0, x1, yt, z0, x0, yt, z0, wc);
                    if (gy < h && !IsWallLike(floor.Map[gy + 1, gx]))
                        WallQuad(wallTex, x1, yb, z1, x0, yb, z1, x0, yt, z1, x1, yt, z1, wc);
                    if (gx > 1 && !IsWallLike(floor.Map[gy, gx - 1]))
                        WallQuad(wallTex, x0, yb, z1, x0, yb, z0, x0, yt, z0, x0, yt, z1, wc);
                    if (gx < w && !IsWallLike(floor.Map[gy, gx + 1]))
                        WallQuad(wallTex, x1, yb, z0, x1, yb, z1, x1, yt, z1, x1, yt, z0, wc);
                }
                else
                {
                    Color fc = Color.FromArgb((int)(alpha * 255), 80, 80, 80);
                    // Add ghost to main batches (will be blended)
                    if (!_ghostBatches.TryGetValue(floorTex, out var b))
                    {
                        b = new List<float>(4096);
                        _ghostBatches[floorTex] = b;
                    }
                    // Emit directly to ghost batch
                    Vert(b, x0, yb, z0, 0, 0, fc); Vert(b, x1, yb, z1, 1, 1, fc); Vert(b, x1, yb, z0, 1, 0, fc);
                    Vert(b, x0, yb, z0, 0, 0, fc); Vert(b, x0, yb, z1, 0, 1, fc); Vert(b, x1, yb, z1, 1, 1, fc);
                }
            }
        }
    }

    private void BuildFloorGeometry(FloorData floor, bool alien)
    {
        int w = floor.Width, h = floor.Height;
        int wallTex = alien ? Tex("bricks") : Tex("wall_a");
        int winTex = alien ? Tex("bricks") : Tex("wall_a_win");
        int floorTex = alien ? Tex("stone") : Tex("floor");

        if (ShowExterior)
            ComputeHullMask(floor, w, h);
        else
            _hullInside = null;

        for (int gy = 1; gy <= h; gy++)
        {
            for (int gx = 1; gx <= w; gx++)
            {
                float x0 = (gx - 1) * Cell, x1 = gx * Cell;
                float z0 = (gy - 1) * Cell, z1 = gy * Cell;
                float yb = _yOff, yt = _yOff + WallH;
                int ct = floor.Map[gy, gx];

                if (IsWallLike(ct))
                {
                    bool isWin = ct == (int)CellType.Window;
                    var room = floor.RoomAt(gx, gy);
                    Color wc = Color.FromArgb(160, 155, 165);
                    if (room != null && RoomColors.ContainsKey(room.Type))
                        wc = Lerp(wc, RoomColors[room.Type], 0.3f);

                    int sideTex = isWin ? winTex : wallTex;
                    Color sc1 = Darken(wc, 0.6f), sc2 = Darken(wc, 0.75f);

                    bool n = gy > 1 && !IsWallLike(floor.Map[gy - 1, gx]);
                    bool s = gy < h && !IsWallLike(floor.Map[gy + 1, gx]);
                    bool we = gx > 1 && !IsWallLike(floor.Map[gy, gx - 1]);
                    bool e = gx < w && !IsWallLike(floor.Map[gy, gx + 1]);

                    if (_hullInside != null)
                    {
                        n = n && _hullInside[gy - 1, gx];
                        s = s && _hullInside[gy + 1, gx];
                        we = we && _hullInside[gy, gx - 1];
                        e = e && _hullInside[gy, gx + 1];
                    }

                    if (n) WallQuad(sideTex, x0, yb, z0, x1, yb, z0, x1, yt, z0, x0, yt, z0, sc1);
                    if (s) WallQuad(sideTex, x1, yb, z1, x0, yb, z1, x0, yt, z1, x1, yt, z1, sc1);
                    if (we) WallQuad(sideTex, x0, yb, z1, x0, yb, z0, x0, yt, z0, x0, yt, z1, sc2);
                    if (e) WallQuad(sideTex, x1, yb, z0, x1, yb, z1, x1, yt, z1, x1, yt, z0, sc2);
                }
                else
                {
                    var room = floor.RoomAt(gx, gy);
                    Color fc = Color.FromArgb(130, 130, 125);
                    if (room != null && RoomColors.ContainsKey(room.Type))
                        fc = Lerp(fc, RoomColors[room.Type], 0.25f);
                    FlatQuad(floorTex, x0, yb, z0, x1, z1, fc);
                }
            }
        }

        // Exterior
        if (ShowExterior && _hullInside != null)
        {
            _emitToExt = true;
            BuildExteriorForFloor(floor, w, h, alien);
            _emitToExt = false;
        }

        // Entities
        BuildEntitiesForFloor(floor);
    }

    private void BuildExteriorForFloor(FloorData floor, int w, int h, bool alien)
    {
        if (_hullInside == null) return;
        int extW = alien ? Tex("alien_ext") : Tex("ext_wall");
        int extWin = alien ? Tex("alien_ext_win") : Tex("ext_win");
        if (extW == 0) extW = Tex("wall_a");
        if (extWin == 0) extWin = Tex("wall_a_win");
        Color ec = alien ? Color.FromArgb(120, 60, 80) : Color.FromArgb(255, 255, 255);
        const float ExtH = WallH;
        Color sN = Darken(ec, 0.6f);
        Color sE = Darken(ec, 0.75f);

        float c = State.HullCorner * Cell; // chamfer offset
        Color sD = Darken(ec, 0.67f); // diagonal face color

        for (int gy = 1; gy <= h; gy++)
        {
            for (int gx = 1; gx <= w; gx++)
            {
                if (!_hullInside[gy, gx]) continue;

                bool isWin = floor.Map[gy, gx] == (int)CellType.Window;
                int ft = isWin ? extWin : extW;
                float x0 = (gx - 1) * Cell, x1 = gx * Cell;
                float z0 = (gy - 1) * Cell, z1 = gy * Cell;
                float yb = _yOff, yt = _yOff + ExtH;

                bool oN = !_hullInside[gy - 1, gx];
                bool oS = !_hullInside[gy + 1, gx];
                bool oW = !_hullInside[gy, gx - 1];
                bool oE = !_hullInside[gy, gx + 1];

                // Detect convex corners
                bool cNW = oN && oW && c > 0;
                bool cNE = oN && oE && c > 0;
                bool cSW = oS && oW && c > 0;
                bool cSE = oS && oE && c > 0;

                // North wall (x0,z0) to (x1,z0) — trimmed at corners
                if (oN)
                {
                    float nx0 = cNW ? x0 + c : x0;
                    float nx1 = cNE ? x1 - c : x1;
                    if (nx0 < nx1)
                        WallQuad(ft, nx0, yb, z0, nx1, yb, z0, nx1, yt, z0, nx0, yt, z0, sN);
                }
                // South wall (x1,z1) to (x0,z1)
                if (oS)
                {
                    float sx0 = cSW ? x0 + c : x0;
                    float sx1 = cSE ? x1 - c : x1;
                    if (sx0 < sx1)
                        WallQuad(ft, sx1, yb, z1, sx0, yb, z1, sx0, yt, z1, sx1, yt, z1, sN);
                }
                // West wall (x0,z1) to (x0,z0)
                if (oW)
                {
                    float wz0 = cNW ? z0 + c : z0;
                    float wz1 = cSW ? z1 - c : z1;
                    if (wz0 < wz1)
                        WallQuad(ft, x0, yb, wz1, x0, yb, wz0, x0, yt, wz0, x0, yt, wz1, sE);
                }
                // East wall (x1,z0) to (x1,z1)
                if (oE)
                {
                    float ez0 = cNE ? z0 + c : z0;
                    float ez1 = cSE ? z1 - c : z1;
                    if (ez0 < ez1)
                        WallQuad(ft, x1, yb, ez0, x1, yb, ez1, x1, yt, ez1, x1, yt, ez0, sE);
                }

                // Diagonal chamfer walls at corners
                if (cNW)
                    WallQuad(ft, x0, yb, z0 + c, x0 + c, yb, z0, x0 + c, yt, z0, x0, yt, z0 + c, sD);
                if (cNE)
                    WallQuad(ft, x1 - c, yb, z0, x1, yb, z0 + c, x1, yt, z0 + c, x1 - c, yt, z0, sD);
                if (cSW)
                    WallQuad(ft, x0 + c, yb, z1, x0, yb, z1 - c, x0, yt, z1 - c, x0 + c, yt, z1, sD);
                if (cSE)
                    WallQuad(ft, x1, yb, z1 - c, x1 - c, yb, z1, x1 - c, yt, z1, x1, yt, z1 - c, sD);
            }
        }

        // Roof: cover inside cells where no floor exists above, with chamfered corners
        if (ShowRoof)
        {
            int roofTex = Tex("roof");
            Color rc = Darken(ec, 0.85f);
            float roofY = _yOff + ExtH;
            for (int gy = 1; gy <= h; gy++)
            {
                for (int gx = 1; gx <= w; gx++)
                {
                    if (!_hullInside[gy, gx] || HasFloorAbove(floor, gx, gy)) continue;

                    float x0 = (gx - 1) * Cell, x1 = gx * Cell;
                    float z0 = (gy - 1) * Cell, z1 = gy * Cell;

                    if (c <= 0)
                    {
                        FlatQuad(roofTex, x0, roofY, z0, x1, z1, rc);
                        continue;
                    }

                    bool oN = !_hullInside[gy - 1, gx];
                    bool oS = !_hullInside[gy + 1, gx];
                    bool oW = !_hullInside[gy, gx - 1];
                    bool oE = !_hullInside[gy, gx + 1];

                    bool cNW = oN && oW;
                    bool cNE = oN && oE;
                    bool cSW = oS && oW;
                    bool cSE = oS && oE;

                    // Build polygon vertices CW from top-left
                    var pts = new List<(float x, float z)>(8);
                    if (cNW) { pts.Add((x0, z0 + c)); pts.Add((x0 + c, z0)); }
                    else pts.Add((x0, z0));
                    if (cNE) { pts.Add((x1 - c, z0)); pts.Add((x1, z0 + c)); }
                    else pts.Add((x1, z0));
                    if (cSE) { pts.Add((x1, z1 - c)); pts.Add((x1 - c, z1)); }
                    else pts.Add((x1, z1));
                    if (cSW) { pts.Add((x0 + c, z1)); pts.Add((x0, z1 - c)); }
                    else pts.Add((x0, z1));

                    // Triangle fan from center
                    float cx = (x0 + x1) * 0.5f, cz = (z0 + z1) * 0.5f;
                    float cu = 0.5f, cv = 0.5f;
                    float invW = 1f / (x1 - x0), invH = 1f / (z1 - z0);
                    for (int i = 0; i < pts.Count; i++)
                    {
                        var a = pts[i];
                        var b = pts[(i + 1) % pts.Count];
                        Tri(roofTex,
                            cx, roofY, cz, cu, cv,
                            a.x, roofY, a.z, (a.x - x0) * invW, (a.z - z0) * invH,
                            b.x, roofY, b.z, (b.x - x0) * invW, (b.z - z0) * invH,
                            rc);
                    }
                }
            }
        }
    }

    private bool HasFloorAbove(FloorData floor, int gx, int gy)
    {
        if (Level == null) return false;
        int fi = Level.Floors.IndexOf(floor);
        if (fi < 0 || fi >= Level.Floors.Count - 1) return false;
        var above = Level.Floors[fi + 1];
        if (gx < 1 || gx > above.Width || gy < 1 || gy > above.Height) return false;
        // If the cell above is open (corridor/room), there's a floor above
        return !IsWallLike(above.Map[gy, gx]);
    }

    private void BuildEntitiesForFloor(FloorData floor)
    {
        float pad = 0.3f;
        Marker(floor.SpawnGX, floor.SpawnGY, Color.Lime, 0.15f, pad);
        if (floor.HasUp) Marker(floor.StairsGX, floor.StairsGY, Color.Green, 0.45f, pad);
        if (floor.HasDown) Marker(floor.DownGX, floor.DownGY, Color.RoyalBlue, 0.45f, pad);
        foreach (var e in floor.Enemies)
            Marker(e.GX, e.GY, EnemyColors.GetValueOrDefault(e.EnemyType, Color.Gray), 0.3f, pad * 0.8f);
        foreach (var c in floor.Consoles)
            Marker(c.GX, c.GY, RoomColors.GetValueOrDefault(c.RoomType, Color.Teal), 0.15f, pad * 0.6f);
        foreach (var l in floor.Loot) Marker(l.GX, l.GY, Color.Gold, 0.3f, pad * 0.7f);
        foreach (var o in floor.Officers) Marker(o.GX, o.GY, Color.White, 0.4f, pad * 0.7f);
        foreach (var n in floor.Npcs) Marker(n.GX, n.GY, Color.Cyan, 0.4f, pad * 0.7f);
    }

    private void Marker(int gx, int gy, Color c, float mh, float pad)
    {
        float x0 = (gx - 1) * Cell + pad, x1 = gx * Cell - pad;
        float z0 = (gy - 1) * Cell + pad, z1 = gy * Cell - pad;
        float yb = _yOff, yt = _yOff + mh;
        FlatQuad(0, x0, yt, z0, x1, z1, c);
        Color d1 = Darken(c, 0.7f);
        WallQuad(0, x0, yb, z1, x1, yb, z1, x1, yt, z1, x0, yt, z1, d1);
        Color d2 = Darken(c, 0.8f);
        WallQuad(0, x1, yb, z0, x1, yb, z1, x1, yt, z1, x1, yt, z0, d2);
    }

    // ── Rendering ───────────────────────────────────────────────

    private void Render()
    {
        if (!_glReady || _gl == null) return;
        _gl.MakeCurrent();

        GL.Viewport(0, 0, _gl.ClientSize.Width, _gl.ClientSize.Height);
        GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit);

        EnsureGLTextures();
        if (Floor == null) { _gl.SwapBuffers(); return; }

        BuildGeometry();

        GL.UseProgram(_prog);
        var mvp = MVP();
        GL.UniformMatrix4(_mvpLoc, false, ref mvp);
        GL.BindVertexArray(_vao);

        // Pass 1: interior geometry
        DrawBatches(_batches);
        // Pass 2: exterior geometry (filled)
        DrawBatches(_extBatches);
        // Pass 3: exterior wireframe overlay
        if (ShowExterior && ShowWireframe)
        {
            GL.Disable(EnableCap.CullFace);
            GL.PolygonMode(MaterialFace.FrontAndBack, PolygonMode.Line);
            GL.Uniform1(_useTexLoc, 0);
            GL.LineWidth(1.5f);
            foreach (var (texId, data) in _extBatches)
            {
                if (data.Count == 0) continue;
                float[] arr = data.ToArray();
                for (int i = 0; i < arr.Length; i += 9)
                {
                    arr[i + 5] = 0.7f; arr[i + 6] = 0.8f; arr[i + 7] = 1f; arr[i + 8] = 1f;
                }
                GL.BindBuffer(BufferTarget.ArrayBuffer, _vbo);
                GL.BufferData(BufferTarget.ArrayBuffer, arr.Length * sizeof(float), arr, BufferUsageHint.StreamDraw);
                GL.DrawArrays(PrimitiveType.Triangles, 0, arr.Length / 9);
            }
            GL.PolygonMode(MaterialFace.FrontAndBack, PolygonMode.Fill);
            GL.Enable(EnableCap.CullFace);
        }
        // Pass 4: ghost floors (transparent, alpha blended)
        if (_ghostBatches.Values.Any(b => b.Count > 0))
        {
            GL.Enable(EnableCap.Blend);
            GL.BlendFunc(BlendingFactor.SrcAlpha, BlendingFactor.OneMinusSrcAlpha);
            DrawBatches(_ghostBatches);
            GL.Disable(EnableCap.Blend);
        }

        GL.BindVertexArray(0);
        GL.UseProgram(0);

        _gl.SwapBuffers();
    }

    private void DrawBatches(Dictionary<int, List<float>> batches)
    {
        foreach (var (texId, data) in batches)
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
