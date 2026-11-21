# Architecture

## Layout

- `core/` is all of the editor's logic, with no UI toolkit in it: the standard library, `nlohmann/json` and FriedCore only. Its link to the player is a loopback socket, POSIX or Winsock according to the system. Nothing under it may include a UI header, and configuring fails if something does.
- `ui/` is the editor application: Dear ImGui over SDL2 and OpenGL 3, with a dark theme and the engine's own font. It draws, forwards input and holds no editor state beyond its widgets.
- `app/` is the executable, `fried_editor`, that starts it.
- `tests/` runs the core with doctest, and builds without any toolkit installed.
- `third_party/fried_core` is [FriedCore](https://github.com/KoyFC/FriedCore) as a submodule, the library shared with the Project Manager: the project file, the platforms, the steps of a build and the programs that run them, and images. It also brings `nlohmann/json` and `stb_image`. Clone with `--recursive`, or run `git submodule update --init` once.
- The rest of `third_party/` holds `doctest` and Dear ImGui 1.92.9 (docking, with one local change for evaluating typed numbers), unchanged.

## Decisions

- A scene is a tree of typed nodes, 2D and 3D, with no game objects or components.
- A scene is a JSON file under the project's `assets/scenes/`, written by this editor and only read by the engine.
- The editor is split into `core/`, which has no UI dependency and is tested headless, and `ui/`, which only draws and forwards input. Nothing under `core/` may include a UI header.
- The viewport is a separate engine process, the editor player, that renders offscreen on request and talks to the editor over loopback TCP.
- Code shared with the Project Manager lives in FriedCore, a library with no UI toolkit in it that both take as a submodule.
