# Opening a project

The editor opens a project's folder, the one with `project.fried`, and finds the engine in its `engine/` folder or next to it (the engine's own `sandbox/` works as is). Otherwise pass `--engine <path>`.

```
build/app_imgui/fried_editor ~/Documentos/GitHub/FriedEngine/sandbox
build/app_imgui/fried_editor <project> --engine <engine>
```

The first run builds the editor player from the engine, which needs Haxe, hxcpp and CMake as building a game does, and takes a minute. On Windows it builds with Visual Studio's C++ tools, or with the w64devkit the Project Manager downloaded, the way a build of the game does. A machine that cannot build says what is missing instead of starting. The log goes to `fried-editor-player-build.log` in the temporary directory. File, Rebuild player does it again after the engine changes.

File, Open scene switches between the project's scene files, and `--scene <file>` opens one at start.

The scene that was open is remembered for each project, in `state.json` in that folder, and opened again next time.

A project that has an `editor.hxml` beside its `build.hxml` gets a player built from it, so its own nodes show in the viewport and in the inspector, and a node field is a list of the scene's nodes. The player is built again when the engine's or the project's sources are newer than it.
