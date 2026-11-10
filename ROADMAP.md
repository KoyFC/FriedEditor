# Roadmap

FriedEditor is the scene editor for [Fried Engine](https://github.com/KoyFC/FriedEngine). Fried Project Manager creates and builds projects, the engine is the runtime, and this editor assembles scenes. The same roadmap is in the engine's `docs/editor-roadmap.md`. Items below say which repository owns them.

## Decisions

- v1 is 2D only, with no hierarchy and built-in components only.
- A scene is a JSON file under the project's `assets/scenes/`, written by this editor and only read by the engine.
- The editor is split into `core/`, which has no UI or Qt dependency and is tested headless, and `ui/<toolkit>/`, which only draws and forwards input. Qt first, so a later Dear ImGui backend means writing one backend and nothing else. Nothing under `core/` may include a UI header.
- The viewport is a separate engine process, the editor player, that renders offscreen on request and talks to the editor over loopback TCP.
- Code shared with the Project Manager is copied minimally and converted to standard types, and extracted into a shared library only if the duplication starts to hurt.

## Phases

### 0. Roadmap in both repositories

- [x] This file, and `docs/editor-roadmap.md` in FriedEngine

### 1. Spikes (FriedEngine and here)

- [ ] A: loader round trip on PC and a console
- [x] B: offscreen frames into a Qt widget on a Wayland session (works, the player runs with the dummy video driver)

### 2. Engine core (FriedEngine)

- [x] Object ids and lookup, runtime asset paths
- [x] Component serialization and registry
- [x] Scene loader with per-platform overrides

### 3. Editor player (FriedEngine)

- [x] Offscreen rendering and `readPixels`
- [x] Protocol, bounds and schema reply

### 4. Editor skeleton

- [x] CMake with `core/`, `core_tests` and `ui/qt/`
- [x] Project and platform model in core (build targets come with play mode)
- [x] Subprocess, TCP listener and message framing (POSIX only so far)
- [x] Player session: build, launch, handshake, schema

### 5. Scene editing

- [x] Scene document with load and save
- [x] Undo and redo commands
- [x] Viewport with pan, zoom and selection
- [x] Hierarchy panel
- [x] Inspector generated from the schema
- [x] Asset browser, drag a texture to create a sprite
- [x] Move, scale and rotate gizmos

### 6. Per-platform

- [x] Platform selector and override editing
- [x] Display frame from `project.fried`
- [x] Asset validation with fixes (texture sizes, DS audio format, fx-CG50 size)

### 7. Play mode

- [x] Build and run the game for PC from the editor

### 8. The sandbox as scene files

- [x] Object references between components, and a registry for a game's own components
- [x] A player per project, built from its `editor.hxml`, rebuilt when its sources change
- [x] The sandbox's 2D scene and its interface layer as scene files, with the same frame as the scene built in code
- [x] Open Scene in the editor

Known limits: an object that other objects refer to cannot be deleted without fixing those references first, or the scene fails to load, and a game component is selected by its `width` and `height` or a fixed 16 pixel marker.

### 9. Projects driven by the editor (FriedEngine and FriedEditor)

- [x] Components start after a scene file is fully loaded, and never in the editor player
- [x] A component can read the size of the view it is drawn to without holding the renderer
- [x] A scene can be loaded on top of another, and a scene with no camera is screen space
- [x] The engine opens the window and loads the scene `project.fried` names, with an optional `Game` class for what a game needs once
- [x] The components of a project are found without a list to maintain
- [x] Components no longer receive the renderer or the layers (`DrawQueue.currentRenderTarget` is still public, for the engine's own drawing)
- [x] The sandbox's sources are in folders by purpose, and it stays the engine's test bed, with WhiteCellSimulator as the project built in the editor's way

### 10. WhiteCellSimulator as scene files (its own repository)

- [x] Menu, game and game over as scenes, with the flow in a controller component
- [x] Enemies and projectiles as components with a texture field, still spawned from code
- [x] The second window of the 3DS stays in the `Game` hook
- [x] Small screens through a `cg50` override block instead of `Screen.scale`
- [x] Built for the PC and the fx-CG50 (the .g3a is 1.9 MB of 2 MB). Not run on a calculator, and the start screen was compared with the old one on the PC only

Known limit: a position that depends on the screen's size, such as the right edge, is set by a component, and the editor shows it unplaced because its player never updates.

### 11. Viewport and speed (FriedEngine and FriedEditor)

- [x] The player renders at the size of the panel, so a frame is not scaled
- [x] The display frame is the display's size divided by the zoom of the game camera on that platform
- [x] A pan step takes 4 ms instead of 43 (Nagle and a player that never slept), and the player is nearly idle when nothing is asked of it

### 12. Feedback while building and opening where it was left (FriedEditor)

- [x] A window with the step, a bar and the end of the log while the player or the game builds, and the error when it fails
- [x] The scene that was open is opened again the next time

## Later

A second UI backend (Dear ImGui), a binary cooked scene format, prefabs, hierarchy, a `screen` field for dual-screen platforms, 3D components, per-platform asset variants, a shared library with the Project Manager.
