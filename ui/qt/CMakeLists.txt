set(CMAKE_AUTOMOC ON)

add_library(fried_editor_qt_backend STATIC
    src/asset_panel.cpp
    src/asset_panel.h
    src/hierarchy_panel.cpp
    src/hierarchy_panel.h
    src/inspector_panel.cpp
    src/inspector_panel.h
    src/main_window.cpp
    src/main_window.h
    src/viewport_widget.cpp
    src/viewport_widget.h
)
target_include_directories(fried_editor_qt_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fried_editor_qt_backend PUBLIC fried_editor_core Qt6::Widgets)
