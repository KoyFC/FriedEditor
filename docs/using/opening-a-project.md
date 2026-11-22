# Opening a project

The editor opens a project's folder, the one with `project.fried`, and finds the engine in its `engine/` folder or next to it (the engine's own `sandbox/` works as is). Otherwise pass `--engine <path>`.

```
build/app/fried_editor ~/Documentos/GitHub/FriedEngine/sandbox
build/app/fried_editor <project> --engine <engine>
```

The first run builds the editor player from the engine, which needs Haxe, hxcpp and CMake as building a game does, and takes a minute. On Windows it builds with Visual Studio's C++ tools, or with the w64devkit the Project Manager downloaded, the way a build of the game does. A machine that cannot build says what is missing instead of starting. The log goes to `fried-editor-player-build.log` in the temporary directory. File, Rebuild player does it again after the engine changes.

File, Open scene switches between the project's scene files, and `--scene <file>` opens one at start.

The scene that was open is remembered for each project, in `state.json` in that folder, and opened again next time.

A project that has an `editor.hxml` beside its `build.hxml` gets a player built from it, so its own nodes show in the viewport and in the inspector, and a node field is a list of the scene's nodes. The player is built again when the engine's or the project's sources are newer than it.

## Opening scenes and finding files

File, Open scene lists the project's scenes, and File, Open recent the last eight that were opened, the latest first. Each scene opens in a tab of its own above the Scene view, as in Godot: going to another tab keeps the changes and the undo history of the scene it leaves, a tab with changes shows a dot, and closing one that has changes asks whether to save them. File, Save All (Ctrl+Shift+S) saves every tab, quitting asks once about all of them, and the tabs that were open come back the next time the project is. The last tab cannot be closed, and none can while the game runs. Ctrl+P (File, Quick open) opens a field in which a part of a name is enough: the letters only have to appear in order, so `sbx3` finds `sandbox_3d.scene.json`, and a match in the file's own name ranks over one in its folders. With nothing typed it lists the scenes, the recent ones first. Up and Down choose, Enter or a click opens a scene, and for any other file it opens the folder that holds it in the Assets tab.

## Panels

Each panel is a tab that can be dragged next to another, out into a group of its own or away from the editor's window, and closed with the cross on its tab. The Window menu opens the ones that were closed, Window, Layout chooses between Godot Default (the hierarchy over the assets on the left, the scene and the game in the middle with the console and the issues under them, and the inspector on the right, which is what a first run has), Unity Default (the hierarchy on the left, the assets under the scene and the game, and the inspector on the right) and Unity's 2 by 3 (the hierarchy over the assets, the scene over the game, and the inspector the full height), and Window, Reset layout puts all of them back where the chosen layout has them. The layout is remembered between runs.
