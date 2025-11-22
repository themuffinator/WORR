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
