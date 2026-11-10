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
- [ ] B: offscreen frames into a Qt widget on a Wayland session

### 2. Engine core (FriedEngine)

- [ ] Object ids and lookup, runtime asset paths
- [ ] Component serialization and registry
- [ ] Scene loader with per-platform overrides

### 3. Editor player (FriedEngine)

- [ ] Offscreen rendering and `readPixels`
- [ ] Protocol, bounds and schema reply

### 4. Editor skeleton

- [ ] CMake with `core/`, `core_tests` and `ui/qt/`
- [ ] Project, platform and build target model in core
- [ ] Subprocess, TCP listener and message framing
- [ ] Player session: build, launch, handshake, schema

### 5. Scene editing

- [ ] Scene document with load and save
- [ ] Undo and redo commands
- [ ] Viewport with pan, zoom and selection
- [ ] Hierarchy panel
- [ ] Inspector generated from the schema
- [ ] Asset browser, drag a texture to create a sprite
- [ ] Move, scale and rotate gizmos

### 6. Per-platform

- [ ] Platform selector and override editing
- [ ] Display frame from `project.fried`
- [ ] Asset validation with fixes (texture sizes, DS audio format, fx-CG50 size)

### 7. Play mode

- [ ] Build and run the game for PC from the editor

## Later

A second UI backend (Dear ImGui), a binary cooked scene format, prefabs, hierarchy, a `screen` field for dual-screen platforms, 3D components, user components in the viewport, per-platform asset variants, a shared library with the Project Manager.
