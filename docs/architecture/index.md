# Architecture

## Layout

- `core/` is all of the editor's logic, with no UI toolkit in it: the standard library, `nlohmann/json` and FriedCore only. Its link to the player is a loopback socket, POSIX or Winsock according to the system. Nothing under it may include a UI header, and configuring fails if something does.
- `ui/imgui/` is the Dear ImGui backend (SDL2 and OpenGL 3), with a dark theme and the engine's own font. It is the editor that `fried_editor` starts.
- `ui/qt/` is the Qt 6 backend, kept as `fried_editor_qt`. It is not used and not kept up to date: every editor feature is built in the Dear ImGui backend only, and the Qt one stops at the asset browser and gizmos. It was last built before the scene became a tree of nodes. A backend draws, forwards input and holds no editor state beyond its widgets, so replacing the toolkit means writing another backend next to these.
- `app_imgui/` and `app/` are the executables that start each backend.
- `tests/` runs the core with doctest, and builds without any toolkit installed.
- `third_party/fried_core` is [FriedCore](https://github.com/KoyFC/FriedCore) as a submodule, the library shared with the Project Manager: the project file, the platforms, the steps of a build and the programs that run them, and images. It also brings `nlohmann/json` and `stb_image`. Clone with `--recursive`, or run `git submodule update --init` once.
- The rest of `third_party/` holds `doctest` and Dear ImGui 1.92.9 (docking, with one local change for evaluating typed numbers), unchanged.

## Decisions

- A scene is a tree of typed nodes, 2D and 3D, with no game objects or components.
- A scene is a JSON file under the project's `assets/scenes/`, written by this editor and only read by the engine.
- The editor is split into `core/`, which has no UI or Qt dependency and is tested headless, and `ui/<toolkit>/`, which only draws and forwards input. Qt came first and Dear ImGui replaced it: ImGui is the editor now and the Qt backend is no longer extended. Nothing under `core/` may include a UI header.
- The viewport is a separate engine process, the editor player, that renders offscreen on request and talks to the editor over loopback TCP.
- Code shared with the Project Manager lives in FriedCore, a library with no UI toolkit in it that both take as a submodule.
