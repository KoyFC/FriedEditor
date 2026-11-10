#include "fried/editor/player_builder.h"

namespace fried::editor
{
    std::vector<SubprocessOptions> PlayerBuilder::engineSteps(const std::string &enginePath, const std::string &logFile)
    {
        std::string playerDirectory = enginePath + "/editor/player";
        return {
            {{"haxe", "build.hxml"}, {}, playerDirectory, logFile},
            {{"cmake", "-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=Release"}, {}, playerDirectory, logFile},
            {{"cmake", "--build", "build", "-j"}, {}, playerDirectory, logFile},
        };
    }

    PlayerBuilder::PlayerBuilder(std::vector<SubprocessOptions> steps)
        : m_steps(std::move(steps))
        , m_next(0)
        , m_state(State::Building)
    {
    }

    PlayerBuilder::State PlayerBuilder::pump()
    {
        if (m_state != State::Building)
        {
            return m_state;
        }

        if (m_running)
        {
            if (m_running->isRunning())
            {
                return m_state;
            }
            if (m_running->exitCode() != 0)
            {
                m_failure = "The step " + std::to_string(m_next) + " of " + std::to_string(m_steps.size()) + " ("
                    + m_steps[m_next - 1].m_arguments[0] + ") failed with exit code " + std::to_string(m_running->exitCode()) + ".";
                m_running.reset();
                m_state = State::Failed;
                return m_state;
            }
            m_running.reset();
        }

        if (m_next == m_steps.size())
        {
            m_state = State::Succeeded;
            return m_state;
        }

        std::string error;
        m_running = startSubprocess(m_steps[m_next], &error);
        ++m_next;
        if (!m_running)
        {
            m_failure = error;
            m_state = State::Failed;
        }
        return m_state;
    }

    std::string playerExecutablePath(const std::string &enginePath)
    {
        return enginePath + "/editor/player/build/fried_editor_player";
    }
}
