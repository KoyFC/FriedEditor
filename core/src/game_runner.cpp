#include "fried/editor/game_runner.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace fried::editor
{
    std::string GameRunner::targetName(const std::string &projectDirectory)
    {
        std::ifstream file(projectDirectory + "/CMakeLists.txt");
        if (!file)
        {
            return std::string();
        }
        std::stringstream text;
        text << file.rdbuf();
        std::string content = text.str();
        std::smatch match;
        if (std::regex_search(content, match, std::regex(R"(fried_add_game\(\s*([A-Za-z0-9_.-]+))")))
        {
            return match[1].str();
        }
        return std::string();
    }

    std::vector<SubprocessOptions> GameRunner::steps(const std::string &projectDirectory, const std::string &logFile)
    {
        return {
            {{"haxe", "build.hxml", "-debug"}, {}, projectDirectory, logFile},
            {{"cmake", "-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=Debug"}, {}, projectDirectory, logFile},
            {{"cmake", "--build", "build", "--config", "Debug", "--parallel"}, {}, projectDirectory, logFile},
        };
    }

    std::string GameRunner::executablePath(const std::string &projectDirectory)
    {
        return projectDirectory + "/build/" + targetName(projectDirectory);
    }

    bool GameRunner::start(const std::string &projectDirectory, const std::string &logFile, std::string *error)
    {
        if (m_state == State::Building || m_state == State::Running)
        {
            return true;
        }
        if (targetName(projectDirectory).empty())
        {
            *error = "The project's CMakeLists.txt has no fried_add_game, so there is no game to build.";
            m_state = State::Failed;
            m_failure = *error;
            return false;
        }
        m_projectDirectory = projectDirectory;
        m_logFile = logFile;
        m_failure.clear();
        m_builder = std::make_unique<PlayerBuilder>(steps(projectDirectory, logFile));
        m_state = State::Building;
        return true;
    }

    GameRunner::State GameRunner::pump()
    {
        if (m_state == State::Building)
        {
            PlayerBuilder::State built = m_builder->pump();
            if (built == PlayerBuilder::State::Failed)
            {
                m_failure = m_builder->failure() + " The log is " + m_logFile;
                m_builder.reset();
                m_state = State::Failed;
            }
            else if (built == PlayerBuilder::State::Succeeded)
            {
                m_builder.reset();
                std::string error;
                m_game = startSubprocess({{executablePath(m_projectDirectory)}, {}, m_projectDirectory + "/build", m_logFile}, &error);
                if (m_game)
                {
                    m_state = State::Running;
                }
                else
                {
                    m_failure = error;
                    m_state = State::Failed;
                }
            }
        }
        else if (m_state == State::Running && !m_game->isRunning())
        {
            m_game.reset();
            m_state = State::Idle;
        }
        return m_state;
    }

    void GameRunner::stop()
    {
        if (m_game)
        {
            m_game->terminate();
            m_game.reset();
        }
        m_builder.reset();
        m_state = State::Idle;
    }
}
