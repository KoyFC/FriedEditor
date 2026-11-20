# Project settings

File, Project Settings opens the project's own settings, which the Project Manager also edits: its name, organization, version, window title, start scene, display, vsync, the frame rate the game starts at and the physics rate. Each change is written to `project.fried` as it is made, and the viewport's frame follows the display at once.

The window has a page for each platform, with its icon, which is made from any image dropped on it or named by its path and converted to what that platform reads, the text its package shows, and its build: the configuration, the build folder, whether the toolchain is installed, what is wrong with the assets for that platform and a button that builds the game for it, with the output coloured as it is printed and a way to stop it. The way each platform is built is remembered for each project.
