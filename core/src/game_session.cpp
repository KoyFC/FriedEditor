#include "fried/editor/game_session.h"

#include "fried/editor/player_session.h"

namespace fried::editor
{
    bool GameSession::listen(const std::string &sceneName, const nlohmann::json &scene, bool isFromStartScene, std::string *error)
    {
        m_listener = listenOnLoopback(error);
        m_sceneName = sceneName;
        m_scene = scene;
        m_isFromStartScene = isFromStartScene;
        return m_listener != nullptr;
    }

    bool GameSession::launch(const std::string &executable, const std::string &enginePath, const std::string &gamePath, const std::string &logFile, std::string *error)
    {
        if (!m_listener)
        {
            *error = "The game has nowhere to connect to.";
            return false;
        }

        // The size is the display's own, which the game reads from project.fried, so it is only there to satisfy the player's arguments.
        SubprocessOptions options;
        options.m_arguments = {executable,
                               "--mode", "play",
                               "--editor-port", std::to_string(m_listener->port()),
                               "--width", "640",
                               "--height", "480",
                               "--engine-assets", enginePath + "/assets",
                               "--game-assets", gamePath + "/assets"};

        // Nothing is drawn on a screen, but the game has its sound.
        options.m_environment = {{"SDL_VIDEODRIVER", "offscreen"}};
        options.m_logFile = logFile;
        m_process = startSubprocess(options, error);
        return m_process != nullptr;
    }

    void GameSession::stop()
    {
        if (m_channel)
        {
            m_channel->send({{"type", "quit"}});
        }
        m_process.reset();
        m_channel.reset();
        m_listener.reset();
        m_state = State::Closed;
    }

    void GameSession::pump()
    {
        if (m_state == State::Closed)
        {
            return;
        }
        if (!m_channel && m_listener)
        {
            if (auto transport = m_listener->accept())
            {
                m_channel = std::make_unique<MessageChannel>(std::move(transport));
            }
        }
        if (m_channel)
        {
            for (const Message &message : m_channel->poll())
            {
                handle(message);
                if (m_state == State::Closed)
                {
                    return;
                }
            }
            if (!m_channel->isOpen())
            {
                m_state = State::Closed;
            }
        }
        if (m_process && !m_process->isRunning() && m_state != State::Closed)
        {
            m_state = State::Closed;
            if (m_onError)
            {
                m_onError("The game stopped (exit code " + std::to_string(m_process->exitCode()) + ").");
            }
        }
    }

    void GameSession::handle(const Message &message)
    {
        const nlohmann::json &header = message.m_header;
        std::string type = header.value("type", "");

        if (type == "hello")
        {
            if (header.value("protocol", 0) != PlayerSession::s_protocol)
            {
                m_state = State::Closed;
                if (m_onError)
                {
                    m_onError("The game speaks protocol " + std::to_string(header.value("protocol", 0)) + ", and this editor speaks " + std::to_string(PlayerSession::s_protocol)
                              + ". Build the game again.");
                }
                return;
            }
            send({{"type", "play"}, {"name", m_sceneName}, {"scene", m_scene}, {"fromStart", m_isFromStartScene}});
            send({{"type", "showGame"}, {"visible", m_isShowing}});
            send({{"type", "frameLimit"}, {"fps", m_frameLimit}, {"refresh", m_refreshRate}});
            m_state = State::Running;
        }
        else if (type == "gameFrame")
        {
            // The game stops sending after two frames that were not answered.
            send({{"type", "frameShown"}});
            GameFrame frame;
            frame.m_width = header.value("width", 0);
            frame.m_height = header.value("height", 0);
            frame.m_rgba = message.m_pixels;
            if (header.contains("stats") && header["stats"].is_object())
            {
                const auto &stats = header["stats"];
                frame.m_stats.m_fps = stats.value("fps", 0.0);
                frame.m_stats.m_frameMs = stats.value("frameMs", 0.0);
                frame.m_stats.m_workMs = stats.value("workMs", 0.0);
                frame.m_stats.m_nodes = stats.value("nodes", 0);
            }
            if (m_onFrame)
            {
                m_onFrame(frame);
            }
        }
        else if (type == "error")
        {
            if (m_onError)
            {
                m_onError(header.value("message", "The game reported an error."));
            }
        }
    }

    void GameSession::send(const nlohmann::json &message)
    {
        if (m_channel)
        {
            m_channel->send(message);
        }
    }

    void GameSession::setField(const std::string &nodeId, const std::string &field, const nlohmann::json &value)
    {
        send({{"type", "setField"}, {"id", nodeId}, {"field", field}, {"value", value}});
    }

    void GameSession::addNode(const std::string &parentId, int index, const nlohmann::json &node)
    {
        send({{"type", "addNode"}, {"parent", parentId}, {"index", index}, {"node", node}});
    }

    void GameSession::removeNode(const std::string &nodeId)
    {
        send({{"type", "removeNode"}, {"id", nodeId}});
    }

    void GameSession::moveNode(const std::string &nodeId, const std::string &parentId, int index)
    {
        send({{"type", "moveNode"}, {"id", nodeId}, {"parent", parentId}, {"index", index}});
    }

    void GameSession::setFrameLimit(int fps, int refreshRate)
    {
        if (fps != m_frameLimit || refreshRate != m_refreshRate)
        {
            m_frameLimit = fps;
            m_refreshRate = refreshRate;
            if (m_state == State::Running)
            {
                send({{"type", "frameLimit"}, {"fps", fps}, {"refresh", refreshRate}});
            }
        }
    }

    void GameSession::setShowing(bool isShowing)
    {
        if (isShowing != m_isShowing)
        {
            m_isShowing = isShowing;
            send({{"type", "showGame"}, {"visible", isShowing}});
        }
    }

    void GameSession::key(int scancode, bool isDown)
    {
        send({{"type", "key"}, {"code", scancode}, {"down", isDown}});
    }

    void GameSession::mouseMove(int x, int y)
    {
        send({{"type", "mouseMove"}, {"x", x}, {"y", y}});
    }

    void GameSession::mouseButton(int button, int x, int y, bool isDown)
    {
        send({{"type", "mouseButton"}, {"button", button}, {"x", x}, {"y", y}, {"down", isDown}});
    }

    void GameSession::mouseWheel(double x, double y)
    {
        send({{"type", "mouseWheel"}, {"x", x}, {"y", y}});
    }
}
