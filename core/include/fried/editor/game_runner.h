#pragma once

#include "fried/editor/player_builder.h"
#include "fried/editor/transport.h"

#include <memory>
#include <string>
#include <vector>

namespace fried::editor
{
    // Builds the project's game for the PC and runs it, which is what play mode is.
    class GameRunner
    {
    public:
        enum class State
        {
            Idle,
            Building,
            Running,
            Failed
        };

        // The target name the project's CMakeLists.txt gives fried_add_game. Empty when there is none.
        static std::string targetName(const std::string &projectDirectory);

        // The steps that build the game in the project's build/ folder: haxe, then CMake to configure and to compile.
        static std::vector<SubprocessOptions> steps(const std::string &projectDirectory, const std::string &logFile);

        // Where the build leaves the game's executable.
        static std::string executablePath(const std::string &projectDirectory);

        // Starts building. Does nothing while a game is being built or is running.
        bool start(const std::string &projectDirectory, const std::string &logFile, std::string *error);

        // Advances the build, then watches the game. Returns the state.
        State pump();

        // Ends the game, or the build, whichever is going on.
        void stop();

        State state() const { return m_state; }
        const std::string &failure() const { return m_failure; }

    private:
        std::string m_projectDirectory;
        std::string m_logFile;
        std::unique_ptr<PlayerBuilder> m_builder;
        std::unique_ptr<Subprocess> m_game;
        State m_state = State::Idle;
        std::string m_failure;
    };
}
