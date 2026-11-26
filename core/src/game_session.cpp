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

    bool GameSession::launch(const std::string &executable, const std::string &enginePath, const std::string &gamePath, int width, int height, const std::string &platform, const std::string &logFile, std::string *error)
    {
        if (!m_listener)
        {
            *error = "The game has nowhere to connect to.";
            return false;
        }

        SubprocessOptions options;
        options.m_arguments = {executable,
                               "--mode", "play",
                               "--editor-port", std::to_string(m_listener->port()),
                               "--width", std::to_string(width),
                               "--height", std::to_string(height),
                               "--platform", platform,
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
            for (Message &message : m_channel->poll())
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
            showFrame();
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

    void GameSession::handle(Message &message)
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
            nlohmann::json play = {{"type", "play"}, {"name", m_sceneName}, {"scene", m_scene}, {"fromStart", m_isFromStartScene}};
            if (m_input.is_object() && !m_input.empty())
            {
                play["input"] = m_input;
            }
            send(play);
            send({{"type", "showGame"}, {"visible", m_isShowing}});
            send({{"type", "frameLimit"}, {"fps", m_frameLimit}, {"refresh", m_refreshRate}});
            send({{"type", "watch"}, {"selected", m_watchedId}, {"all", m_isWatchingAll}});
            if (m_speed != 1.0)
            {
                send({{"type", "speed"}, {"scale", m_speed}});
            }
            m_state = State::Running;
        }
        else if (type == "gameFrame")
        {
            // The game stops sending after two frames that were not answered.
            send({{"type", "frameShown"}});
            GameFrame frame;
            frame.m_width = header.value("width", 0);
            frame.m_height = header.value("height", 0);
            frame.m_rgba = std::move(message.m_pixels);
            if (header.contains("stats") && header["stats"].is_object())
            {
                const auto &stats = header["stats"];
                frame.m_stats.m_fps = stats.value("fps", 0.0);
                frame.m_stats.m_frameMs = stats.value("frameMs", 0.0);
                frame.m_stats.m_workMs = stats.value("workMs", 0.0);
                frame.m_stats.m_nodes = stats.value("nodes", 0);
                if (stats.contains("sections") && stats["sections"].is_array())
                {
                    for (const auto &section : stats["sections"])
                    {
                        frame.m_stats.m_sections.push_back(section.is_number() ? section.get<double>() : 0.0);
                    }
                }
            }
            m_arrivedFrames.push_back(std::move(frame));
        }
        else if (type == "sceneChanged")
        {
            if (m_onSceneChanged)
            {
                m_onSceneChanged(header.value("name", ""));
            }
        }
        else if (type == "state")
        {
            if (m_onState && header.contains("nodes") && header["nodes"].is_array())
            {
                m_onState(header["nodes"]);
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

    // A game drawing as fast as the editor seldom lands one frame in every editor frame: it is two in one and none in the next. The oldest is shown at once and the newest is kept for an editor frame that gets none, so every editor frame has a new one. Older frames that both replace are dropped.
    void GameSession::showFrame()
    {
        std::optional<GameFrame> shown;
        if (!m_arrivedFrames.empty())
        {
            shown = std::move(m_arrivedFrames.front());
            m_keptFrame.reset();
            if (m_arrivedFrames.size() > 1)
            {
                m_keptFrame = std::move(m_arrivedFrames.back());
            }
            m_arrivedFrames.clear();
        }
        else if (m_keptFrame)
        {
            shown = std::move(m_keptFrame);
            m_keptFrame.reset();
        }
        if (shown && m_onFrame)
        {
            m_onFrame(std::move(*shown));
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

    void GameSession::setInput(const nlohmann::json &block)
    {
        m_input = block;
        if (m_state == State::Running)
        {
            send({{"type", "inputMap"}, {"input", block.is_object() ? block : nlohmann::json::object()}});
        }
    }

    void GameSession::setPaused(bool isPaused)
    {
        m_isPaused = isPaused;
        if (m_state == State::Running)
        {
            send({{"type", "pause"}, {"paused", isPaused}});
        }
    }

    void GameSession::stepFrame()
    {
        if (m_state == State::Running && m_isPaused)
        {
            send({{"type", "step"}});
        }
    }

    void GameSession::setSpeed(double scale)
    {
        m_speed = scale;
        if (m_state == State::Running)
        {
            send({{"type", "speed"}, {"scale", scale}});
        }
    }

    void GameSession::setWatch(const std::string &selectedId, bool isWatchingAll)
    {
        if (selectedId != m_watchedId || isWatchingAll != m_isWatchingAll)
        {
            m_watchedId = selectedId;
            m_isWatchingAll = isWatchingAll;
            if (m_state == State::Running)
            {
                send({{"type", "watch"}, {"selected", selectedId}, {"all", isWatchingAll}});
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
