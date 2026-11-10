# FriedEditor

The scene editor for [Fried Engine](https://github.com/KoyFC/FriedEngine). See [`ROADMAP.md`](ROADMAP.md) for the plan and what is done.

## Layout

- `core/` is all of the editor's logic, with no UI toolkit in it: the standard library and the vendored `nlohmann/json` only. Nothing under it may include a UI header, and configuring fails if something does.
- `ui/qt/` is the Qt 6 backend, the only code that includes Qt. A backend draws, forwards input and holds no editor state beyond its widgets, so replacing the toolkit means writing another backend next to it.
- `app/` is the executable that starts a backend.
- `tests/` runs the core with doctest, and builds without any toolkit installed.
- `third_party/` holds `nlohmann/json` and `doctest`, unchanged.

## Building

```
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Without Qt 6 Widgets installed, only the core and its tests are built. `-DFRIED_EDITOR_QT=OFF` does the same on purpose.

## Running

The editor opens a project's folder, the one with `project.fried`, and finds the engine in its `engine/` folder or next to it (the engine's own `sandbox/` works as is). Otherwise pass `--engine <path>`.

```
build/app/fried_editor ~/Documentos/GitHub/FriedEngine/sandbox
build/app/fried_editor <project> --engine <engine>
```

The first run builds the editor player from the engine, which needs Haxe, hxcpp and CMake as building a game does, and takes a minute. The log goes to `fried-editor-player-build.log` in the temporary directory. File, Rebuild player does it again after the engine changes.

The viewport shows the scene without running it. Left click selects and drags an object, the wheel zooms, and the right or middle button pans. The hierarchy adds, deletes and reorders objects and picks the scene's camera, the inspector edits every field of the selected object, and Ctrl+Z and Ctrl+Y undo and redo. Ctrl+S saves the scene to `assets/scenes/` in the project. The platform selector previews that platform's overrides.

`FRIED_ENGINE=<engine> build/tests/core_tests` also runs the tests that start the real player.
