find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_library(imgui STATIC
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/misc/cpp/imgui_stdlib.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_sdl2.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${PROJECT_SOURCE_DIR}/third_party/imgui)
target_link_libraries(imgui PUBLIC PkgConfig::SDL2 OpenGL::GL ${CMAKE_DL_LIBS})

add_library(fried_editor_imgui STATIC
    src/app.cpp
    src/app.h
    src/panels.cpp
    src/panels.h
    src/pointer.cpp
    src/pointer.h
    src/style.cpp
    src/style.h
    src/textures.cpp
    src/textures.h
)
target_include_directories(fried_editor_imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
target_link_libraries(fried_editor_imgui PUBLIC fried_editor_core imgui)
