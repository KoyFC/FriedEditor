add_executable(core_tests
    main.cpp
    asset_browser_test.cpp
    asset_files_test.cpp
    command_stack_test.cpp
    editor_state_test.cpp
    large_scene_test.cpp
    editor_test.cpp
    expression_test.cpp
    display_name_test.cpp
    fuzzy_test.cpp
    game_runner_test.cpp
    gizmo_test.cpp
    input_map_test.cpp
    log_tail_test.cpp
    game_session_test.cpp
    player_session_test.cpp
    project_settings_test.cpp
    scene_document_test.cpp
    script_files_test.cpp
    transport_test.cpp
    viewport_test.cpp
    viewport3d_test.cpp
    version_test.cpp
)
target_link_libraries(core_tests PRIVATE fried_editor_core)
target_compile_definitions(core_tests PRIVATE FRIED_EDITOR_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")

add_test(NAME core_tests COMMAND core_tests)
