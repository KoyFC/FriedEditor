#include "fried/editor/player_builder.h"

#include <filesystem>

namespace fs = std::filesystem;

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

    std::vector<SubprocessOptions> PlayerBuilder::projectSteps(const std::string &enginePath, const std::string &projectDirectory, const std::string &logFile)
    {
        std::string buildDirectory = projectDirectory + "/build/editor-player";
        return {
            {{"haxe", "editor.hxml"}, {}, projectDirectory, logFile},
            {{"cmake", "-S", enginePath + "/editor/project-player", "-B", buildDirectory, "-DFRIED_PROJECT_DIR=" + projectDirectory, "-DCMAKE_BUILD_TYPE=Release"}, {}, projectDirectory, logFile},
            {{"cmake", "--build", buildDirectory, "-j"}, {}, projectDirectory, logFile},
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

namespace fried::editor
{
    bool hasProjectPlayer(const std::string &projectDirectory)
    {
        std::error_code ignored;
        return fs::exists(projectDirectory + "/editor.hxml", ignored);
    }

    std::string playerExecutablePath(const std::string &enginePath, const std::string &projectDirectory)
    {
        if (hasProjectPlayer(projectDirectory))
        {
            return projectDirectory + "/build/editor-player/fried_project_editor_player";
        }
        return playerExecutablePath(enginePath);
    }

    namespace
    {
        fs::file_time_type newestSource(const fs::path &directory, fs::file_time_type newest)
        {
            std::error_code ignored;
            for (fs::recursive_directory_iterator it(directory, ignored), end; it != end; it.increment(ignored))
            {
                std::string extension = it->path().extension().string();
                if (it->is_regular_file(ignored) && (extension == ".hx" || extension == ".hxml" || extension == ".cpp" || extension == ".h"))
                {
                    newest = std::max(newest, it->last_write_time(ignored));
                }
            }
            return newest;
        }
    }

    bool isPlayerStale(const std::string &enginePath, const std::string &projectDirectory)
    {
        std::error_code ignored;
        std::string executable = playerExecutablePath(enginePath, projectDirectory);
        if (!fs::exists(executable, ignored))
        {
            return true;
        }
        fs::file_time_type built = fs::last_write_time(executable, ignored);
        fs::file_time_type newest = fs::file_time_type::min();
        newest = newestSource(enginePath + "/src", newest);
        newest = newestSource(enginePath + "/native", newest);
        if (hasProjectPlayer(projectDirectory))
        {
            newest = newestSource(projectDirectory + "/src", newest);
            newest = std::max(newest, fs::last_write_time(projectDirectory + "/editor.hxml", ignored));
        }
        else
        {
            newest = newestSource(enginePath + "/editor/player", newest);
        }
        return newest > built;
    }
}
