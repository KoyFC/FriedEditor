# Installing

## From the Project Manager

[Fried Project Manager](https://koyfc.github.io/FriedProjectManager/using/editor/) downloads a release of the editor and opens it on a project.

## Releases

Each release `v<version>` on [GitHub](https://github.com/KoyFC/FriedEditor/releases) has `FriedEditor-<version>-x86_64.AppImage` (SDL2 inside, OpenGL from the system), `FriedEditor-<version>-windows-x64.zip` (`fried_editor.exe` and `SDL2.dll`) and `SHA256SUMS`, which is what the Project Manager pins to download it.

## Building from source

```
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

The Dear ImGui backend is built when its libraries are found (SDL2 and OpenGL) and the core and its tests are always built. SDL2 is found through its own CMake package when it has one, which is how it is found on Windows: unpack the `SDL2-devel-*-VC.zip` release, pass `-DSDL2_DIR=<SDL2>/cmake`, and keep `SDL2.dll` next to `fried_editor.exe`. Anywhere else `pkg-config` finds it. The Qt backend is off by default, since it no longer builds against the core, which now edits a tree of nodes: `-DFRIED_EDITOR_QT=ON` asks for it, and `-DFRIED_EDITOR_IMGUI=OFF` leaves the ImGui one out on purpose.

The Dear ImGui backend is built when its libraries are found (SDL2 and OpenGL) and the core and its tests are always built. SDL2 is found through its own CMake package when it has one, which is how it is found on Windows: unpack the `SDL2-devel-*-VC.zip` release, pass `-DSDL2_DIR=<SDL2>/cmake`, and keep `SDL2.dll` next to `fried_editor.exe`. Anywhere else `pkg-config` finds it. The Qt backend is off by default, since it no longer builds against the core, which now edits a tree of nodes: `-DFRIED_EDITOR_QT=ON` asks for it, and `-DFRIED_EDITOR_IMGUI=OFF` leaves the ImGui one out on purpose.
