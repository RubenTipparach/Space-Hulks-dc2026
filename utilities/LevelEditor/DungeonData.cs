using System.Text.Json.Serialization;

namespace SpaceHulksLevelEditor;

public enum RoomType
{
    Corridor = 0,
    Bridge = 1,
    Medbay = 2,
    Weapons = 3,
    Engines = 4,
    Reactor = 5,
    Shields = 6,
    Cargo = 7,
    Barracks = 8,
    Teleporter = 9,
}

public enum EnemyType
{
    None = 0,
    Lurker = 1,
    Brute = 2,
    Spitter = 3,
    Hiveguard = 4,
}

public enum CellType
{
    Open = 0,
    Wall = 1,
}

public enum WallDir { North, South, East, West }

public class WindowFace
{
    [JsonPropertyName("gX")] public int GX { get; set; }
    [JsonPropertyName("gY")] public int GY { get; set; }
    public WallDir Dir { get; set; }
}

public enum ShipType
{
    Human = 0,
    Alien = 1,
}

public enum OfficerRank
{
    Ensign = 0,
    Lieutenant = 1,
    Commander = 2,
    Captain = 3,
}

public enum MissionType
{
    DestroyShip = 0,
    CaptureOfficer = 1,
    SabotageReactor = 2,
    RetrieveData = 3,
    Rescue = 4,
}

public class RoomData
{
    public int X { get; set; }
    public int Y { get; set; }
    public int Width { get; set; } = 3;
    public int Height { get; set; } = 3;
    public RoomType Type { get; set; } = RoomType.Corridor;
    public bool LightOn { get; set; } = true;
    public int ShipIdx { get; set; } = -1;
    public int SubsystemHp { get; set; }
    public int SubsystemHpMax { get; set; }

    public int CenterX => X + Width / 2;
    public int CenterY => Y + Height / 2;

    public bool Contains(int gx, int gy) =>
        gx >= X && gx < X + Width && gy >= Y && gy < Y + Height;
}

public class EntityData
{
    [JsonPropertyName("gX")] public int GX { get; set; }
    [JsonPropertyName("gY")] public int GY { get; set; }
    public EnemyType EnemyType { get; set; }
    public string Name { get; set; } = "";
}

public class ConsoleData
{
    [JsonPropertyName("gX")] public int GX { get; set; }
    [JsonPropertyName("gY")] public int GY { get; set; }
    public RoomType RoomType { get; set; }
}

public class LootData
{
    [JsonPropertyName("gX")] public int GX { get; set; }
    [JsonPropertyName("gY")] public int GY { get; set; }
}

public class OfficerData
{
    [JsonPropertyName("gX")] public int GX { get; set; }
    [JsonPropertyName("gY")] public int GY { get; set; }
    public string Name { get; set; } = "OFFICER";
    public OfficerRank Rank { get; set; } = OfficerRank.Ensign;
    public int RoomIdx { get; set; } = -1;
    public EnemyType CombatType { get; set; } = EnemyType.Brute;
}

public class NpcData
{
    [JsonPropertyName("gX")] public int GX { get; set; }
    [JsonPropertyName("gY")] public int GY { get; set; }
    public string Name { get; set; } = "NPC";
    public int DialogId { get; set; }
}

public class MissionData
{
    public MissionType Type { get; set; } = MissionType.DestroyShip;
    public int TargetOfficer { get; set; } = -1;
    public string Description { get; set; } = "";
}

public class FloorData
{
    public int Width { get; set; } = 20;
    public int Height { get; set; } = 20;
    public int[,] Map { get; set; } = new int[81, 81]; // 1-indexed, max 80x80
    public int SpawnGX { get; set; } = 3;
    public int SpawnGY { get; set; } = 10;
    public int StairsGX { get; set; } = -1;
    public int StairsGY { get; set; } = -1;
    public int StairsDir { get; set; } = 1;
    public bool HasUp { get; set; }
    public int DownGX { get; set; } = -1;
    public int DownGY { get; set; } = -1;
    public int DownDir { get; set; } = 3;
    public bool HasDown { get; set; }
    public List<RoomData> Rooms { get; set; } = new();
    public List<EntityData> Enemies { get; set; } = new();
    public List<ConsoleData> Consoles { get; set; } = new();
    public List<LootData> Loot { get; set; } = new();
    public List<OfficerData> Officers { get; set; } = new();
    public List<NpcData> Npcs { get; set; } = new();
    public List<WindowFace> Windows { get; set; } = new();

    public bool HasWindow(int gx, int gy, WallDir dir) =>
        Windows.Any(w => w.GX == gx && w.GY == gy && w.Dir == dir);

    public void ToggleWindow(int gx, int gy, WallDir dir)
    {
        int idx = Windows.FindIndex(w => w.GX == gx && w.GY == gy && w.Dir == dir);
        if (idx >= 0)
            Windows.RemoveAt(idx);
        else
            Windows.Add(new WindowFace { GX = gx, GY = gy, Dir = dir });
    }

    public FloorData()
    {
        // Fill with walls
        for (int y = 0; y <= 20; y++)
            for (int x = 0; x <= 20; x++)
                Map[y, x] = 1;
    }

    public RoomData? RoomAt(int gx, int gy) =>
        Rooms.FirstOrDefault(r => r.Contains(gx, gy));

    public bool RoomOverlaps(int rx, int ry, int rw, int rh, RoomData? exclude = null)
    {
        foreach (var room in Rooms)
        {
            if (room == exclude) continue;
            // Check with 1-tile margin
            if (rx - 1 < room.X + room.Width &&
                rx + rw + 1 > room.X &&
                ry - 1 < room.Y + room.Height &&
                ry + rh + 1 > room.Y)
                return true;
        }
        return false;
    }
}

public class LevelData
{
    public List<FloorData> Floors { get; set; } = new() { new FloorData() };
    public string ShipName { get; set; } = "UNNAMED SHIP";
    public ShipType ShipType { get; set; } = ShipType.Human;
    public bool IsHub { get; set; }
    public int HullHp { get; set; } = 30;
    public int HullHpMax { get; set; } = 30;
    public List<MissionData> Missions { get; set; } = new();
}
