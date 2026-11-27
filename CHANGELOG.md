# Changelog

All notable changes to Fried Editor are written here, newest version first.

- Changes go under the topmost `## vX.Y.Z` heading while they are in development. That heading is the version in progress and has no date.
- When a version is published, replace `vX.Y.Z` with its number and add the date (`## v0.3.0 - 2026-12-01`), and start a new `## vX.Y.Z` section above it.
- Each section uses `Added`, `Changed`, `Fixed` and `Removed`, and leaves out the ones it has no entries for.
- The Release workflow copies a version's section into its GitHub release, see [Releasing](https://koyfc.github.io/FriedEditor/development/#releasing).

## vX.Y.Z

### Added
- The `mesh` and `material` fields of a `MeshInstance3D` are a list of the meshes and materials the engine has, with the fields of the one chosen under it, and a mesh can be a model file (`.obj`, `.gltf` or `.glb`), which the Assets panel lists and filters as Models.
- Play Mode Changes: when the game stops, the edits made while it ran are listed and the ones ticked are made again on the scene.
- Window > Undo History, a list of the scene's undo steps that goes to any of them.
- Quick Open finds commands with `>` and nodes of the scene with `@`.
- The cells of a tilemap are edited as a text area, and the Game stats show the time of each section of the frame when the project's player is built with `-D fried_profile`.

### Changed
- A scene that names a `MeshInstance`, or gives a `MeshInstance3D` a `shape` and a `color`, is opened with a `mesh` and a material instead, and says it was migrated. The protocol with the player is 6, so a player built before has to be built again.

### Fixed
- Panning the view with the right or middle button works in a Wayland session whose SDL2 is `sdl2-compat`, where the pointer is neither locked nor wrapped (see the scene view's documentation).

## v0.3.0 - 2026-11-26

### Added
- Play the game inside the editor from a Game tab: play, pause, step a frame, change the speed, frame limit, running values shown in the scene view and inspector, and a console tab with the game's log.
- Android and the touch controls, and playing the game at the screen size and as the platform being previewed.
- Several scenes open at once as tabs, scene instances, and a layout choice in the Window menu (Unity default, Godot default).
- Hierarchy: icons, alternating rows, filtering by name or type, duplicate, copy and paste, dropping images, fonts and scenes on it.
- Assets dock: browse by folder, rename, move, duplicate, delete (to the system trash) and make folders, list scripts and create them from a template, open them in VS Code, drag from the grid.
- Import settings of images and sounds in the inspector, kept in sidecar files; what a font comes to on the DS.
- Project Settings: platforms, autoload scenes, input actions (validated in Issues), float precision per platform.
- 2D grid with snapping, locking nodes, Blender style G, R and S transforms, quick open and recent scenes.
- Revert arrow beside fields that differ from their default, Ctrl+Shift+S, Godot's keys for play, pause and stop.
- Run on Wayland in a Wayland session.

### Changed
- The Qt backend is gone. The editor lives in `ui` and `app`.
- New look: grey palette with a blue accent, the Inter font, a Unity style toolbar, icons rasterized from Godot's into one texture.
- Every shortcut is defined once and read from keys, menus and tooltips, and labels use one case and one word each.
- The editor shows only flat nodes in 2D and only meshes in 3D.

### Fixed
- The navigation frame no longer drifts off the hierarchy's selection.
- Delete, duplicate, copy and paste only act on the panel that has the keyboard.
- Game frames are read into a reused buffer and only the newest is shown.

## v0.2.0 - 2026-11-20

### Added
- 3D view with a free camera and selection, moving a Node3D along its parent's axes, and drawing the edges of each Camera3D's view and its far plane.
- The player runs on SDL's offscreen video driver so it can draw with OpenGL.
- Documentation site on GitHub Pages, published with GitHub Actions.

### Changed
- Licensed under MIT.

## v0.1.0 - 2026-11-19

- First published version.
