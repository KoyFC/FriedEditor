#pragma once

#include "fried/editor/transport.h"

#include <memory>
#include <string>
#include <vector>

namespace fried::editor
{
    // Runs the commands that build the editor player, one after another, without blocking the editor.
    class PlayerBuilder
    {
    public:
        enum class State
        {
            Building,
            Succeeded,
            Failed
        };

        // The steps the engine's own build uses: haxe, then CMake to configure and to compile.
        static std::vector<SubprocessOptions> engineSteps(const std::string &enginePath, const std::string &logFile);

        PlayerBuilder(std::vector<SubprocessOptions> steps);

        // Advances the build and says where it stands. A step that fails stops it.
        State pump();

        // What the step that failed was, once it has.
        const std::string &failure() const { return m_failure; }

    private:
        std::vector<SubprocessOptions> m_steps;
        std::size_t m_next;
        std::unique_ptr<Subprocess> m_running;
        State m_state;
        std::string m_failure;
    };

    // Where the player's executable is once built, inside an engine checkout.
    std::string playerExecutablePath(const std::string &enginePath);
}
