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
set(FRIED_ICON_SOURCE "#include \"icons.h\"\n\nnamespace editor_ui\n{\n    const IconSource s_iconSources[] = {\n")
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

# The fonts are compiled in as byte arrays, for the same reason.
set(FRIED_FONT_SOURCE "#include \"style.h\"\n\nnamespace editor_ui\n{\n")
foreach(font_file Inter-Regular Inter-SemiBold)
    string(REPLACE "-" "_" font_symbol ${font_file})
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/fonts/${font_file}.ttf font_hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," font_bytes "${font_hex}")
    string(LENGTH "${font_hex}" font_hex_length)
    math(EXPR font_size "${font_hex_length} / 2")
    string(APPEND FRIED_FONT_SOURCE "    extern const unsigned char s_${font_symbol}[] = {${font_bytes}};\n    extern const unsigned int s_${font_symbol}Size = ${font_size};\n")
endforeach()
string(APPEND FRIED_FONT_SOURCE "}\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/fonts_data.cpp.tmp "${FRIED_FONT_SOURCE}")
configure_file(${CMAKE_CURRENT_BINARY_DIR}/fonts_data.cpp.tmp ${CMAKE_CURRENT_BINARY_DIR}/fonts_data.cpp COPYONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fonts/Inter-Regular.ttf ${CMAKE_CURRENT_SOURCE_DIR}/fonts/Inter-SemiBold.ttf)

add_library(fried_editor_ui STATIC
    ${CMAKE_CURRENT_BINARY_DIR}/icons_data.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/fonts_data.cpp
    src/app.cpp
    src/app.h
    src/icons.cpp
    src/icons.h
    src/panels.cpp
    src/panels.h
    src/pointer.cpp
    src/project_settings_window.cpp
    src/input_map_page.cpp
    src/pointer.h
    src/shortcuts.cpp
    src/shortcuts.h
    src/dialog_window.cpp
    src/style.cpp
    src/style.h
    src/textures.cpp
    src/textures.h
)
target_include_directories(fried_editor_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src PRIVATE ${PROJECT_SOURCE_DIR}/third_party ${PROJECT_SOURCE_DIR}/third_party/nanosvg)
target_link_libraries(fried_editor_ui PUBLIC fried_editor_core imgui)

# On X11 a window the editor opens is made a dialog of the main one, which needs Xlib.
find_package(X11 QUIET)
if(X11_FOUND AND NOT WIN32 AND NOT APPLE)
    target_link_libraries(fried_editor_ui PRIVATE X11::X11)
    target_compile_definitions(fried_editor_ui PRIVATE FRIED_EDITOR_X11)
endif()
