namespace SpaceHulksLevelEditor;

public static class TextureCache
{
    public static Bitmap? WallA { get; private set; }
    public static Bitmap? WallAWindow { get; private set; }
    public static Bitmap? Bricks { get; private set; }
    public static Bitmap? ExteriorWall { get; private set; }
    public static Bitmap? ExteriorWindow { get; private set; }
    public static Bitmap? AlienExterior { get; private set; }
    public static Bitmap? AlienExteriorWindow { get; private set; }
    public static Bitmap? HubFloor { get; private set; }
    public static Bitmap? HubCeiling { get; private set; }
    public static Bitmap? HubCorridor { get; private set; }

    private static bool _loaded;

    public static void EnsureLoaded()
    {
        if (_loaded) return;
        _loaded = true;

        string? basePath = FindAssetsPath();
        if (basePath == null) return;

        WallA = LoadTex(basePath, "wall_A.png");
        WallAWindow = LoadTex(basePath, "wall_A_window.png");
        Bricks = LoadTex(basePath, "alien_exterior.png");  // reuse alien exterior for alien interior
        ExteriorWall = LoadTex(basePath, "exterior_ship_wall.png");
        ExteriorWindow = LoadTex(basePath, "exerior_window.png");
        AlienExterior = LoadTex(basePath, "alien_exterior.png");
        AlienExteriorWindow = LoadTex(basePath, "alien_exterior_window.png");
        HubFloor = LoadTex(basePath, "hub_floor.png");
        HubCeiling = LoadTex(basePath, "hub_ceiling.png");
        HubCorridor = LoadTex(basePath, "hub_corridor_wall.png");
    }

    private static string? FindAssetsPath()
    {
        string?[] startDirs =
        {
            Path.GetDirectoryName(typeof(TextureCache).Assembly.Location),
            Directory.GetCurrentDirectory(),
        };
        foreach (var start in startDirs)
        {
            var dir = start;
            for (int i = 0; i < 10 && dir != null; i++)
            {
                string candidate = Path.Combine(dir, "assets", "textures");
                if (Directory.Exists(candidate)) return candidate;
                dir = Path.GetDirectoryName(dir);
            }
        }
        return null;
    }

    private static Bitmap? LoadTex(string basePath, string name)
    {
        string path = Path.Combine(basePath, name);
        return File.Exists(path) ? new Bitmap(path) : null;
    }
}
