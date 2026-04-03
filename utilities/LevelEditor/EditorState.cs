namespace SpaceHulksLevelEditor;

public class EditorState
{
    public float HullPadding { get; set; } = .5f;
    public float HullCorner { get; set; } = 0.25f;
    public ShipType ShipType { get; set; } = ShipType.Human;
    public bool IsHub { get; set; }
}
