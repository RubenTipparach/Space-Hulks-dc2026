using System.Text.Json;
using System.Text.Json.Serialization;

namespace SpaceHulksLevelEditor;

/// <summary>
/// Snapshot-based undo/redo for floor edits.
/// Each snapshot is a JSON string of the FloorData.
/// </summary>
public class UndoManager
{
    private readonly Stack<string> _undoStack = new();
    private readonly Stack<string> _redoStack = new();
    private string? _current;

    private static readonly JsonSerializerOptions Opts = new()
    {
        Converters = { new JsonStringEnumConverter() },
    };

    public bool CanUndo => _undoStack.Count > 0;
    public bool CanRedo => _redoStack.Count > 0;

    /// <summary>Save current state before making a change.</summary>
    public void SaveState(FloorData floor)
    {
        string snap = Snapshot(floor);
        if (snap == _current) return; // no change
        if (_current != null)
            _undoStack.Push(_current);
        _current = snap;
        _redoStack.Clear();

        // Limit stack size
        if (_undoStack.Count > 200)
        {
            var temp = new Stack<string>(_undoStack.Reverse().Skip(50));
            _undoStack.Clear();
            foreach (var s in temp) _undoStack.Push(s);
        }
    }

    /// <summary>Undo: restore previous state.</summary>
    public FloorData? Undo(FloorData currentFloor)
    {
        if (_undoStack.Count == 0) return null;
        // Save current to redo
        _current ??= Snapshot(currentFloor);
        _redoStack.Push(_current);
        _current = _undoStack.Pop();
        return Restore(_current);
    }

    /// <summary>Redo: restore next state.</summary>
    public FloorData? Redo(FloorData currentFloor)
    {
        if (_redoStack.Count == 0) return null;
        _current ??= Snapshot(currentFloor);
        _undoStack.Push(_current);
        _current = _redoStack.Pop();
        return Restore(_current);
    }

    /// <summary>Clear all history (e.g. on floor switch).</summary>
    public void Clear()
    {
        _undoStack.Clear();
        _redoStack.Clear();
        _current = null;
    }

    /// <summary>Record initial state without pushing to undo stack.</summary>
    public void SetBaseline(FloorData floor)
    {
        _current = Snapshot(floor);
        _redoStack.Clear();
    }

    private static string Snapshot(FloorData f)
    {
        // Serialize floor to JSON (compact)
        var dto = new FloorDto
        {
            Width = f.Width, Height = f.Height,
            SpawnGX = f.SpawnGX, SpawnGY = f.SpawnGY,
            StairsGX = f.StairsGX, StairsGY = f.StairsGY, StairsDir = f.StairsDir,
            HasUp = f.HasUp,
            DownGX = f.DownGX, DownGY = f.DownGY, DownDir = f.DownDir,
            HasDown = f.HasDown,
        };
        // Copy map
        dto.Map = new int[f.Height][];
        for (int y = 0; y < f.Height; y++)
        {
            dto.Map[y] = new int[f.Width];
            for (int x = 0; x < f.Width; x++)
                dto.Map[y][x] = f.Map[y + 1, x + 1];
        }
        // Copy lists (serialize directly)
        dto.Rooms = f.Rooms.Select(r => new RoomData
        {
            X = r.X, Y = r.Y, Width = r.Width, Height = r.Height,
            Type = r.Type, LightOn = r.LightOn, ShipIdx = r.ShipIdx,
            SubsystemHp = r.SubsystemHp, SubsystemHpMax = r.SubsystemHpMax,
        }).ToList();
        dto.Enemies = f.Enemies.Select(e => new EntityData
        {
            GX = e.GX, GY = e.GY, EnemyType = e.EnemyType, Name = e.Name,
        }).ToList();
        dto.Consoles = f.Consoles.Select(c => new ConsoleData
        {
            GX = c.GX, GY = c.GY, RoomType = c.RoomType,
        }).ToList();
        dto.Loot = f.Loot.Select(l => new LootData { GX = l.GX, GY = l.GY }).ToList();
        dto.Officers = f.Officers.Select(o => new OfficerData
        {
            GX = o.GX, GY = o.GY, Name = o.Name, Rank = o.Rank,
            RoomIdx = o.RoomIdx, CombatType = o.CombatType,
        }).ToList();
        dto.Npcs = f.Npcs.Select(n => new NpcData
        {
            GX = n.GX, GY = n.GY, Name = n.Name, DialogId = n.DialogId,
        }).ToList();

        return JsonSerializer.Serialize(dto, Opts);
    }

    private static FloorData Restore(string json)
    {
        var dto = JsonSerializer.Deserialize<FloorDto>(json, Opts)!;
        var f = new FloorData
        {
            Width = dto.Width, Height = dto.Height,
            SpawnGX = dto.SpawnGX, SpawnGY = dto.SpawnGY,
            StairsGX = dto.StairsGX, StairsGY = dto.StairsGY, StairsDir = dto.StairsDir,
            HasUp = dto.HasUp,
            DownGX = dto.DownGX, DownGY = dto.DownGY, DownDir = dto.DownDir,
            HasDown = dto.HasDown,
            Rooms = dto.Rooms, Enemies = dto.Enemies,
            Consoles = dto.Consoles, Loot = dto.Loot,
            Officers = dto.Officers, Npcs = dto.Npcs,
        };
        for (int y = 0; y < dto.Height && y < dto.Map.Length; y++)
            for (int x = 0; x < dto.Width && x < dto.Map[y].Length; x++)
                f.Map[y + 1, x + 1] = dto.Map[y][x];
        return f;
    }

    // Minimal DTO for snapshot serialization
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
    }
}
