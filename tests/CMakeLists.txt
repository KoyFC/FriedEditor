add_executable(core_tests
    main.cpp
    player_session_test.cpp
    project_test.cpp
    transport_test.cpp
    version_test.cpp
)
target_link_libraries(core_tests PRIVATE fried_editor_core)

add_test(NAME core_tests COMMAND core_tests)
