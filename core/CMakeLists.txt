add_library(fried_editor_core STATIC
    src/asset_browser.cpp
    src/command_stack.cpp
    src/commands.cpp
    src/editor.cpp
    src/editor_state.cpp
    src/expression.cpp
    src/game_runner.cpp
    src/gizmo.cpp
    src/message_channel.cpp
    src/player_builder.cpp
    src/player_session.cpp
    src/project_settings.cpp
    src/scene_document.cpp
    src/viewport.cpp
    src/version.cpp
)

if(WIN32)
    target_sources(fried_editor_core PRIVATE src/windows/sockets.cpp)
    target_link_libraries(fried_editor_core PUBLIC ws2_32)
elseif(UNIX)
    target_sources(fried_editor_core PRIVATE src/posix/sockets.cpp)
endif()

target_include_directories(fried_editor_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party
)
target_link_libraries(fried_editor_core PUBLIC fried_core)
target_compile_features(fried_editor_core PUBLIC cxx_std_17)

target_compile_definitions(fried_editor_core PRIVATE FRIED_EDITOR_VERSION="${FRIED_EDITOR_VERSION}")
