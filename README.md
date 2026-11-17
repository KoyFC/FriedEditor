# FriedEditor

The scene editor for [Fried Engine](https://github.com/KoyFC/FriedEngine). See [`ROADMAP.md`](ROADMAP.md) for the plan and what is done.

## Layout

- `core/` is all of the editor's logic, with no UI toolkit in it: the standard library, `nlohmann/json` and FriedCore only. Nothing under it may include a UI header, and configuring fails if something does.
- `ui/imgui/` is the Dear ImGui backend (SDL2 and OpenGL 3), with a dark theme and the engine's own font. It is the editor that `fried_editor` starts.
- `ui/qt/` is the Qt 6 backend, kept as `fried_editor_qt`. It is not used and not kept up to date: every editor feature is built in the Dear ImGui backend only, and the Qt one stops at the asset browser and gizmos. It was last built before the scene became a tree of nodes. A backend draws, forwards input and holds no editor state beyond its widgets, so replacing the toolkit means writing another backend next to these.
- `app_imgui/` and `app/` are the executables that start each backend.
- `tests/` runs the core with doctest, and builds without any toolkit installed.
- `third_party/fried_core` is [FriedCore](https://github.com/KoyFC/FriedCore) as a submodule, the library shared with the Project Manager: the project file, the platforms, the steps of a build and the programs that run them, and images. It also brings `nlohmann/json` and `stb_image`. Clone with `--recursive`, or run `git submodule update --init` once.
- The rest of `third_party/` holds `doctest` and Dear ImGui 1.92.9 (docking, with one local change for evaluating typed numbers), unchanged.

## Building

```
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

The Dear ImGui backend is built when its libraries are found (SDL2 and OpenGL) and the core and its tests are always built. The Qt backend is off by default, since it no longer builds against the core, which now edits a tree of nodes: `-DFRIED_EDITOR_QT=ON` asks for it, and `-DFRIED_EDITOR_IMGUI=OFF` leaves the ImGui one out on purpose.

## Running

The editor opens a project's folder, the one with `project.fried`, and finds the engine in its `engine/` folder or next to it (the engine's own `sandbox/` works as is). Otherwise pass `--engine <path>`.

```
build/app_imgui/fried_editor ~/Documentos/GitHub/FriedEngine/sandbox
build/app_imgui/fried_editor <project> --engine <engine>
```

The first run builds the editor player from the engine, which needs Haxe, hxcpp and CMake as building a game does, and takes a minute. The log goes to `fried-editor-player-build.log` in the temporary directory. File, Rebuild player does it again after the engine changes.

The viewport shows the scene without running it. Left click selects and drags a node, the wheel zooms, and the right or middle button pans. The hierarchy is the scene's tree of nodes: it adds a node of any class under the selected one, deletes it, reorders it, drags it under another node or out of one, changes its type and picks the scene's camera, with a right click menu for each node. The inspector has one section for each class of the selected node's chain, its own class first and `Node` last, and a node dragged under a rotated or scaled parent moves by the amount in its own coordinates that follows the pointer, and Ctrl+Z and Ctrl+Y undo and redo. The layout of the panels is docked and is remembered in `~/.config/fried-editor/layout.ini`. The toolbar's Move, Rotate and Scale (W, E and R) put handles on the selected node, where it is in the world even when it is a child of another node, and dragging one is a single undo step. The Assets dock lists the images, fonts, sounds and scenes of the project and of the engine: drag an image onto the view for a new Sprite, onto a Sprite to change its texture, or a font for a new Label; a double click drops it in the middle. The Issues tab checks the project's assets against the platform being previewed: textures over 1024 pixels on the 3DS and DS, DS audio that is not mono PCM WAV, audio on the fx-CG50 and a project that does not fit its 2 MB. An issue with a fix has a button, and the file as it was is kept in `.fried-editor/originals/` in the project. A build, of the player or of the game, opens a window with its step, a bar and the end of its log, and a failed one stays open with the error until it is closed. The scene that was open is remembered for each project, in `~/.config/fried-editor/state.json`, and opened again next time. Play (F5, or the green button in the toolbar) saves the scene, builds the project's game for the PC in its `build/` folder, which needs the same tools as the Project Manager's build, and runs it. The button becomes Stop while the game builds or runs, and the game's log is `fried-editor-game.log` in the temporary directory. File, Open scene switches between the project's scene files, and `--scene <file>` opens one at start. A project that has an `editor.hxml` beside its `build.hxml` gets a player built from it, so its own nodes show in the viewport and in the inspector, and a node field is a list of the scene's nodes. The player is built again when the engine's or the project's sources are newer than it. Ctrl+S saves the scene to `assets/scenes/` in the project. File, Project Settings opens the project's own settings, which the Project Manager also edits: its name, organization, version, window title, start scene and display. Each change is written to `project.fried` as it is made, and the viewport's frame follows the display at once. The window has a page for each platform, with its icon, which is made from any image dropped on it or named by its path and converted to what that platform reads, the text its package shows, and its build: the configuration, the build folder, whether the toolchain is installed, what is wrong with the assets for that platform and a button that builds the game for it, with the output coloured as it is printed and a way to stop it. The way each platform is built is remembered for each project. The viewport draws at the size of its panel, so nothing is scaled, and the white frame is the area the game shows on the selected platform: the display's size divided by the game camera's zoom there. The platform selector previews that platform's screen: the camera area follows the project's display mode on it, and the Issues tab checks the assets for it.

`FRIED_ENGINE=<engine> build/tests/core_tests` also runs the tests that start the real player.
