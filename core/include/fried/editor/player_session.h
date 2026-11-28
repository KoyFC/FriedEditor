#pragma once

#include "fried/editor/input_map.h"
#include "fried/editor/message_channel.h"
#include "fried/editor/modules.h"
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

    // A class of transform or component: the class it extends, and the fields it declares itself, which are not the ones it inherits.
    struct ClassSchema
    {
        std::string m_name;
        std::string m_parent;

        // "transform" for the classes that place an object, "component" for the rest.
        std::string m_kind = "component";

        // The transform class an object needs to have this component, empty when any will do.
        std::string m_requires;

        // The module and backend the class belongs to, empty for a class that belongs to none.
        std::string m_module;
        std::string m_backend;

        // Whether the engine has the class, as opposed to the project.
        bool m_isBuiltin = false;
        std::vector<FieldSchema> m_fields;
    };

    struct ResourceSchema
    {
        std::string m_name;
        std::string m_category;
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

    // Where a Node3D lands on the frame, as the player measures it through the editor's 3D camera.
    struct ObjectBounds3D
    {
        std::string m_id;
        double m_x = 0;
        double m_y = 0;
        double m_width = 0;
        double m_height = 0;
        double m_depth = 0;
        double m_originX = 0;
        double m_originY = 0;
        double m_worldX = 0;
        double m_worldY = 0;
        double m_worldZ = 0;
        // Where one more unit of the node's x, y and z lands, or empty when one of them is behind the camera.
        std::vector<std::pair<double, double>> m_axes;
    };

    // What a Camera3D sees, as eight corners in the world: the four of its near plane and then the four of a plane in front of it that stands for the far end, each going round the plane.
    struct Frustum
    {
        std::string m_id;
        std::vector<double> m_corners;
    };

    struct Frame
    {
        int m_width = 0;
        int m_height = 0;
        std::vector<unsigned char> m_rgba;
        std::vector<ObjectBounds> m_bounds;
        std::vector<ObjectBounds3D> m_bounds3D;
        std::vector<Frustum> m_frusta;
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

        static constexpr int s_protocol = 8;

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

        // The names of keys, buttons and axes the engine writes input actions with, which arrive with the schema.
        const InputNames &inputNames() const { return m_inputNames; }
        const ClassSchema *classNamed(const std::string &name) const;
        // The modules of the engine the player was built from, which arrive with the schema.
        const std::vector<ModuleSchema> &modules() const { return m_modules; }

        const std::vector<ResourceSchema> &resources() const { return m_resources; }
        const ResourceSchema *resourceNamed(const std::string &name) const;

        // The class and every class above it, the most derived first, which is the order an inspector shows them in.
        std::vector<const ClassSchema *> classChain(const std::string &name) const;

        void load(const nlohmann::json &scene, int viewWidth, int viewHeight);
        // The empty component is the object itself: its name and whether it is active.
        void setField(const std::string &objectId, const std::string &component, int index, const std::string &field, const nlohmann::json &value);
        // Several fields of several objects at once, as a list of {id, components: [{type, index, fields}]}.
        void setFields(const nlohmann::json &objects);

        // Changes the player's objects in place. The empty parent is the top of the scene, and the index is the place among the new siblings once the object has left its old one.
        void addObject(const std::string &parentId, int index, const nlohmann::json &object);
        void removeObject(const std::string &objectId);
        void moveObject(const std::string &objectId, const std::string &parentId, int index);
        void addComponent(const std::string &objectId, const nlohmann::json &component);
        void removeComponent(const std::string &objectId, const std::string &type, int index);
        void setEditorCamera(double x, double y, double zoom);
        void setEditorCamera3D(double x, double y, double z, double rotationX, double rotationY, double fieldOfView);
        // Which of the scene's two kinds of drawing the player shows, the flat nodes or the meshes.
        void setEditorView(bool is3D);
        // Whether the player draws the scene as the game would, through the cameras the scene names, instead of through the editor's.
        void setPreview(bool isEnabled);
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
        std::vector<ModuleSchema> m_modules;
        std::vector<ClassSchema> m_schema;
        std::vector<ResourceSchema> m_resources;
        InputNames m_inputNames;
    };
}
