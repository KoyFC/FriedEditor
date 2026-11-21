find_package(OpenGL REQUIRED)

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
target_link_libraries(imgui PUBLIC ${FRIED_SDL2_TARGET} OpenGL::GL ${CMAKE_DL_LIBS})

# The icons are compiled in, one string of SVG each, so the editor needs no files beside it.
file(GLOB FRIED_ICON_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/icons/*.svg)
set(FRIED_ICON_SOURCE "#include \"icons.h\"\n\nnamespace imgui_backend\n{\n    const IconSource s_iconSources[] = {\n")
set(FRIED_ICON_COUNT 0)
foreach(icon_file ${FRIED_ICON_FILES})
    get_filename_component(icon_name ${icon_file} NAME_WE)
    file(READ ${icon_file} icon_text)
    string(APPEND FRIED_ICON_SOURCE "        {\"${icon_name}\", R\"svg(${icon_text})svg\"},\n")
    math(EXPR FRIED_ICON_COUNT "${FRIED_ICON_COUNT} + 1")
endforeach()
string(APPEND FRIED_ICON_SOURCE "    };\n    const int s_iconSourceCount = ${FRIED_ICON_COUNT};\n}\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/icons_data.cpp.tmp "${FRIED_ICON_SOURCE}")
configure_file(${CMAKE_CURRENT_BINARY_DIR}/icons_data.cpp.tmp ${CMAKE_CURRENT_BINARY_DIR}/icons_data.cpp COPYONLY)

add_library(fried_editor_imgui STATIC
    ${CMAKE_CURRENT_BINARY_DIR}/icons_data.cpp
    src/app.cpp
    src/app.h
    src/icons.cpp
    src/icons.h
    src/panels.cpp
    src/panels.h
    src/pointer.cpp
    src/project_settings_window.cpp
    src/pointer.h
    src/style.cpp
    src/style.h
    src/textures.cpp
    src/textures.h
)
target_include_directories(fried_editor_imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src PRIVATE ${PROJECT_SOURCE_DIR}/third_party ${PROJECT_SOURCE_DIR}/third_party/nanosvg)
target_link_libraries(fried_editor_imgui PUBLIC fried_editor_core imgui)
