# Third party notices

Fried Editor itself is under the MIT License, see [`LICENSE`](LICENSE). These are the components that are bundled with it or linked into it. Each licence below is the one the component's own sources or files state.

## Dear ImGui

`third_party/imgui/` (1.92.9, the docking branch, with one local change for evaluating typed numbers), Copyright (c) 2014-2026 Omar Cornut, <https://github.com/ocornut/imgui>, under the MIT License carried in `third_party/imgui/LICENSE.txt`.

## SDL2

SDL2 2.32.10 for the Windows build, Copyright (C) 1997-2025 Sam Lantinga and contributors, <https://www.libsdl.org/>, under the zlib license that its `LICENSE.txt` carries. `SDL2.dll` is shipped in the Windows zip, and the Linux AppImage carries the SDL2 of the machine that built it, `libsdl2-dev` of Ubuntu 22.04 (the runner `release.yml` names).

## nlohmann/json

`third_party/fried_core/third_party/nlohmann/json.hpp` (v3.11.3), Copyright (c) 2013-2022 Niels Lohmann, <https://github.com/nlohmann/json>, under the MIT License carried in `LICENSE.MIT` beside it.

## stb_image and stb_image_write

`third_party/fried_core/third_party/stb/stb_image.h` (v2.30) and `stb_image_write.h` (v1.16), by Sean Barrett and contributors, <https://github.com/nothings/stb>. Each header offers its terms as two alternatives at its end: the MIT License, or the public domain (Unlicense).

## NanoSVG

`third_party/nanosvg/` (`nanosvg.h` and `nanosvgrast.h`, as bundled with SDL_image 2.8.12), Copyright (c) 2013-14 Mikko Mononen, <https://github.com/memononen/nanosvg>, under the zlib license that the header of each file states. The editor rasterizes its icons with it.

## Godot Engine icons

`ui/imgui/icons/` holds the editor icons of Godot Engine 4, <https://github.com/godotengine/godot/tree/master/editor/icons>, Copyright (c) 2014-present Godot Engine contributors and (c) 2007-2014 Juan Linietsky, Ariel Manzur, under the MIT License carried in `ui/imgui/icons/LICENSE.txt`. They are compiled into the editor.
