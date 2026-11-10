set(CMAKE_AUTOMOC ON)

add_library(fried_editor_qt STATIC
    src/main_window.cpp
    src/main_window.h
)
target_include_directories(fried_editor_qt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fried_editor_qt PUBLIC fried_editor_core Qt6::Widgets)
