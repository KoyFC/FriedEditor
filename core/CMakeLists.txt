add_library(fried_editor_core STATIC
    src/message_channel.cpp
    src/project.cpp
    src/version.cpp
)

if(UNIX)
    target_sources(fried_editor_core PRIVATE
        src/posix/sockets.cpp
        src/posix/subprocess.cpp
    )
endif()

target_include_directories(fried_editor_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party
)
target_compile_features(fried_editor_core PUBLIC cxx_std_17)

target_compile_definitions(fried_editor_core PRIVATE FRIED_EDITOR_VERSION="${PROJECT_VERSION}")
