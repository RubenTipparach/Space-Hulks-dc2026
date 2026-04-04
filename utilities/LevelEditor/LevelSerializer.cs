using System.Text.Json;
using System.Text.Json.Serialization;

namespace SpaceHulksLevelEditor;

public static class LevelSerializer
{
    private static readonly JsonSerializerOptions Options = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        Converters = { new JsonStringEnumConverter() },
    };

    // Intermediate DTO for JSON (can't serialize int[,] directly)
    private class FloorDto
    {
        public int Width { get; set; }
        public int Height { get; set; }
        public int[][] Map { get; set; } = Array.Empty<int[]>();
        public int SpawnGX { get; set; }
        public int SpawnGY { get; set; }
        public int StairsGX { get; set; }
        public int StairsGY { get; set; }
        public int StairsDir { get; set; }
        public bool HasUp { get; set; }
        public int DownGX { get; set; }
        public int DownGY { get; set; }
        public int DownDir { get; set; }
        public bool HasDown { get; set; }
        public List<RoomData> Rooms { get; set; } = new();
        public List<EntityData> Enemies { get; set; } = new();
        public List<ConsoleData> Consoles { get; set; } = new();
        public List<LootData> Loot { get; set; } = new();
        public List<OfficerData> Officers { get; set; } = new();
        public List<NpcData> Npcs { get; set; } = new();
        public List<WindowFace>? Windows { get; set; }
    }

    private class LevelDto
    {
        public string ShipName { get; set; } = "";
        public ShipType ShipType { get; set; } = ShipType.Human;
        public bool IsHub { get; set; }
        public int HullHp { get; set; } = 30;
        public int HullHpMax { get; set; } = 30;
        public List<MissionData> Missions { get; set; } = new();
        public List<FloorDto> Floors { get; set; } = new();
    }

    public static string Serialize(LevelData level)
    {
        var dto = new LevelDto
        {
            ShipName = level.ShipName,
            ShipType = level.ShipType,
            IsHub = level.IsHub,
            HullHp = level.HullHp,
            HullHpMax = level.HullHpMax,
            Missions = level.Missions,
        };
        foreach (var f in level.Floors)
        {
            var fd = new FloorDto
            {
                Width = f.Width, Height = f.Height,
                SpawnGX = f.SpawnGX, SpawnGY = f.SpawnGY,
                StairsGX = f.StairsGX, StairsGY = f.StairsGY, StairsDir = f.StairsDir,
                HasUp = f.HasUp,
                DownGX = f.DownGX, DownGY = f.DownGY, DownDir = f.DownDir,
                HasDown = f.HasDown,
                Rooms = f.Rooms, Enemies = f.Enemies,
                Consoles = f.Consoles, Loot = f.Loot,
                Officers = f.Officers, Npcs = f.Npcs,
                Windows = f.Windows.Count > 0 ? f.Windows : null,
            };
            // Convert 2D array to jagged (1-indexed rows 1..h, cols 1..w)
            fd.Map = new int[f.Height][];
            for (int y = 0; y < f.Height; y++)
            {
                fd.Map[y] = new int[f.Width];
                for (int x = 0; x < f.Width; x++)
                    fd.Map[y][x] = f.Map[y + 1, x + 1];
            }
            dto.Floors.Add(fd);
        }
        return JsonSerializer.Serialize(dto, Options);
    }

    public static LevelData Deserialize(string json)
    {
        Console.WriteLine($"[LevelSerializer] Parsing JSON ({json.Length} chars)...");
        var dto = JsonSerializer.Deserialize<LevelDto>(json, Options)
                  ?? throw new Exception("Invalid JSON");
        Console.WriteLine($"[LevelSerializer] Ship: \"{dto.ShipName}\" Type={dto.ShipType} Hub={dto.IsHub} Floors={dto.Floors.Count}");
        var level = new LevelData
        {
            ShipName = dto.ShipName,
            ShipType = dto.ShipType,
            IsHub = dto.IsHub,
            HullHp = dto.HullHp,
            HullHpMax = dto.HullHpMax,
            Missions = dto.Missions,
            Floors = new(),
        };
        foreach (var fd in dto.Floors)
        {
            var f = new FloorData
            {
                Width = fd.Width, Height = fd.Height,
                SpawnGX = fd.SpawnGX, SpawnGY = fd.SpawnGY,
                StairsGX = fd.StairsGX, StairsGY = fd.StairsGY, StairsDir = fd.StairsDir,
                HasUp = fd.HasUp,
                DownGX = fd.DownGX, DownGY = fd.DownGY, DownDir = fd.DownDir,
                HasDown = fd.HasDown,
                Rooms = fd.Rooms, Enemies = fd.Enemies,
                Consoles = fd.Consoles, Loot = fd.Loot,
                Officers = fd.Officers, Npcs = fd.Npcs,
            };
            Console.WriteLine($"[LevelSerializer]   Floor {level.Floors.Count}: {fd.Width}x{fd.Height} " +
                              $"spawn=({fd.SpawnGX},{fd.SpawnGY}) rooms={fd.Rooms.Count} " +
                              $"enemies={fd.Enemies.Count} npcs={fd.Npcs.Count} " +
                              $"map rows={fd.Map.Length}");
            for (int y = 0; y < fd.Height && y < fd.Map.Length; y++)
                for (int x = 0; x < fd.Width && x < fd.Map[y].Length; x++)
                    f.Map[y + 1, x + 1] = fd.Map[y][x];
            f.Windows = fd.Windows ?? new();
            // Migrate old CellType.Window (value 2) to face-based windows
            for (int y = 1; y <= f.Height; y++)
                for (int x = 1; x <= f.Width; x++)
                    if (f.Map[y, x] == 2)
                    {
                        f.Map[y, x] = (int)CellType.Wall;
                        if (y > 1 && f.Map[y - 1, x] == 0) f.Windows.Add(new WindowFace { GX = x, GY = y, Dir = WallDir.North });
                        if (y < f.Height && f.Map[y + 1, x] == 0) f.Windows.Add(new WindowFace { GX = x, GY = y, Dir = WallDir.South });
                        if (x > 1 && f.Map[y, x - 1] == 0) f.Windows.Add(new WindowFace { GX = x, GY = y, Dir = WallDir.West });
                        if (x < f.Width && f.Map[y, x + 1] == 0) f.Windows.Add(new WindowFace { GX = x, GY = y, Dir = WallDir.East });
                    }
            level.Floors.Add(f);
        }
        return level;
    }

    public static void Save(LevelData level, string path) =>
        File.WriteAllText(path, Serialize(level));

    public static LevelData Load(string path) =>
        Deserialize(File.ReadAllText(path));
}
