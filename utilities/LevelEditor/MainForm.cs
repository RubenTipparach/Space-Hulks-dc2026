using System.Runtime.InteropServices;

namespace SpaceHulksLevelEditor;

public class MainForm : Form
{
    [DllImport("kernel32.dll")] private static extern bool AllocConsole();
    private LevelData _level = new();
    private int _currentFloor;

    private readonly GridPanel _grid;
    private readonly Preview3DPanel _preview3D;
    private readonly EditorState _state = new();
    private bool _in3D;
    private readonly Label _modeLabel;
    private Button _btn2D = null!, _btn3D = null!;
    private readonly ListBox _floorList;
    private readonly Label _statusLabel;
    private Panel? _toolPanel;
    private Panel? _floorPanel;

    // Ship property controls (need refresh on load)
    private TextBox _shipNameBox = null!;
    private NumericUpDown _hullHpNum = null!, _hullHpMaxNum = null!;
    private ComboBox _shipTypeCombo = null!;
    private CheckBox _hubCheck = null!;

    // Mission editor controls
    private ListBox _missionList = null!;
    private ComboBox _missionTypeCombo = null!;
    private NumericUpDown _missionTargetNum = null!;
    private TextBox _missionDescBox = null!;
    private bool _updatingMission;

    public MainForm()
    {
        Text = "Space Hulks Level Editor";
        Size = new Size(1200, 780);
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Color.FromArgb(30, 30, 35);
        ForeColor = Color.White;

        // ── Left: Floor list ────────────────────────────
        _floorPanel = new Panel { Dock = DockStyle.Left, Width = 140, Padding = new Padding(4) };
        var floorPanel = _floorPanel;

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
            State = _state,
        };
        _grid.DataChanged += () => UpdateStatus();

        // ── Right: Tools ────────────────────────────────
        _toolPanel = new Panel { Dock = DockStyle.Right, Width = 220, Padding = new Padding(4), AutoScroll = true };
        var toolPanel = _toolPanel;

        var toolFlow = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown,
            WrapContents = false, AutoScroll = true,
        };

        // ── Ship Properties ─────────────────────────────
        toolFlow.Controls.Add(MakeLabel("SHIP PROPERTIES"));

        // Ship name
        toolFlow.Controls.Add(MakeLabel("Name:"));
        _shipNameBox = new TextBox
        {
            Width = 200, Text = "UNNAMED SHIP",
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White,
        };
        _shipNameBox.TextChanged += (_, _) => _level.ShipName = _shipNameBox.Text;
        toolFlow.Controls.Add(_shipNameBox);

        // Ship type
        _shipTypeCombo = new ComboBox
        {
            Width = 200, DropDownStyle = ComboBoxStyle.DropDownList,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White,
        };
        foreach (ShipType st in Enum.GetValues<ShipType>())
            _shipTypeCombo.Items.Add(st);
        _shipTypeCombo.SelectedIndex = 0;
        _shipTypeCombo.SelectedIndexChanged += (_, _) =>
        {
            if (_shipTypeCombo.SelectedItem is ShipType st)
            {
                _level.ShipType = st;
                _state.ShipType = st;
            }
        };
        toolFlow.Controls.Add(_shipTypeCombo);

        // Hull HP
        toolFlow.Controls.Add(MakeLabel("Hull HP / Max:"));
        var hullFlow = new FlowLayoutPanel { Width = 200, Height = 28, FlowDirection = FlowDirection.LeftToRight };
        _hullHpNum = new NumericUpDown { Minimum = 0, Maximum = 999, Value = 30, Width = 70,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White };
        _hullHpNum.ValueChanged += (_, _) => _level.HullHp = (int)_hullHpNum.Value;
        _hullHpMaxNum = new NumericUpDown { Minimum = 0, Maximum = 999, Value = 30, Width = 70,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White };
        _hullHpMaxNum.ValueChanged += (_, _) => _level.HullHpMax = (int)_hullHpMaxNum.Value;
        hullFlow.Controls.AddRange(new Control[] { _hullHpNum, _hullHpMaxNum });
        toolFlow.Controls.Add(hullFlow);

        // Hub checkbox
        _hubCheck = new CheckBox
        {
            Text = "Hub Ship", ForeColor = Color.White, AutoSize = true,
            Margin = new Padding(2, 4, 2, 2),
        };
        _hubCheck.CheckedChanged += (_, _) => { _level.IsHub = _hubCheck.Checked; _state.IsHub = _hubCheck.Checked; };
        toolFlow.Controls.Add(_hubCheck);

        // Show exterior toggle
        var extCheck = new CheckBox
        {
            Text = "Show Exterior", ForeColor = Color.White, AutoSize = true,
            Margin = new Padding(2, 4, 2, 2),
        };
        extCheck.CheckedChanged += (_, _) => _preview3D.ShowExterior = extCheck.Checked;
        toolFlow.Controls.Add(extCheck);

        // Hull padding
        toolFlow.Controls.Add(MakeLabel("Hull Padding:"));
        var hullPad = new NumericUpDown { Minimum = 0, Maximum = 10, Value = (decimal)_state.HullPadding, Width = 60,
            DecimalPlaces = 2, Increment = 0.05m,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White };
        toolFlow.Controls.Add(hullPad);

        // Hull corner chamfer
        toolFlow.Controls.Add(MakeLabel("Hull Corner:"));
        var hullCorner = new NumericUpDown { Minimum = 0, Maximum = 10, Value = (decimal)_state.HullCorner, Width = 60,
            DecimalPlaces = 2, Increment = 0.05m,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White };
        toolFlow.Controls.Add(hullCorner);

        // Wire up events after both controls exist (corner clamped to padding)
        hullPad.ValueChanged += (_, _) => {
            _state.HullPadding = (float)hullPad.Value;
            if (hullCorner.Value > hullPad.Value) hullCorner.Value = hullPad.Value;
            _grid.Invalidate();
        };
        hullCorner.ValueChanged += (_, _) => {
            if (hullCorner.Value > hullPad.Value) hullCorner.Value = hullPad.Value;
            _state.HullCorner = (float)hullCorner.Value;
            _grid.Invalidate();
        };

        // Show all floors
        var allFloorsCheck = new CheckBox
        {
            Text = "Show All Floors", ForeColor = Color.White, AutoSize = true,
            Margin = new Padding(2, 4, 2, 2),
        };
        allFloorsCheck.CheckedChanged += (_, _) => _preview3D.ShowAllFloors = allFloorsCheck.Checked;
        toolFlow.Controls.Add(allFloorsCheck);

        // ── Draw Tools ──────────────────────────────────
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

        // Officer placement
        var btnOfficer = MakeButton("Place Officer", 200);
        btnOfficer.Click += (_, _) => { _grid.Tool = EditTool.Officer; UpdateStatus(); };
        toolFlow.Controls.Add(btnOfficer);

        var rankCombo = new ComboBox
        {
            Width = 200, DropDownStyle = ComboBoxStyle.DropDownList,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White,
        };
        foreach (OfficerRank r in Enum.GetValues<OfficerRank>())
            rankCombo.Items.Add(r);
        rankCombo.SelectedIndex = 0;
        rankCombo.SelectedIndexChanged += (_, _) =>
        {
            if (rankCombo.SelectedItem is OfficerRank r) _grid.PlaceOfficerRank = r;
        };
        toolFlow.Controls.Add(rankCombo);

        // Officer combat type
        toolFlow.Controls.Add(MakeLabel("Combat Type:"));
        var combatTypeCombo = new ComboBox
        {
            Width = 200, DropDownStyle = ComboBoxStyle.DropDownList,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White,
        };
        foreach (EnemyType et in Enum.GetValues<EnemyType>())
            if (et != EnemyType.None) combatTypeCombo.Items.Add(et);
        combatTypeCombo.SelectedIndex = 1; // Brute default
        combatTypeCombo.SelectedIndexChanged += (_, _) =>
        {
            if (combatTypeCombo.SelectedItem is EnemyType et) _grid.PlaceOfficerCombatType = et;
        };
        toolFlow.Controls.Add(combatTypeCombo);

        // NPC placement
        var btnNpc = MakeButton("Place NPC", 200);
        btnNpc.Click += (_, _) => { _grid.Tool = EditTool.Npc; UpdateStatus(); };
        toolFlow.Controls.Add(btnNpc);

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

        // ── Missions ────────────────────────────────────
        toolFlow.Controls.Add(MakeLabel("MISSIONS"));

        _missionList = new ListBox
        {
            Width = 200, Height = 60,
            BackColor = Color.FromArgb(25, 25, 30), ForeColor = Color.White,
            BorderStyle = BorderStyle.FixedSingle,
        };
        _missionList.SelectedIndexChanged += (_, _) => RefreshMissionEditor();
        toolFlow.Controls.Add(_missionList);

        var missionBtnFlow = new FlowLayoutPanel { Width = 200, Height = 28, FlowDirection = FlowDirection.LeftToRight };
        var btnAddMission = MakeButton("Add", 95);
        btnAddMission.Click += (_, _) => AddMission();
        var btnRemMission = MakeButton("Remove", 95);
        btnRemMission.Click += (_, _) => RemoveMission();
        missionBtnFlow.Controls.AddRange(new Control[] { btnAddMission, btnRemMission });
        toolFlow.Controls.Add(missionBtnFlow);

        toolFlow.Controls.Add(MakeLabel("Type:"));
        _missionTypeCombo = new ComboBox
        {
            Width = 200, DropDownStyle = ComboBoxStyle.DropDownList,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White,
        };
        foreach (MissionType mt in Enum.GetValues<MissionType>())
            _missionTypeCombo.Items.Add(mt);
        _missionTypeCombo.SelectedIndex = 0;
        _missionTypeCombo.SelectedIndexChanged += (_, _) => UpdateSelectedMission();
        toolFlow.Controls.Add(_missionTypeCombo);

        toolFlow.Controls.Add(MakeLabel("Target Officer:"));
        _missionTargetNum = new NumericUpDown { Minimum = -1, Maximum = 16, Value = -1, Width = 80,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White };
        _missionTargetNum.ValueChanged += (_, _) => UpdateSelectedMission();
        toolFlow.Controls.Add(_missionTargetNum);

        toolFlow.Controls.Add(MakeLabel("Description:"));
        _missionDescBox = new TextBox
        {
            Width = 200,
            BackColor = Color.FromArgb(40, 40, 50), ForeColor = Color.White,
        };
        _missionDescBox.TextChanged += (_, _) => UpdateSelectedMission();
        toolFlow.Controls.Add(_missionDescBox);

        toolPanel.Controls.Add(toolFlow);

        // ── Menu bar ────────────────────────────────────
        var menu = new MenuStrip();
        var fileMenu = new ToolStripMenuItem("File");
        fileMenu.DropDownItems.Add("New", null, (_, _) => NewLevel());
        fileMenu.DropDownItems.Add("Open...", null, (_, _) => OpenLevel());
        fileMenu.DropDownItems.Add("Save...", null, (_, _) => SaveLevel());
        fileMenu.DropDownItems.Add(new ToolStripSeparator());
        fileMenu.DropDownItems.Add("Open Hub Ship", null, (_, _) => QuickLoad("hub.json"));
        fileMenu.DropDownItems.Add("Open Enemy Ship", null, (_, _) => QuickLoad("sample_enemy_ship.json"));
        fileMenu.DropDownItems.Add(new ToolStripSeparator());
        fileMenu.DropDownItems.Add("Export Hub Ship", null, (_, _) => QuickSave("hub.json"));
        fileMenu.DropDownItems.Add("Export Enemy Ship", null, (_, _) => QuickSave("sample_enemy_ship.json"));
        fileMenu.DropDownItems.Add(new ToolStripSeparator());
        fileMenu.DropDownItems.Add("Exit", null, (_, _) => Close());
        menu.Items.Add(fileMenu);

        var editMenu = new ToolStripMenuItem("Edit");
        editMenu.DropDownItems.Add("Undo\tCtrl+Z", null, (_, _) => DoUndo());
        editMenu.DropDownItems.Add("Redo\tCtrl+Shift+Z", null, (_, _) => DoRedo());
        menu.Items.Add(editMenu);

        var genMenu = new ToolStripMenuItem("Generate");
        genMenu.DropDownItems.Add("Generate Floor", null, (_, _) => GenerateFloor());
        genMenu.DropDownItems.Add(new ToolStripSeparator());
        genMenu.DropDownItems.Add("Small Ship 20x20 (1 floor)", null, (_, _) => GenerateShip(20, 1));
        genMenu.DropDownItems.Add("Medium Ship 40x40 (2 floors)", null, (_, _) => GenerateShip(40, 2));
        genMenu.DropDownItems.Add("Large Ship 80x80 (3 floors)", null, (_, _) => GenerateShip(80, 3));
        menu.Items.Add(genMenu);

        var viewMenu = new ToolStripMenuItem("View");
        var extMenuItem = new ToolStripMenuItem("Show Exterior") { CheckOnClick = true };
        extMenuItem.CheckedChanged += (_, _) =>
        {
            _preview3D.ShowExterior = extMenuItem.Checked;
            extCheck.Checked = extMenuItem.Checked;
        };
        viewMenu.DropDownItems.Add(extMenuItem);
        menu.Items.Add(viewMenu);

        MainMenuStrip = menu;
        Controls.Add(menu);

        // ── 3D Preview panel ─────────────────────────────
        _preview3D = new Preview3DPanel { Dock = DockStyle.Fill, State = _state };

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

        // ── 2D / 3D toggle buttons ─────────────────────
        var modeBar = new Panel { Dock = DockStyle.Top, Height = 32, BackColor = Color.FromArgb(25, 25, 35) };
        var btn2D = new Button
        {
            Text = "2D EDITOR", Width = 120, Height = 28, Location = new Point(4, 2),
            FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(60, 130, 180), ForeColor = Color.White,
            Font = new Font("Consolas", 10, FontStyle.Bold),
        };
        var btn3D = new Button
        {
            Text = "3D PREVIEW", Width = 120, Height = 28, Location = new Point(128, 2),
            FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(50, 50, 60), ForeColor = Color.White,
            Font = new Font("Consolas", 10, FontStyle.Bold),
        };
        btn2D.Click += (_, _) => Toggle3D(false);
        btn3D.Click += (_, _) => Toggle3D(true);
        var btnAllFloors = new Button
        {
            Text = "ALL FLOORS", Width = 120, Height = 28, Location = new Point(256, 2),
            FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(50, 50, 60), ForeColor = Color.White,
            Font = new Font("Consolas", 10, FontStyle.Bold),
        };
        btnAllFloors.Click += (_, _) =>
        {
            _preview3D.ShowAllFloors = !_preview3D.ShowAllFloors;
            btnAllFloors.BackColor = _preview3D.ShowAllFloors ? Color.FromArgb(60, 130, 180) : Color.FromArgb(50, 50, 60);
            if (!_in3D) Toggle3D(true);
        };
        var btnWireframe = new Button
        {
            Text = "WIREFRAME", Width = 110, Height = 28, Location = new Point(384, 2),
            FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(60, 130, 180), ForeColor = Color.White,
            Font = new Font("Consolas", 9, FontStyle.Bold),
        };
        btnWireframe.Click += (_, _) =>
        {
            _preview3D.ShowWireframe = !_preview3D.ShowWireframe;
            btnWireframe.BackColor = _preview3D.ShowWireframe ? Color.FromArgb(60, 130, 180) : Color.FromArgb(50, 50, 60);
        };
        var btnGhostFloors = new Button
        {
            Text = "GHOST", Width = 80, Height = 28, Location = new Point(498, 2),
            FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(50, 50, 60), ForeColor = Color.White,
            Font = new Font("Consolas", 9, FontStyle.Bold),
        };
        btnGhostFloors.Click += (_, _) =>
        {
            _preview3D.ShowGhostFloors = !_preview3D.ShowGhostFloors;
            btnGhostFloors.BackColor = _preview3D.ShowGhostFloors ? Color.FromArgb(60, 130, 180) : Color.FromArgb(50, 50, 60);
        };
        var btnRoof = new Button
        {
            Text = "ROOF", Width = 70, Height = 28, Location = new Point(582, 2),
            FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(50, 50, 60), ForeColor = Color.White,
            Font = new Font("Consolas", 9, FontStyle.Bold),
        };
        btnRoof.Click += (_, _) =>
        {
            _preview3D.ShowRoof = !_preview3D.ShowRoof;
            btnRoof.BackColor = _preview3D.ShowRoof ? Color.FromArgb(60, 130, 180) : Color.FromArgb(50, 50, 60);
            if (_preview3D.ShowRoof)
            {
                AllocConsole();
                _preview3D._roofDebug = true;
                _preview3D.Refresh();
                _preview3D._roofDebug = false;
            }
        };
        _modeLabel = new Label
        {
            Text = "F5", AutoSize = true, Location = new Point(660, 7),
            ForeColor = Color.Gray, Font = new Font("Consolas", 8),
        };
        modeBar.Controls.AddRange(new Control[] { btn2D, btn3D, btnAllFloors, btnWireframe, btnGhostFloors, btnRoof, _modeLabel });

        // Store button refs for highlight updates
        _btn2D = btn2D; _btn3D = btn3D;

        // ── Center panels (grid + preview, only one visible) ──
        _grid.Dock = DockStyle.Fill;
        _preview3D.Dock = DockStyle.Fill;
        _preview3D.Visible = false;

        // ── Layout ──────────────────────────────────────
        Controls.Add(_grid);
        Controls.Add(_preview3D);
        Controls.Add(modeBar);
        Controls.Add(toolPanel);
        Controls.Add(floorPanel);

        // Initialize
        NewLevel();

        // F5 hotkey toggles 2D/3D
        KeyPreview = true;
        KeyDown += (_, e) =>
        {
            if (e.KeyCode == Keys.F5)
            {
                Toggle3D();
                e.Handled = true;
            }
            else if (e.Control && e.Shift && e.KeyCode == Keys.Z)
            {
                DoRedo();
                e.Handled = true;
            }
            else if (e.Control && e.KeyCode == Keys.Z)
            {
                DoUndo();
                e.Handled = true;
            }
        };
    }

    public void Enter3DPreview() => Toggle3D(true);
    public void Exit3DPreview() => Toggle3D(false);

    private void Toggle3D(bool? force = null)
    {
        _in3D = force ?? !_in3D;
        if (_in3D)
        {
            _preview3D.Floor = _grid.Floor;
            _preview3D.Level = _level;
            _preview3D.StartPreview();
            _grid.Visible = false;
            _preview3D.Visible = true;
        }
        else
        {
            _preview3D.StopPreview();
            _preview3D.Visible = false;
            _grid.Visible = true;
            _grid.Invalidate();
            UpdateStatus();
        }
        _btn2D.BackColor = _in3D ? Color.FromArgb(50, 50, 60) : Color.FromArgb(60, 130, 180);
        _btn3D.BackColor = _in3D ? Color.FromArgb(60, 130, 180) : Color.FromArgb(50, 50, 60);
    }

    // ── Mission editor ──────────────────────────────────

    private void RefreshMissionList()
    {
        _missionList.Items.Clear();
        foreach (var m in _level.Missions)
            _missionList.Items.Add($"{m.Type}: {(m.Description.Length > 20 ? m.Description[..20] + "..." : m.Description)}");
    }

    private void RefreshMissionEditor()
    {
        if (_missionList.SelectedIndex < 0 || _missionList.SelectedIndex >= _level.Missions.Count)
            return;
        _updatingMission = true;
        var m = _level.Missions[_missionList.SelectedIndex];
        _missionTypeCombo.SelectedItem = m.Type;
        _missionTargetNum.Value = m.TargetOfficer;
        _missionDescBox.Text = m.Description;
        _updatingMission = false;
    }

    private void UpdateSelectedMission()
    {
        if (_updatingMission) return;
        if (_missionList.SelectedIndex < 0 || _missionList.SelectedIndex >= _level.Missions.Count)
            return;
        var m = _level.Missions[_missionList.SelectedIndex];
        if (_missionTypeCombo.SelectedItem is MissionType mt) m.Type = mt;
        m.TargetOfficer = (int)_missionTargetNum.Value;
        m.Description = _missionDescBox.Text;
        RefreshMissionList();
    }

    private void AddMission()
    {
        _level.Missions.Add(new MissionData { Description = "New mission" });
        RefreshMissionList();
        _missionList.SelectedIndex = _level.Missions.Count - 1;
    }

    private void RemoveMission()
    {
        int idx = _missionList.SelectedIndex;
        if (idx < 0 || idx >= _level.Missions.Count) return;
        _level.Missions.RemoveAt(idx);
        RefreshMissionList();
    }

    // ── Level helpers ───────────────────────────────────

    private void SyncControlsFromLevel()
    {
        _shipNameBox.Text = _level.ShipName;
        _shipTypeCombo.SelectedItem = _level.ShipType;
        _hubCheck.Checked = _level.IsHub;
        _hullHpNum.Value = Math.Clamp(_level.HullHp, 0, 999);
        _hullHpMaxNum.Value = Math.Clamp(_level.HullHpMax, 0, 999);
        _state.ShipType = _level.ShipType;
        _state.IsHub = _level.IsHub;
        RefreshMissionList();
    }

    private void NewLevel()
    {
        _level = new LevelData();
        _currentFloor = 0;
        SyncControlsFromLevel();
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
        _grid.Undo.Clear();
        _grid.Undo.SetBaseline(_level.Floors[idx]);
        _grid.Invalidate();
        _preview3D.Floor = _level.Floors[idx];
        _preview3D.Level = _level;
        _preview3D.CurrentFloorIndex = idx;
        if (_in3D)
            _preview3D.StartPreview();
        UpdateStatus();
    }

    private void DoUndo()
    {
        if (_grid.Floor == null || !_grid.Undo.CanUndo) return;
        var restored = _grid.Undo.Undo(_grid.Floor);
        if (restored == null) return;
        _level.Floors[_currentFloor] = restored;
        _grid.Floor = restored;
        _grid.Invalidate();
        _preview3D.Floor = restored;
        UpdateStatus();
    }

    private void DoRedo()
    {
        if (_grid.Floor == null || !_grid.Undo.CanRedo) return;
        var restored = _grid.Undo.Redo(_grid.Floor);
        if (restored == null) return;
        _level.Floors[_currentFloor] = restored;
        _grid.Floor = restored;
        _grid.Invalidate();
        _preview3D.Floor = restored;
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

    private static int _genSeed = Environment.TickCount;
    private void GenerateShip(int gridSize = 20, int floors = 1)
    {
        int baseSeed = _genSeed++;
        _level = new LevelData();
        _level.Floors.Clear();
        for (int i = 0; i < floors; i++)
        {
            bool hasDown = i > 0;
            bool hasUp = i < floors - 1;
            _level.Floors.Add(LevelGenerator.Generate(i, hasDown, hasUp, baseSeed + i * 777, gridSize, gridSize));
        }
        _currentFloor = 0;
        SyncControlsFromLevel();
        RefreshFloorList();
    }

    private string? FindLevelsDir()
    {
        // Walk up from exe dir to find levels/ folder
        var dir = AppDomain.CurrentDomain.BaseDirectory;
        for (int i = 0; i < 8; i++)
        {
            var candidate = Path.Combine(dir, "levels");
            if (Directory.Exists(candidate)) return candidate;
            var parent = Directory.GetParent(dir);
            if (parent == null) break;
            dir = parent.FullName;
        }
        return null;
    }

    private void QuickLoad(string filename)
    {
        var levelsDir = FindLevelsDir();
        if (levelsDir == null)
        {
            MessageBox.Show("Could not find levels/ directory. Use Open... instead.", "Error");
            return;
        }
        var path = Path.Combine(levelsDir, filename);
        if (!File.Exists(path))
        {
            MessageBox.Show($"File not found: {path}", "Error");
            return;
        }
        try
        {
            Console.WriteLine($"[QuickLoad] Loading {path}...");
            _level = LevelSerializer.Load(path);
            Console.WriteLine($"[QuickLoad] Success: {_level.ShipName}, {_level.Floors.Count} floors");
            _currentFloor = 0;
            SyncControlsFromLevel();
            RefreshFloorList();
            _statusLabel.Text = $"Loaded {path}";
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[QuickLoad] ERROR: {ex}");
            MessageBox.Show($"Error loading {filename}:\n\n{ex.Message}\n\n{ex.StackTrace}", "Load Error");
        }
    }

    private void QuickSave(string filename)
    {
        var levelsDir = FindLevelsDir();
        if (levelsDir == null)
        {
            MessageBox.Show("Could not find levels/ directory. Use Save... instead.", "Error");
            return;
        }
        var path = Path.Combine(levelsDir, filename);
        try
        {
            LevelSerializer.Save(_level, path);
            _statusLabel.Text = $"Exported to {path}";
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Error saving: {ex.Message}", "Error");
        }
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
                SyncControlsFromLevel();
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
                           $"Officers: {f.Officers.Count} | NPCs: {f.Npcs.Count} | " +
                           $"Open: {openCells}";
    }

    private static Button MakeButton(string text, int width) => new()
    {
        Text = text, Width = width, Height = 24,
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
