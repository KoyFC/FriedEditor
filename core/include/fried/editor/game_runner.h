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

        // The steps that build the game for the PC in the project's build/ folder, which are the Project Manager's. Empty when the machine cannot build, with the reason in error.
        static std::vector<SubprocessOptions> steps(const std::string &projectDirectory, const std::string &logFile, const fried::core::Toolchains &toolchains,
                                                    std::string *error);

        // Where the build leaves the game's executable.
        static std::string executablePath(const std::string &projectDirectory, const fried::core::Toolchains &toolchains);

        // Starts building. Does nothing while a game is being built or is running.
        bool start(const std::string &projectDirectory, const std::string &logFile, const fried::core::Toolchains &toolchains, std::string *error);

        // Advances the build, then watches the game. Returns the state.
        State pump();

        // Ends the game, or the build, whichever is going on.
        void stop();

        // While the game is being built: which step it is on, out of how many, and what it does.
        std::size_t step() const { return m_builder ? m_builder->step() : 0; }
        std::size_t stepCount() const { return m_builder ? m_builder->stepCount() : 0; }
        std::string stepLabel() const { return m_builder ? m_builder->stepLabel() : std::string(); }
        const std::string &logFile() const { return m_logFile; }

        // What the build has printed so far, and nothing once it is over.
        const std::vector<fried::core::OutputLine> *lines() const { return m_builder ? &m_builder->lines() : nullptr; }

        State state() const { return m_state; }
        const std::string &failure() const { return m_failure; }

    private:
        std::string m_projectDirectory;
        std::string m_logFile;
        fried::core::Toolchains m_toolchains;
        std::unique_ptr<PlayerBuilder> m_builder;
        std::unique_ptr<Subprocess> m_game;
        State m_state = State::Idle;
        std::string m_failure;
    };
}
