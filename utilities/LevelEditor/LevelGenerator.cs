namespace SpaceHulksLevelEditor;

public static class LevelGenerator
{
    private static Random _rng = new();

    public static FloorData Generate(int floorNum, bool hasDown, bool hasUp, int seed = -1, int gridW = 20, int gridH = 20)
    {
        if (seed >= 0) _rng = new Random(seed);
        else _rng = new Random();

        var f = new FloorData { Width = gridW, Height = gridH };
        int w = f.Width, h = f.Height;
        f.HasDown = hasDown;
        f.HasUp = hasUp;

        // Fill with walls
        for (int y = 1; y <= h; y++)
            for (int x = 1; x <= w; x++)
                f.Map[y, x] = 1;

        int midY = h / 2;

        // Central corridor (2 tiles wide)
        for (int x = 3; x <= w - 2; x++)
        {
            f.Map[midY, x] = 0;
            f.Map[midY + 1, x] = 0;
        }

        // Scale room count and size with grid
        int maxRooms = w <= 20 ? 8 : w <= 40 ? 14 : 22;
        int minRooms = w <= 20 ? 5 : w <= 40 ? 8 : 12;
        int numRooms = minRooms + _rng.Next(maxRooms - minRooms + 1);
        int roomMinW = w <= 20 ? 3 : w <= 40 ? 4 : 5;
        int roomMaxW = w <= 20 ? 4 : w <= 40 ? 6 : 8;

        int shipLeft = 3, shipRight = w - 2;
        int shipSpan = shipRight - shipLeft;
        int spacing = shipSpan / (numRooms + 1);
        if (spacing < roomMaxW + 2) spacing = roomMaxW + 2;

        // Build unique room type pool — mandatory first, then shuffled optional
        var typePool = new List<RoomType> { RoomType.Bridge, RoomType.Engines, RoomType.Weapons };
        var optional = new List<RoomType> {
            RoomType.Shields, RoomType.Reactor, RoomType.Medbay,
            RoomType.Cargo, RoomType.Barracks
        };
        // Shuffle optional
        for (int i = optional.Count - 1; i > 0; i--)
        {
            int j = _rng.Next(i + 1);
            (optional[i], optional[j]) = (optional[j], optional[i]);
        }
        typePool.AddRange(optional);
        if (numRooms > typePool.Count) numRooms = typePool.Count;
        int typeIdx = 0;

        for (int i = 0; i < numRooms; i++)
        {
            int rw = roomMinW + _rng.Next(roomMaxW - roomMinW + 1);
            int rh = roomMinW + _rng.Next(roomMaxW - roomMinW + 1);
            int rx = shipLeft + spacing * (i + 1) - rw / 2;
            if (rx < 2) rx = 2;
            if (rx + rw > w) rx = w - rw;

            int ry;
            if (i % 2 == 0)
            {
                ry = midY - rh - 1;
                if (ry < 2) ry = 2;
            }
            else
            {
                ry = midY + 2 + 1;
                if (ry + rh > h) ry = h - rh;
            }

            // Check overlap with existing rooms (1-tile margin)
            if (f.RoomOverlaps(rx, ry, rw, rh)) continue;

            var room = new RoomData
            {
                X = rx, Y = ry, Width = rw, Height = rh,
                Type = typePool[typeIdx++],
                LightOn = true,
            };
            f.Rooms.Add(room);

            // Carve room
            for (int py = ry; py < ry + rh; py++)
                for (int px = rx; px < rx + rw; px++)
                    if (py >= 1 && py <= h && px >= 1 && px <= w)
                        f.Map[py, px] = 0;

            // Connect to corridor
            int connX = rx + rw / 2;
            if (i % 2 == 0)
            {
                for (int y = ry + rh; y <= midY; y++)
                    if (y >= 1 && y <= h && connX >= 1 && connX <= w)
                        f.Map[y, connX] = 0;
            }
            else
            {
                for (int y = midY + 1; y < ry; y++)
                    if (y >= 1 && y <= h && connX >= 1 && connX <= w)
                        f.Map[y, connX] = 0;
            }

            // Place console at room center
            f.Consoles.Add(new ConsoleData
            {
                GX = room.CenterX, GY = room.CenterY, RoomType = room.Type
            });

            // Place 1-2 enemies per room
            int aliens = 1 + _rng.Next(2);
            int maxType = floorNum <= 1 ? 2 : floorNum <= 3 ? 3 : 4;
            for (int a = 0; a < aliens; a++)
            {
                int ax = rx + _rng.Next(rw);
                int ay = ry + _rng.Next(rh);
                if (ax == room.CenterX && ay == room.CenterY) continue;
                f.Enemies.Add(new EntityData
                {
                    GX = ax, GY = ay,
                    EnemyType = (EnemyType)(1 + _rng.Next(maxType)),
                    Name = GenerateAlienName()
                });
            }
        }

        // Windows: one per room on the outer wall facing away from corridor
        foreach (var room in f.Rooms)
        {
            int cx = room.CenterX;
            if (room.Y + room.Height <= midY)
            {
                // Room above corridor — N window on wall row above room
                int wy = room.Y - 1;
                if (wy >= 1 && wy <= h && cx >= 1 && cx <= w && f.Map[wy, cx] == 1)
                    f.Windows.Add(new WindowFace { GX = cx, GY = wy, Dir = WallDir.North });
            }
            else if (room.Y > midY + 1)
            {
                // Room below corridor — S window on wall row below room
                int wy = room.Y + room.Height;
                if (wy >= 1 && wy <= h && cx >= 1 && cx <= w && f.Map[wy, cx] == 1)
                    f.Windows.Add(new WindowFace { GX = cx, GY = wy, Dir = WallDir.South });
            }
        }

        // Spawn
        f.SpawnGX = shipLeft;
        f.SpawnGY = midY;

        // Up-stairs
        if (hasUp)
        {
            f.StairsGX = shipRight;
            f.StairsGY = midY;
            f.StairsDir = 1;
            if (f.StairsGX >= 1 && f.StairsGX <= w && f.StairsGY >= 1 && f.StairsGY <= h)
                f.Map[f.StairsGY, f.StairsGX] = 0;
        }

        // Down-stairs
        if (hasDown)
        {
            f.DownGX = shipLeft + 1;
            f.DownGY = midY;
            f.DownDir = 3;
            if (f.DownGX >= 1 && f.DownGX <= w && f.DownGY >= 1 && f.DownGY <= h)
                f.Map[f.DownGY, f.DownGX] = 0;
        }

        return f;
    }

    private static readonly string[] Prefixes = {
        "ZR", "KR", "VX", "GH", "SK", "BL", "TR", "NX", "QZ", "XL"
    };
    private static readonly string[] Suffixes = {
        "AAK", "IKS", "ULL", "ORM", "AXE", "ENT", "IRE", "OKK", "URG", "ASH"
    };

    private static string GenerateAlienName() =>
        $"{Prefixes[_rng.Next(Prefixes.Length)]}'{Suffixes[_rng.Next(Suffixes.Length)]}";
}
