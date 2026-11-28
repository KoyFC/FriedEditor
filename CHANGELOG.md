# Changelog

All notable changes to Fried Editor are written here, newest version first.

- Changes go under the topmost `## vX.Y.Z` heading while they are in development. That heading is the version in progress and has no date.
- When a version is published, replace `vX.Y.Z` with its number and add the date (`## v0.3.0 - 2026-12-01`), and start a new `## vX.Y.Z` section above it.
- Each section uses `Added`, `Changed`, `Fixed` and `Removed`, and leaves out the ones it has no entries for.
- The Release workflow copies a version's section into its GitHub release, see [Releasing](https://koyfc.github.io/FriedEditor/development/#releasing).

## vX.Y.Z

### Added
- Modules in Player Settings: the General page and each platform's page choose the backend of each module (such as `kinematic` or `box2d` for 2D physics), and the ones a platform cannot run are listed and refused.
- The inspector greys the settings the platform shown does not use, such as a collider's `friction` or a body's `bullet` where 2D physics is the default one, and a line above the components lists the platforms. Their tooltip names the module and the platforms.
- The inspector warns about components left out of a platform's build by the modules, and greys the ones a platform shown does not use. Add Component lists the ones no platform uses, and cannot add them.

### Changed
- The settings of each platform are saved in `platforms.<platform>` of `project.fried`, and a project in the old layout is upgraded when it is saved.
- The editor protocol is 8, which carries the modules of the engine, so the player has to be rebuilt.

## v0.4.0 - 2026-11-28

### Added
- Game objects and components: the hierarchy lists the scene's objects, each with its transform and its components, as in Unity. The inspector shows the active switch and name, the transform and one section for each component, with Add Component, Remove, Change Transform Type and Create New Object, and the running game's values are shown by component. A project with version 2 scenes is offered the engine's `fried_import migrate-scenes` when it opens.
- Projects open and save in YAML: the project file, the scenes (`.scene.yaml`) and the import settings are written as YAML in the order a person reads them, and a project that is still in JSON is offered a conversion when it opens, with a backup of the originals in `.fried/json-backup/`.
- The `mesh` and `material` fields of a `MeshInstance3D` are a list of the meshes and materials the engine has, with the fields of the one chosen under it, and a mesh can be a model file (`.obj`, `.gltf` or `.glb`), which the Assets panel lists and filters as Models.
- Play Mode Changes: when the game stops, the edits made while it ran are listed and the ones ticked are made again on the scene.
- Window > Undo History, a list of the scene's undo steps that goes to any of them.
- Quick Open finds commands with `>` and objects of the scene with `@`.
- The cells of a tilemap are edited as a text area, and the Game stats show the time of each section of the frame when the project's player is built with `-D fried_profile`.
- `UIDocument` fields: the `document` and `theme` of a document are chosen from the project's HTML and CSS files, an `.html` file dropped on the scene or added from the Assets dock makes a `UIDocument`, and a `.css` file dropped on a document sets its theme. The Game stats list the time of the UI style and layout.

### Changed
- Scenes are version 3 (`objects`, `transform` and `components`) and the protocol with the player is 7, so a player built before has to be built again. New scripts extend `Component` and go under `src/components`, and the project's autoloads are component classes or scenes.

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
