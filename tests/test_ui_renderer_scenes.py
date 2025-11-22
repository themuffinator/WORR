import json
from pathlib import Path

DATA_PATH = Path(__file__).with_name("ui_renderer_scenes.json")
REQUIRED_PRIMITIVES = {"text", "icon", "fill_rect", "stroke_rect"}
REQUIRED_STATES = {"normal", "hover", "active", "disabled"}


def test_scene_catalog_exists():
    assert DATA_PATH.exists(), "Sample scene catalog is missing"


def test_sample_scenes_cover_primitives_states_and_scaling():
    payload = json.loads(DATA_PATH.read_text())
    scenes = payload.get("scenes", [])
    assert scenes, "Scene catalog must define at least one scene"

    primitives = set()
    states = set()
    dpi_scales = set()
    layout_scales = set()

    for scene in scenes:
        dpi_scale = scene.get("dpi_scale")
        layout_scale = scene.get("layout_scale")
        assert isinstance(dpi_scale, (int, float)), "Each scene needs a numeric dpi_scale"
        assert isinstance(layout_scale, (int, float)), "Each scene needs a numeric layout_scale"
        dpi_scales.add(float(dpi_scale))
        layout_scales.add(float(layout_scale))

        commands = scene.get("commands", [])
        assert commands, f"Scene '{scene.get('name')}' must include commands"
        for command in commands:
            primitive = command.get("primitive")
            state = command.get("state")
            assert primitive in REQUIRED_PRIMITIVES, f"Unexpected primitive '{primitive}'"
            assert state in REQUIRED_STATES, f"Unexpected state '{state}'"
            primitives.add(primitive)
            states.add(state)

            # Ensure sample metadata is present to aid manual validation.
            assert "note" in command, f"Command '{primitive}' in scene '{scene.get('name')}' should describe its intent"

    assert primitives == REQUIRED_PRIMITIVES, "All primitives must be represented in the sample scenes"
    assert states == REQUIRED_STATES, "All interaction states must be represented in the sample scenes"
    assert len(dpi_scales) >= 3, "Provide multiple DPI scales to verify scaling behavior"
    assert any(scale > 1.0 for scale in layout_scales), "Include a layout_scale above 1.0 to stress combined scaling"
