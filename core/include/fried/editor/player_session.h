#pragma once

#include "fried/editor/message_channel.h"
#include "fried/editor/transport.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fried::editor
{
    struct FieldSchema
    {
        std::string m_name;
        std::string m_kind;
        std::vector<std::string> m_values;
        nlohmann::json m_default;
    };

    struct ComponentSchema
    {
        std::string m_name;
        std::vector<FieldSchema> m_fields;
    };

    // Where an object is in the world, as the player measures it.
    struct ObjectBounds
    {
        std::string m_id;
        double m_x = 0;
        double m_y = 0;
        double m_width = 0;
        double m_height = 0;
    };

    struct Frame
    {
        int m_width = 0;
        int m_height = 0;
        std::vector<unsigned char> m_rgba;
        std::vector<ObjectBounds> m_bounds;
    };

    // The editor's side of the link to the editor player: it waits for the player to connect, asks for what the editor needs and hands back the answers.
    class PlayerSession
    {
    public:
        enum class State
        {
            Waiting,
            Ready,
            Closed
        };

        static constexpr int s_protocol = 1;

        // Starts waiting on a loopback port, which the player is told to connect to.
        bool listen(std::string *error);
        int port() const { return m_listener ? m_listener->port() : 0; }

        // Runs the player, which connects back.
        bool launch(const std::string &executable, const std::string &enginePath, const std::string &gamePath, int width, int height, std::string *error);

        // Accepts the player and delivers what it has sent to the callbacks. Called often, from the editor's own thread.
        void pump();

        State state() const { return m_state; }
        const std::string &platform() const { return m_platform; }
        const std::vector<ComponentSchema> &schema() const { return m_schema; }

        void load(const nlohmann::json &scene, const std::string &platform);
        void setField(const std::string &objectId, const std::string &component, const std::string &field, const nlohmann::json &value);
        void setEditorCamera(double x, double y, double zoom);
        void render();

        std::function<void()> m_onReady;
        std::function<void()> m_onLoaded;
        std::function<void(const Frame &)> m_onFrame;
        std::function<void(const std::string &)> m_onError;

    private:
        void handle(const Message &message);

        std::unique_ptr<Listener> m_listener;
        std::unique_ptr<Subprocess> m_process;
        std::unique_ptr<MessageChannel> m_channel;
        State m_state = State::Waiting;
        std::string m_platform;
        std::vector<ComponentSchema> m_schema;
    };
}
