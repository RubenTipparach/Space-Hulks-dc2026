namespace SpaceHulksLevelEditor;

public class MainForm : Form
{
    private LevelData _level = new();
    private int _currentFloor;

    private readonly GridPanel _grid;
    private readonly ListBox _floorList;
    private readonly Label _statusLabel;

    public MainForm()
    {
        Text = "Space Hulks Level Editor";
        Size = new Size(1200, 780);
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Color.FromArgb(30, 30, 35);
        ForeColor = Color.White;

        // ── Left: Floor list ────────────────────────────
        var floorPanel = new Panel { Dock = DockStyle.Left, Width = 140, Padding = new Padding(4) };

        var floorLabel = new Label { Text = "FLOORS", Dock = DockStyle.Top, Height = 20, ForeColor = Color.Cyan };
        _floorList = new ListBox
        {
            Dock = DockStyle.Fill, BackColor = Color.FromArgb(25, 25, 30),
            ForeColor = Color.White, BorderStyle = BorderStyle.FixedSingle,
        };
        _floorList.SelectedIndexChanged += (_, _) => SelectFloor(_floorList.SelectedIndex);

        var floorBtnPanel = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 80, FlowDirection = FlowDirection.LeftToRight };
        var btnAddFloor = MakeButton("Add", 60);
        btnAddFloor.Click += (_, _) => AddFloor();
        var btnRemFloor = MakeButton("Remove", 60);
        btnRemFloor.Click += (_, _) => RemoveFloor();
        var btnUpFloor = MakeButton("Up", 30);
        btnUpFloor.Click += (_, _) => MoveFloor(-1);
        var btnDownFloor = MakeButton("Down", 42);
        btnDownFloor.Click += (_, _) => MoveFloor(1);
        floorBtnPanel.Controls.AddRange(new Control[] { btnAddFloor, btnRemFloor, btnUpFloor, btnDownFloor });

        floorPanel.Controls.Add(_floorList);
        floorPanel.Controls.Add(floorBtnPanel);
        floorPanel.Controls.Add(floorLabel);

        // ── Center: Grid ────────────────────────────────
        _grid = new GridPanel
        {
            Dock = DockStyle.Fill,
            AutoScroll = true,
        };
        _grid.DataChanged += () => UpdateStatus();

        // ── Right: Tools ────────────────────────────────
        var toolPanel = new Panel { Dock = DockStyle.Right, Width = 220, Padding = new Padding(4), AutoScroll = true };

        var toolFlow = new FlowLayoutPanel
        {
            Dock = DockStyle.Top, Height = 540, FlowDirection = FlowDirection.TopDown,
            WrapContents = false, AutoSize = false,
        };

        // Tool group label
        toolFlow.Controls.Add(MakeLabel("DRAW TOOLS"));
        var toolButtons = new (string label, EditTool tool)[]
        {
            ("Corridor", EditTool.Corridor),
            ("Wall", EditTool.Wall),
            ("Window", EditTool.Window),
            ("Eraser", EditTool.Eraser),
        };
        foreach (var (label, tool) in toolButtons)
        {
            var btn = MakeButton(label, 200);
            btn.Click += (_, _) => { _grid.Tool = tool; UpdateStatus(); };
            toolFlow.Controls.Add(btn);
        }

        toolFlow.Controls.Add(MakeLabel("ROOM TOOLS"));
        var btnRoom = MakeButton("Place Room", 200);
        btnRoom.Click += (_, _) => { _grid.Tool = EditTool.Room; UpdateStatus(); };
        toolFlow.Controls.Add(btnRoom);

        // Room type combo
        var roomTypeCombo = new ComboBox
        {
            Width = 200, DropDownStyle = ComboBoxStyle.DropDownList,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White,
        };
        foreach (RoomType rt in Enum.GetValues<RoomType>())
            if (rt != RoomType.Corridor) roomTypeCombo.Items.Add(rt);
        roomTypeCombo.SelectedIndex = 0;
        roomTypeCombo.SelectedIndexChanged += (_, _) =>
        {
            if (roomTypeCombo.SelectedItem is RoomType rt)
            {
                _grid.PlaceRoomType = rt;
                _grid.PlaceConsoleType = rt;
            }
        };
        toolFlow.Controls.Add(roomTypeCombo);

        // Room size
        toolFlow.Controls.Add(MakeLabel("Room W:"));
        var roomW = new NumericUpDown { Minimum = 2, Maximum = 8, Value = 3, Width = 60,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White };
        roomW.ValueChanged += (_, _) => _grid.RoomBrushW = (int)roomW.Value;
        toolFlow.Controls.Add(roomW);

        toolFlow.Controls.Add(MakeLabel("Room H:"));
        var roomH = new NumericUpDown { Minimum = 2, Maximum = 8, Value = 3, Width = 60,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White };
        roomH.ValueChanged += (_, _) => _grid.RoomBrushH = (int)roomH.Value;
        toolFlow.Controls.Add(roomH);

        var btnDelRoom = MakeButton("Delete Room (click)", 200);
        btnDelRoom.Click += (_, _) =>
        {
            MessageBox.Show("Right-click a room on the grid to delete it.", "Delete Room");
        };
        toolFlow.Controls.Add(btnDelRoom);

        toolFlow.Controls.Add(MakeLabel("ENTITIES"));

        // Enemy placement
        var btnEnemy = MakeButton("Place Enemy", 200);
        btnEnemy.Click += (_, _) => { _grid.Tool = EditTool.Enemy; UpdateStatus(); };
        toolFlow.Controls.Add(btnEnemy);

        var enemyCombo = new ComboBox
        {
            Width = 200, DropDownStyle = ComboBoxStyle.DropDownList,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White,
        };
        foreach (EnemyType et in Enum.GetValues<EnemyType>())
            if (et != EnemyType.None) enemyCombo.Items.Add(et);
        enemyCombo.SelectedIndex = 0;
        enemyCombo.SelectedIndexChanged += (_, _) =>
        {
            if (enemyCombo.SelectedItem is EnemyType et) _grid.PlaceEnemyType = et;
        };
        toolFlow.Controls.Add(enemyCombo);

        var btnConsole = MakeButton("Place Console", 200);
        btnConsole.Click += (_, _) => { _grid.Tool = EditTool.Console; UpdateStatus(); };
        toolFlow.Controls.Add(btnConsole);

        var btnLoot = MakeButton("Place Loot", 200);
        btnLoot.Click += (_, _) => { _grid.Tool = EditTool.Loot; UpdateStatus(); };
        toolFlow.Controls.Add(btnLoot);

        toolFlow.Controls.Add(MakeLabel("PLACEMENT"));

        var btnSpawn = MakeButton("Set Spawn", 200);
        btnSpawn.Click += (_, _) => { _grid.Tool = EditTool.Spawn; UpdateStatus(); };
        toolFlow.Controls.Add(btnSpawn);

        var btnStairsUp = MakeButton("Place Stairs Up", 200);
        btnStairsUp.Click += (_, _) => { _grid.Tool = EditTool.StairsUp; UpdateStatus(); };
        toolFlow.Controls.Add(btnStairsUp);

        var btnStairsDown = MakeButton("Place Stairs Down", 200);
        btnStairsDown.Click += (_, _) => { _grid.Tool = EditTool.StairsDown; UpdateStatus(); };
        toolFlow.Controls.Add(btnStairsDown);

        // Stairs direction
        toolFlow.Controls.Add(MakeLabel("Stairs Dir:"));
        var dirCombo = new ComboBox
        {
            Width = 200, DropDownStyle = ComboBoxStyle.DropDownList,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White,
        };
        dirCombo.Items.AddRange(new object[] { "North (0)", "East (1)", "South (2)", "West (3)" });
        dirCombo.SelectedIndex = 1;
        dirCombo.SelectedIndexChanged += (_, _) => _grid.PlaceStairsDir = dirCombo.SelectedIndex;
        toolFlow.Controls.Add(dirCombo);

        toolPanel.Controls.Add(toolFlow);

        // ── Menu bar ────────────────────────────────────
        var menu = new MenuStrip();
        var fileMenu = new ToolStripMenuItem("File");
        fileMenu.DropDownItems.Add("New", null, (_, _) => NewLevel());
        fileMenu.DropDownItems.Add("Open...", null, (_, _) => OpenLevel());
        fileMenu.DropDownItems.Add("Save...", null, (_, _) => SaveLevel());
        fileMenu.DropDownItems.Add(new ToolStripSeparator());
        fileMenu.DropDownItems.Add("Exit", null, (_, _) => Close());
        menu.Items.Add(fileMenu);

        var genMenu = new ToolStripMenuItem("Generate");
        genMenu.DropDownItems.Add("Generate Floor", null, (_, _) => GenerateFloor());
        genMenu.DropDownItems.Add("Generate Ship (3 floors)", null, (_, _) => GenerateShip());
        menu.Items.Add(genMenu);

        MainMenuStrip = menu;
        Controls.Add(menu);

        // ── Status bar ──────────────────────────────────
        _statusLabel = new Label
        {
            Dock = DockStyle.Bottom, Height = 24,
            ForeColor = Color.FromArgb(150, 150, 150),
            BackColor = Color.FromArgb(25, 25, 30),
            TextAlign = ContentAlignment.MiddleLeft,
            Padding = new Padding(4, 0, 0, 0),
        };
        Controls.Add(_statusLabel);

        // ── Layout ──────────────────────────────────────
        Controls.Add(_grid);
        Controls.Add(toolPanel);
        Controls.Add(floorPanel);

        // Initialize
        NewLevel();
    }

    private void NewLevel()
    {
        _level = new LevelData();
        _currentFloor = 0;
        RefreshFloorList();
    }

    private void RefreshFloorList()
    {
        _floorList.Items.Clear();
        for (int i = 0; i < _level.Floors.Count; i++)
            _floorList.Items.Add($"Floor {i}");
        if (_currentFloor >= _level.Floors.Count)
            _currentFloor = _level.Floors.Count - 1;
        if (_currentFloor < 0) _currentFloor = 0;
        if (_floorList.Items.Count > 0)
            _floorList.SelectedIndex = _currentFloor;
        SelectFloor(_currentFloor);
    }

    private void SelectFloor(int idx)
    {
        if (idx < 0 || idx >= _level.Floors.Count) return;
        _currentFloor = idx;
        _grid.Floor = _level.Floors[idx];
        _grid.Invalidate();
        UpdateStatus();
    }

    private void AddFloor()
    {
        if (_level.Floors.Count >= 16) return;
        _level.Floors.Add(new FloorData());
        RefreshFloorList();
        _floorList.SelectedIndex = _level.Floors.Count - 1;
    }

    private void RemoveFloor()
    {
        if (_level.Floors.Count <= 1) return;
        _level.Floors.RemoveAt(_currentFloor);
        RefreshFloorList();
    }

    private void MoveFloor(int dir)
    {
        int newIdx = _currentFloor + dir;
        if (newIdx < 0 || newIdx >= _level.Floors.Count) return;
        (_level.Floors[_currentFloor], _level.Floors[newIdx]) =
            (_level.Floors[newIdx], _level.Floors[_currentFloor]);
        _currentFloor = newIdx;
        RefreshFloorList();
    }

    private void GenerateFloor()
    {
        bool hasDown = _currentFloor > 0;
        bool hasUp = _currentFloor < _level.Floors.Count - 1 || _level.Floors.Count < 16;
        var floor = LevelGenerator.Generate(_currentFloor, hasDown, hasUp);
        _level.Floors[_currentFloor] = floor;
        SelectFloor(_currentFloor);
        RefreshFloorList();
    }

    private void GenerateShip()
    {
        _level = new LevelData();
        _level.Floors.Clear();
        for (int i = 0; i < 3; i++)
        {
            bool hasDown = i > 0;
            bool hasUp = i < 2;
            _level.Floors.Add(LevelGenerator.Generate(i, hasDown, hasUp, 42 + i * 777));
        }
        _currentFloor = 0;
        RefreshFloorList();
    }

    private void OpenLevel()
    {
        using var dlg = new OpenFileDialog
        {
            Filter = "Space Hulks Level (*.json)|*.json|All Files|*.*",
            Title = "Open Level"
        };
        if (dlg.ShowDialog() == DialogResult.OK)
        {
            try
            {
                _level = LevelSerializer.Load(dlg.FileName);
                _currentFloor = 0;
                RefreshFloorList();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error loading: {ex.Message}", "Error");
            }
        }
    }

    private void SaveLevel()
    {
        using var dlg = new SaveFileDialog
        {
            Filter = "Space Hulks Level (*.json)|*.json",
            Title = "Save Level"
        };
        if (dlg.ShowDialog() == DialogResult.OK)
        {
            try
            {
                LevelSerializer.Save(_level, dlg.FileName);
                _statusLabel.Text = $"Saved to {dlg.FileName}";
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error saving: {ex.Message}", "Error");
            }
        }
    }

    private void UpdateStatus()
    {
        var f = _grid.Floor;
        if (f == null) return;
        int openCells = 0;
        for (int y = 1; y <= f.Height; y++)
            for (int x = 1; x <= f.Width; x++)
                if (f.Map[y, x] == 0) openCells++;
        _statusLabel.Text = $"Tool: {_grid.Tool} | Floor {_currentFloor} | " +
                           $"Rooms: {f.Rooms.Count} | Enemies: {f.Enemies.Count} | " +
                           $"Consoles: {f.Consoles.Count} | Loot: {f.Loot.Count} | " +
                           $"Open cells: {openCells}";
    }

    private static Button MakeButton(string text, int width) => new()
    {
        Text = text, Width = width, Height = 26,
        FlatStyle = FlatStyle.Flat,
        BackColor = Color.FromArgb(50, 50, 60),
        ForeColor = Color.White,
        Margin = new Padding(2),
    };

    private static Label MakeLabel(string text) => new()
    {
        Text = text, AutoSize = true,
        ForeColor = Color.FromArgb(100, 200, 220),
        Margin = new Padding(2, 8, 2, 2),
        Font = new Font("Consolas", 8, FontStyle.Bold),
    };
}
