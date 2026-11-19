#pragma once

#include "fried/editor/transport.h"

#include "fried/core/step_runner.h"
#include "fried/core/toolchains.h"

#include <memory>
#include <string>
#include <vector>

namespace fried::editor
{
    // What this machine builds with: the toolchains on it and the ones the Project Manager installed, and Visual Studio on Windows.
    fried::core::Toolchains machineToolchains();

    // Runs the commands that build the editor player, one after another, without blocking the editor. What they print goes to the log file the steps name.
    class PlayerBuilder
    {
    public:
        enum class State
        {
            Building,
            Succeeded,
            Failed
        };

        // The steps the engine's own build uses: haxe, then CMake to configure and to compile. Empty when the machine cannot build, with the reason in error.
        static std::vector<SubprocessOptions> engineSteps(const std::string &enginePath, const std::string &logFile, const fried::core::Toolchains &toolchains,
                                                          std::string *error);

        // The same for a project that has its own editor.hxml, which puts the project's components in the player.
        static std::vector<SubprocessOptions> projectSteps(const std::string &enginePath, const std::string &projectDirectory, const std::string &logFile,
                                                           const fried::core::Toolchains &toolchains, std::string *error);

        PlayerBuilder(std::vector<SubprocessOptions> steps);

        // Advances the build and says where it stands. A step that fails stops it.
        State pump();

        // Which step is running, from 1, how many there are, and what it does in words.
        std::size_t step() const { return m_runner.step(); }
        std::size_t stepCount() const { return m_runner.stepCount(); }
        std::string stepLabel() const { return m_runner.stepLabel(); }

        // What the steps have printed so far, a line at a time.
        const std::vector<fried::core::OutputLine> &lines() const { return m_runner.lines(); }

        // What the step that failed was, once it has.
        const std::string &failure() const { return m_failure; }

    private:
        fried::core::StepRunner m_runner;
        std::string m_failure;
    };

    // Where the player's executable is once built, inside an engine checkout.
    std::string playerExecutablePath(const std::string &enginePath, const fried::core::Toolchains &toolchains);

    // A project with an editor.hxml has a player of its own, and any other uses the engine's.
    bool hasProjectPlayer(const std::string &projectDirectory);
    std::string playerExecutablePath(const std::string &enginePath, const std::string &projectDirectory, const fried::core::Toolchains &toolchains);

    // True when the player is missing or older than the engine's or the project's sources, which would leave it describing components that have since changed.
    bool isPlayerStale(const std::string &enginePath, const std::string &projectDirectory, const fried::core::Toolchains &toolchains);
}
