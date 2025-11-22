# Menu manifest

The UI manifest at `menus/index.json` now describes one or more named stacks of menu scripts. Each entry is an ordered list so you can compose multi-file stacks instead of a single path.

```json
{
  "controller": "menus/controller.json",
  "main": ["worr.menu.json"],
  "ingame": ["worr.menu.json"],
  "overrides": {
    "main": ["custom.menu.json"]
  }
}
```

* Top-level keys other than `controller` and `overrides` define named stacks. Values can be a string or an array of strings.
* `overrides` mirrors the stack names and, when present, those entries are tried before the base stack.
* `controller` points to a JSON file that maps menu contexts to stack names, letting you control the load order per context.

A simple controller example (`menus/controller.json`):

```json
{
  "main": ["main"],
  "ingame": ["ingame", "main"]
}
```

If the in-game stack is missing, the loader automatically falls back to the `main` stack while retaining the built-in buffer fallback for safety.

Typography configuration can ride alongside the manifest/controller pair:

* The `fonts` block accepts either a string or an array for each typography role (`body`, `label`, `heading`, `monospace`). Entries are tried in order and may mix scalable TTF/OTF, `*.kfont` descriptions, and legacy bitmap assets such as `conchars.pcx`. The `ui_font` and `ui_font_fallback` cvars accept the same comma- or semicolon-delimited chains and prepend/append them to every role.
* `fontSizes` mirrors the role map and lets a manifest pin pixel heights per role. Values above zero override the default ramps (body/monospace base size, label scaled down slightly, heading scaled up slightly). You can also steer sizes through `ui_font_size` or the role-specific `ui_font_size_body`, `ui_font_size_label`, `ui_font_size_heading`, and `ui_font_size_monospace` cvars.
* Role-specific font cvars (`ui_font_body`, `ui_font_label`, `ui_font_heading`, `ui_font_monospace`) opt a role into its own chain without affecting the others. When a role supplies no fonts, the loader falls back to the body chain and finally the default renderer font.
