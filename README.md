# FriedEditor

The scene editor for [Fried Engine](https://github.com/KoyFC/FriedEngine). See [`ROADMAP.md`](ROADMAP.md) for the plan and what is done.

## Layout

- `core/` is all of the editor's logic, with no UI toolkit in it: the standard library and the vendored `nlohmann/json` only. Nothing under it may include a UI header, and configuring fails if something does.
- `ui/imgui/` is the Dear ImGui backend (SDL2 and OpenGL 3), with a dark theme and the engine's own font. It is the editor that `fried_editor` starts.
- `ui/qt/` is the Qt 6 backend, kept as `fried_editor_qt`. A backend draws, forwards input and holds no editor state beyond its widgets, so replacing the toolkit means writing another backend next to these. The Qt one is no longer extended: it has everything up to the asset browser and gizmos.
- `app_imgui/` and `app/` are the executables that start each backend.
- `tests/` runs the core with doctest, and builds without any toolkit installed.
- `third_party/` holds `nlohmann/json`, `doctest`, Dear ImGui 1.91.9 (docking) and `stb_image`, unchanged.

## Building

```
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Each backend is built when its libraries are found (SDL2 and OpenGL for ImGui, Qt 6 Widgets for Qt) and the core and its tests are always built. `-DFRIED_EDITOR_QT=OFF` and `-DFRIED_EDITOR_IMGUI=OFF` leave one out on purpose.

## Running

The editor opens a project's folder, the one with `project.fried`, and finds the engine in its `engine/` folder or next to it (the engine's own `sandbox/` works as is). Otherwise pass `--engine <path>`.

```
build/app_imgui/fried_editor ~/Documentos/GitHub/FriedEngine/sandbox
build/app_imgui/fried_editor <project> --engine <engine>
```

The first run builds the editor player from the engine, which needs Haxe, hxcpp and CMake as building a game does, and takes a minute. The log goes to `fried-editor-player-build.log` in the temporary directory. File, Rebuild player does it again after the engine changes.

The viewport shows the scene without running it. Left click selects and drags an object, the wheel zooms, and the right or middle button pans. The hierarchy adds, deletes and reorders objects and picks the scene's camera, the inspector edits every field of the selected object, and Ctrl+Z and Ctrl+Y undo and redo. The layout of the panels is docked and is remembered in `~/.config/fried-editor/layout.ini`. The toolbar's Move, Rotate and Scale (W, E and R) put handles on the selected object, and dragging one is a single undo step. The Assets dock lists the images, fonts, sounds and scenes of the project and of the engine: drag an image onto the view for a new Sprite, onto a Sprite to change its texture, or a font for a new Text; a double click drops it in the middle. The Issues tab checks the project's assets against the platform being previewed: textures over 1024 pixels on the 3DS and DS, DS audio that is not mono PCM WAV, audio on the fx-CG50 and a project that does not fit its 2 MB. An issue with a fix has a button, and the file as it was is kept in `.fried-editor/originals/` in the project. A build, of the player or of the game, opens a window with its step, a bar and the end of its log, and a failed one stays open with the error until it is closed. The scene that was open is remembered for each project, in `~/.config/fried-editor/state.json`, and opened again next time. Play (F5, or the green button in the toolbar) saves the scene, builds the project's game for the PC in its `build/` folder, which needs the same tools as the Project Manager's build, and runs it. The button becomes Stop while the game builds or runs, and the game's log is `fried-editor-game.log` in the temporary directory. File, Open scene switches between the project's scene files, and `--scene <file>` opens one at start. A project that has an `editor.hxml` beside its `build.hxml` gets a player built from it, so its own components show in the viewport and in the inspector, and an object field is a list of the scene's objects. The player is built again when the engine's or the project's sources are newer than it. Ctrl+S saves the scene to `assets/scenes/` in the project. The viewport draws at the size of its panel, so nothing is scaled, and the white frame is the area the game shows on the selected platform: the display's size divided by the game camera's zoom there. The platform selector previews that platform's screen: the camera area follows the project's display mode on it, and the Issues tab checks the assets for it.

`FRIED_ENGINE=<engine> build/tests/core_tests` also runs the tests that start the real player.
