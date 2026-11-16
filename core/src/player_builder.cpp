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
    {
        std::string logFile = steps.empty() ? std::string() : steps.front().m_logFile;
        m_runner.start(std::move(steps), logFile);
    }

    PlayerBuilder::State PlayerBuilder::pump()
    {
        switch (m_runner.pump())
        {
        case fried::core::StepRunner::State::Succeeded:
            return State::Succeeded;
        case fried::core::StepRunner::State::Running:
            return State::Building;
        default:
            m_failure = "The step " + std::to_string(m_runner.step()) + " of " + std::to_string(m_runner.stepCount()) + " failed. " + m_runner.failure();
            return State::Failed;
        }
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
                if (it->is_regular_file(ignored) && (extension == ".hx" || extension == ".hxml" || extension == ".cpp" || extension == ".h" || extension == ".cmake" || it->path().filename() == "CMakeLists.txt"))
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
        newest = newestSource(enginePath + "/cmake", newest);
        newest = newestSource(enginePath + "/editor", newest);
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
