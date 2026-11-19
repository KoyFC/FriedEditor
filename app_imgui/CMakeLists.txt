add_executable(fried_editor imgui_main.cpp)
target_link_libraries(fried_editor PRIVATE fried_editor_imgui)

if(WIN32)
    # A window of its own and no console beside it, as the Project Manager starts it.
    set_target_properties(fried_editor PROPERTIES WIN32_EXECUTABLE TRUE)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(fried_editor PRIVATE SDL2::SDL2main)
    endif()
endif()
