#pragma once

#include "fried/editor/core_types.h"
#include "fried/editor/message_channel.h"
#include "fried/editor/transport.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fried::editor
{
    // What the running game says about itself every half second.
    struct GameStats
    {
        double m_fps = 0.0;
        // The milliseconds a whole frame takes, waiting included, and the part of it spent updating and drawing.
        double m_frameMs = 0.0;
        double m_workMs = 0.0;
        int m_nodes = 0;
    };

    // One frame of the game, at the size of its display.
    struct GameFrame
    {
        int m_width = 0;
        int m_height = 0;
        std::vector<unsigned char> m_rgba;
        GameStats m_stats;
    };

    // The editor's side of the link to the game it is playing: the project's player started in play mode, which runs the game and sends back its frames. The editor tells it which scene to open, the changes the user makes while it runs and the keys and the mouse.
    class GameSession
    {
    public:
        enum class State
        {
            Waiting,
            Running,
            Closed
        };

        // Starts waiting on a loopback port for the game, which is told to open the scene when it says hello: the one it is given, or the project's start scene.
        bool listen(const std::string &sceneName, const nlohmann::json &scene, bool isFromStartScene, std::string *error);
        int port() const { return m_listener ? m_listener->port() : 0; }

        // Runs the player in play mode, which connects back, drawing at the given size, the area its cameras show, and treating the platform key as the one it runs on for what a platform decides, such as which on-screen controls show. What the game prints goes to the log file.
        bool launch(const std::string &executable, const std::string &enginePath, const std::string &gamePath, int width, int height, const std::string &platform, const std::string &logFile, std::string *error);

        // Accepts the player and delivers what it has sent to the callbacks. Called often, from the editor's own thread.
        void pump();

        // Ends the game.
        void stop();

        State state() const { return m_state; }

        void setField(const std::string &nodeId, const std::string &field, const nlohmann::json &value);
        void addNode(const std::string &parentId, int index, const nlohmann::json &node);
        void removeNode(const std::string &nodeId);
        void moveNode(const std::string &nodeId, const std::string &parentId, int index);

        // How many frames a second the game is held to: 0 for as many as it can draw, or a negative number for the project's own rule, which follows the rate of the display when the project waits for vsync. The display's rate is what the editor is shown on.
        void setFrameLimit(int fps, int refreshRate);

        // The input actions the game runs with, the `input` block of the project as the editor has it now. They go with `play`, and a change while the game runs replaces them in it, so no build is needed to try a binding.
        void setInput(const nlohmann::json &block);

        // A paused game draws and listens to the editor but runs no update and no physics. A step lets one frame through while it is paused, and the speed is a factor on the game's own time scale.
        void setPaused(bool isPaused);
        void stepFrame();
        void setSpeed(double scale);
        bool isPaused() const { return m_isPaused; }
        double speed() const { return m_speed; }

        // What the game reports the values of: the node being inspected, and every node while the scene view is on screen.
        void setWatch(const std::string &selectedId, bool isWatchingAll);

        // The game sends no frames while nothing shows them.
        void setShowing(bool isShowing);

        // A key by its SDL scancode, and the mouse in the pixels of the game's display.
        void key(int scancode, bool isDown);
        void mouseMove(int x, int y);
        void mouseButton(int button, int x, int y, bool isDown);
        void mouseWheel(double x, double y);

        std::function<void(GameFrame &&)> m_onFrame;
        // The fields that changed in the game since it last said, as a list of {id, fields}.
        std::function<void(const nlohmann::json &)> m_onState;
        // The game is now on another scene, by the name of its file without the extension.
        std::function<void(const std::string &)> m_onSceneChanged;
        // The game answered a change with an error, or the link broke, in which case the state is Closed.
        std::function<void(const std::string &)> m_onError;

    private:
        void handle(Message &message);
        void showFrame();
        void send(const nlohmann::json &message);

        std::unique_ptr<Listener> m_listener;
        std::unique_ptr<Subprocess> m_process;
        std::unique_ptr<MessageChannel> m_channel;
        std::vector<GameFrame> m_arrivedFrames;
        std::optional<GameFrame> m_keptFrame;
        State m_state = State::Waiting;
        std::string m_sceneName;
        nlohmann::json m_scene;
        bool m_isFromStartScene = false;
        bool m_isShowing = true;
        std::string m_watchedId;
        bool m_isWatchingAll = false;
        nlohmann::json m_input;
        bool m_isPaused = false;
        double m_speed = 1.0;
        int m_frameLimit = -1;
        int m_refreshRate = 60;
    };
}
