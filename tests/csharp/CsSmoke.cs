using Godot;

// C# parity smoke (GOALS G4.8): drives rivegd's ClassDB-registered API from
// C# — loader, metadata, node creation, typed inputs, data binding, signals.
public partial class CsSmoke : SceneTree
{
    private int _frames;
    private GodotObject _sprite;
    private int _propertyChanges;

    public override void _Initialize()
    {
        var file = ResourceLoader.Load("res://fixtures/data_binding_test.riv");
        if (file == null || file.GetClass() != "RiveFileResource")
        {
            Fail("loader did not return a RiveFileResource");
            return;
        }
        var artboards = (string[])file.Call("get_artboard_names");
        if (artboards.Length == 0)
        {
            Fail("no artboards");
            return;
        }

        // Untyped ClassDB access (no C# glue needed — same as any extension).
        _sprite = (GodotObject)ClassDB.Instantiate("RiveSprite2D");
        _sprite.Set("file", file);
        _sprite.Set("artboard", "artboard-1");
        ((Node)Root).AddChild((Node)_sprite);

        _sprite.Connect("property_changed",
            Callable.From((string path, Variant value) =>
            {
                _propertyChanges++;
                GD.Print($"C# property_changed: {path} = {value}");
            }));
        _sprite.Call("watch_property", "width");
        _sprite.Call("set_property", "width", 55.0);
        _sprite.Call("set_bool_input", "orient", true);
    }

    public override bool _Process(double delta)
    {
        _frames++;
        if (_frames == 90)
        {
            if (_propertyChanges == 0)
            {
                Fail("no property_changed signals reached C#");
                return true;
            }
            var width = (double)_sprite.Call("get_property", "width");
            if (System.Math.Abs(width - 55.0) > 0.01)
            {
                Fail($"get_property returned {width}, expected 55");
                return true;
            }
            GD.Print("CSHARP SMOKE OK");
            Quit(0);
        }
        return false;
    }

    private void Fail(string message)
    {
        GD.PushError("CSHARP SMOKE FAIL: " + message);
        Quit(1);
    }
}
