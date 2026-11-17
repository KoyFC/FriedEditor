#pragma once

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
    struct FieldSchema
    {
        std::string m_name;
        std::string m_kind;
        std::vector<std::string> m_values;
        nlohmann::json m_default;
    };

    // A class of node: the class it extends, and the fields it declares itself, which are not the ones it inherits.
    struct ClassSchema
    {
        std::string m_name;
        std::string m_parent;
        std::vector<FieldSchema> m_fields;
    };

    // What a node's parent gives it in the world, so a position in the world can be turned into one in the parent's space.
    struct ParentValues
    {
        double m_x = 0;
        double m_y = 0;
        double m_rotation = 0;
        double m_scaleX = 1;
        double m_scaleY = 1;
        double m_width = 0;
        double m_height = 0;
    };

    // Where a node is in the world, as the player measures it.
    struct ObjectBounds
    {
        std::string m_id;
        double m_x = 0;
        double m_y = 0;
        double m_width = 0;
        double m_height = 0;
        std::optional<ParentValues> m_parent;

        // Where the node is in the world, which its box does not always start at. A player that does not say leaves it empty.
        std::optional<std::pair<double, double>> m_origin;
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

        static constexpr int s_protocol = 2;

        // Starts waiting on a loopback port, which the player is told to connect to.
        bool listen(std::string *error);
        int port() const { return m_listener ? m_listener->port() : 0; }

        // Runs the player, which connects back.
        bool launch(const std::string &executable, const std::string &enginePath, const std::string &gamePath, int width, int height, std::string *error);

        // Accepts the player and delivers what it has sent to the callbacks. Called often, from the editor's own thread.
        void pump();

        State state() const { return m_state; }
        const std::string &platform() const { return m_platform; }
        const std::vector<ClassSchema> &schema() const { return m_schema; }
        const ClassSchema *classNamed(const std::string &name) const;

        // The class and every class above it, the most derived first, which is the order an inspector shows them in.
        std::vector<const ClassSchema *> classChain(const std::string &name) const;

        void load(const nlohmann::json &scene, int viewWidth, int viewHeight);
        void setField(const std::string &nodeId, const std::string &field, const nlohmann::json &value);

        // Changes the player's tree in place. The empty parent is the top of the scene, and the index is the place among the new siblings once the node has left its old one.
        void addNode(const std::string &parentId, int index, const nlohmann::json &node);
        void removeNode(const std::string &nodeId);
        void moveNode(const std::string &nodeId, const std::string &parentId, int index);
        void setEditorCamera(double x, double y, double zoom);
        void resize(int width, int height);
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
        std::vector<ClassSchema> m_schema;
    };
}
