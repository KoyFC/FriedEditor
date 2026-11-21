# Play

Play (F5, or the play button in the middle of the toolbar) saves the scene, builds the project's game for the PC in its `build/` folder, which needs the same tools as the Project Manager's build, and runs it. The button turns blue and becomes Stop while the game builds or runs, and the game's log is `fried-editor-game.log` in the temporary directory. Ctrl+S saves the scene to `assets/scenes/` in the project.

A build, of the player or of the game, opens a window with its step, a bar and the end of its log, and a failed one stays open with the error until it is closed.

The Console tab, next to the assets, shows the log while the game builds and after it runs, and comes to the front when Play is pressed. It follows the file as it is written, drops the terminal colors, and colors the lines: errors in red and warnings in yellow, by the level the engine prints or by what the build tools say. The Errors, Warnings and Output buttons hide each kind, the field searches, Copy puts the lines on view on the clipboard and Clear forgets them. The tab carries the number of errors, and it keeps the last ten thousand lines.
