#include "fried/editor/player_builder.h"

#include "fried/core/build.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace fried::editor
{
    namespace
    {
        // The Haxe compile in its folder, then the CMake steps this machine builds with, which run with the same PATH.
        std::vector<SubprocessOptions> haxeThenCmake(const std::vector<std::string> &haxe, const std::string &haxeDirectory,
                                                     const std::vector<fried::core::BuildStep> &cmake, const std::string &cmakeDirectory,
                                                     const std::string &logFile, const fried::core::Toolchains &toolchains, std::string *error)
        {
            if (cmake.empty())
            {
                return {};
            }
            if (toolchains.m_haxe.empty())
            {
                *error = "haxe is not on the PATH, so the editor player cannot be built. Install Haxe and hxcpp.";
                return {};
            }

            std::vector<SubprocessOptions> steps = {{haxe, cmake.front().m_environment, haxeDirectory, logFile}};
            for (SubprocessOptions &step : fried::core::subprocessSteps(cmake, cmakeDirectory))
            {
                step.m_logFile = logFile;
                steps.push_back(step);
            }
            return steps;
        }
    }

    fried::core::Toolchains machineToolchains()
    {
        fried::core::Toolchains toolchains = fried::core::detectToolchains(fried::core::managerTools());
        toolchains.m_visualStudioCmake = fried::core::visualStudioCmake();
        return toolchains;
    }

    std::vector<SubprocessOptions> PlayerBuilder::engineSteps(const std::string &enginePath, const std::string &logFile, const fried::core::Toolchains &toolchains,
                                                              std::string *error)
    {
        std::string playerDirectory = enginePath + "/editor/player";
        std::vector<fried::core::BuildStep> cmake = fried::core::Build::cmakeSteps(playerDirectory, playerDirectory + "/build", fried::core::BuildType::Release, {}, toolchains, error);
        return haxeThenCmake({"haxe", "build.hxml"}, playerDirectory, cmake, playerDirectory, logFile, toolchains, error);
    }

    std::vector<SubprocessOptions> PlayerBuilder::projectSteps(const std::string &enginePath, const std::string &projectDirectory, const std::string &logFile,
                                                               const fried::core::Toolchains &toolchains, std::string *error)
    {
        std::vector<fried::core::BuildStep> cmake = fried::core::Build::cmakeSteps(enginePath + "/editor/project-player", projectDirectory + "/build/editor-player",
                                                                                   fried::core::BuildType::Release, {"-DFRIED_PROJECT_DIR=" + projectDirectory}, toolchains, error);
        return haxeThenCmake({"haxe", "editor.hxml"}, projectDirectory, cmake, projectDirectory, logFile, toolchains, error);
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

    std::string playerExecutablePath(const std::string &enginePath, const fried::core::Toolchains &toolchains)
    {
        return fried::core::Build::executablePath(enginePath + "/editor/player/build", "fried_editor_player", fried::core::BuildType::Release, toolchains);
    }
}

namespace fried::editor
{
    bool hasProjectPlayer(const std::string &projectDirectory)
    {
        std::error_code ignored;
        return fs::exists(projectDirectory + "/editor.hxml", ignored);
    }

    std::string playerExecutablePath(const std::string &enginePath, const std::string &projectDirectory, const fried::core::Toolchains &toolchains)
    {
        if (hasProjectPlayer(projectDirectory))
        {
            return fried::core::Build::executablePath(projectDirectory + "/build/editor-player", "fried_project_editor_player", fried::core::BuildType::Release, toolchains);
        }
        return playerExecutablePath(enginePath, toolchains);
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

    bool isPlayerStale(const std::string &enginePath, const std::string &projectDirectory, const fried::core::Toolchains &toolchains)
    {
        std::error_code ignored;
        std::string executable = playerExecutablePath(enginePath, projectDirectory, toolchains);
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
