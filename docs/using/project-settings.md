# Project and player settings

The settings and the build output open as windows of their own, which can be moved outside the editor, onto another monitor, and closed with their own close button. On Linux the editor asks SDL for X11 so this works under Wayland too (through XWayland); set `SDL_VIDEODRIVER` to choose another driver, in which case the windows stay inside the editor if that driver cannot make windows of its own.

As in Unity, the settings are in two windows. Edit, Project Settings holds what the game is: the start scene, the display, vsync, the frame rate the game starts at and the physics rate. File, Player Settings holds what the builds are made of: the name, organization, version and window title, the platforms it builds for, and a page for each platform. The Project Manager edits the same file. Each change is written to `project.fried` as it is made, and the viewport's frame follows the display at once.

The platforms are ticked on the General page of Player Settings, all of them until the project narrows them. The engine refuses to configure a build for one that is not ticked, and checks the limits of the ones that are, such as an image too large for the 3DS or the DS, even in a build for another platform. At least one has to stay ticked.

Player Settings has a page for each platform that is ticked, with its icon, which is made from any image dropped on it or named by its path and converted to what that platform reads, the text its package shows, and its build: the configuration, the build folder, whether the toolchain is installed, what is wrong with the assets for that platform and a button that builds the game for it, with the output coloured as it is printed and a way to stop it. The way each platform is built is remembered for each project.
