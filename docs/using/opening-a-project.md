# Opening a project

The editor opens a project's folder, the one with `project.fried`, and finds the engine in its `engine/` folder or next to it (the engine's own `sandbox/` works as is). Otherwise pass `--engine <path>`.

```
build/app/fried_editor ~/Documentos/GitHub/FriedEngine/sandbox
build/app/fried_editor <project> --engine <engine>
```

The first run builds the editor player from the engine, which needs Haxe, hxcpp and CMake as building a game does, and takes a minute. On Windows it builds with Visual Studio's C++ tools, or with the w64devkit the Project Manager downloaded, the way a build of the game does. A machine that cannot build says what is missing instead of starting. The log goes to `fried-editor-player-build.log` in the temporary directory. File > Rebuild Player does it again after the engine changes.

File > Open Scene switches between the project's scene files, and `--scene <file>` opens one at start.

The scene that was open is remembered for each project, in `state.json` in that folder, and opened again next time.

A project that has an `editor.hxml` beside its `build.hxml` gets a player built from it, so its own components show in the viewport and in the inspector, and a field that points at an object is a list of the scene's objects. The player is built again when the engine's or the project's sources are newer than it.

## Opening scenes and finding files

File > Open Scene lists the project's scenes, and File > Open Recent the last eight that were opened, the latest first. Each scene opens in a tab of its own above the Scene view, as in Godot: going to another tab keeps the changes and the undo history of the scene it leaves, a tab with changes shows a star, and closing one that has changes asks whether to save them. File > Save All (Ctrl+Alt+S) saves every tab, File > Save Scene As (Ctrl+Shift+S) saves the open scene under a new name and goes on with the new file, leaving the old one as it was last saved, and the undo history starts again, quitting (Ctrl+Q) asks once about all of them, and the tabs that were open come back the next time the project is. The last tab cannot be closed, and none can while the game runs.

Ctrl+P (File > Quick Open) opens a field in which a part of a name is enough: the letters only have to appear in order, so `sbx3` finds `sandbox_3d.scene.yaml`, and a match in the file's own name ranks over one in its folders. With nothing typed it lists the scenes, the recent ones first. Up and Down choose, Enter or a click opens a scene, and for any other file it opens the folder that holds it in the Assets tab.

Starting the text with `>` lists the commands of the menus instead (save, play, pause, stop, undo and the rest, the ones that cannot be run now left out), and starting it with `@` lists the objects of the open scene, by name and with the path of their parents, and Enter selects the one chosen.

## Panels

Each panel is a tab that can be dragged next to another, out into a group of its own or away from the editor's window, and closed with the cross on its tab. The Window menu opens the ones that were closed, Window > Layout chooses between Godot Default (the hierarchy over the assets on the left, the scene and the game in the middle with the console and the issues under them, and the inspector on the right, which is what a first run has), Unity Default (the hierarchy on the left, the assets under the scene and the game, and the inspector on the right) and Unity 2 by 3 (the hierarchy over the assets, the scene over the game, and the inspector the full height), and Window > Reset Layout puts all of them back where the chosen layout has them. The layout is remembered between runs.

A project whose project file, scenes or import settings are still written in JSON is offered a conversion to YAML when it opens. Converting copies the originals to `.fried/json-backup/` in the project first, rewrites them, and opens the project again; Not Now keeps working with the JSON files, which still load and save as they are. `.fried/` belongs in the project's `.gitignore`.
