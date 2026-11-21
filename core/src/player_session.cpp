#include "fried/editor/player_session.h"

namespace fried::editor
{
    namespace
    {
        // A key that is missing, or is not a list, reads as an empty list rather than as a crash.
        const nlohmann::json &listOf(const nlohmann::json &object, const char *key)
        {
            static const nlohmann::json s_empty = nlohmann::json::array();
            auto found = object.find(key);
            return found != object.end() && found->is_array() ? *found : s_empty;
        }
    }

    bool PlayerSession::listen(std::string *error)
    {
        m_listener = listenOnLoopback(error);
        return m_listener != nullptr;
    }

    bool PlayerSession::launch(const std::string &executable, const std::string &enginePath, const std::string &gamePath, int width, int height,
                               std::string *error)
    {
        if (!m_listener && !listen(error))
        {
            return false;
        }
        SubprocessOptions options;
        options.m_arguments = {executable,
                               "--editor-port", std::to_string(m_listener->port()),
                               "--width", std::to_string(width),
                               "--height", std::to_string(height),
                               "--engine-assets", enginePath + "/assets",
                               "--game-assets", gamePath + "/assets"};

        // No display is needed to draw, which is also what lets it run on any session.
        options.m_environment = {{"SDL_VIDEODRIVER", "offscreen"}, {"SDL_AUDIODRIVER", "dummy"}};
        m_process = startSubprocess(options, error);
        return m_process != nullptr;
    }

    void PlayerSession::pump()
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
                m_onError("The player stopped (exit code " + std::to_string(m_process->exitCode()) + ").");
            }
        }
    }

    void PlayerSession::handle(const Message &message)
    {
        const nlohmann::json &header = message.m_header;
        std::string type = header.value("type", "");

        if (type == "hello")
        {
            if (header.value("protocol", 0) != s_protocol)
            {
                m_state = State::Closed;
                if (m_onError)
                {
                    m_onError("The player speaks protocol " + std::to_string(header.value("protocol", 0)) + ", and this editor speaks "
                              + std::to_string(s_protocol) + ". Rebuild the player.");
                }
                return;
            }
            m_platform = header.value("platform", "pc");
            m_channel->send({{"type", "schema"}});
        }
        else if (type == "schema")
        {
            m_schema.clear();
            for (const auto &entry : listOf(header, "classes"))
            {
                ClassSchema schema;
                schema.m_name = entry.value("name", "");
                schema.m_parent = entry.contains("parent") && entry["parent"].is_string() ? entry["parent"].get<std::string>() : "";
                for (const auto &field : listOf(entry, "fields"))
                {
                    FieldSchema fieldSchema;
                    fieldSchema.m_name = field.value("name", "");
                    fieldSchema.m_kind = field.value("kind", "");
                    for (const auto &value : listOf(field, "values"))
                    {
                        fieldSchema.m_values.push_back(value.get<std::string>());
                    }
                    fieldSchema.m_default = field.contains("defaultValue") ? field["defaultValue"] : nlohmann::json();
                    schema.m_fields.push_back(std::move(fieldSchema));
                }
                m_schema.push_back(std::move(schema));
            }
            m_state = State::Ready;
            if (m_onReady)
            {
                m_onReady();
            }
        }
        else if (type == "loaded")
        {
            if (m_onLoaded)
            {
                m_onLoaded();
            }
        }
        else if (type == "frame")
        {
            Frame frame;
            frame.m_width = header.value("width", 0);
            frame.m_height = header.value("height", 0);
            frame.m_rgba = message.m_pixels;
            for (const auto &bounds : listOf(header, "bounds"))
            {
                ObjectBounds box{bounds.value("id", ""), bounds.value("x", 0.0), bounds.value("y", 0.0), bounds.value("width", 0.0),
                                 bounds.value("height", 0.0), std::nullopt};
                if (bounds.contains("originX") && bounds.contains("originY") && bounds["originX"].is_number() && bounds["originY"].is_number())
                {
                    box.m_origin = std::make_pair(bounds["originX"].get<double>(), bounds["originY"].get<double>());
                }
                if (bounds.contains("parent") && bounds["parent"].is_object())
                {
                    const auto &parent = bounds["parent"];
                    box.m_parent = ParentValues{parent.value("x", 0.0),      parent.value("y", 0.0),      parent.value("rotation", 0.0), parent.value("scaleX", 1.0),
                                                parent.value("scaleY", 1.0), parent.value("width", 0.0),  parent.value("height", 0.0)};
                }
                frame.m_bounds.push_back(std::move(box));
            }
            for (const auto &entry : listOf(header, "bounds3D"))
            {
                ObjectBounds3D box;
                box.m_id = entry.value("id", "");
                box.m_x = entry.value("x", 0.0);
                box.m_y = entry.value("y", 0.0);
                box.m_width = entry.value("width", 0.0);
                box.m_height = entry.value("height", 0.0);
                box.m_depth = entry.value("depth", 0.0);
                box.m_originX = entry.value("originX", 0.0);
                box.m_originY = entry.value("originY", 0.0);
                box.m_worldX = entry.value("worldX", 0.0);
                box.m_worldY = entry.value("worldY", 0.0);
                box.m_worldZ = entry.value("worldZ", 0.0);
                for (const auto &axis : listOf(entry, "axes"))
                {
                    box.m_axes.emplace_back(axis.value("x", 0.0), axis.value("y", 0.0));
                }
                frame.m_bounds3D.push_back(std::move(box));
            }
            for (const auto &entry : listOf(header, "frusta"))
            {
                Frustum frustum;
                frustum.m_id = entry.value("id", "");
                for (const auto &number : listOf(entry, "corners"))
                {
                    frustum.m_corners.push_back(number.is_number() ? number.get<double>() : 0.0);
                }
                if (frustum.m_corners.size() == 24)
                {
                    frame.m_frusta.push_back(std::move(frustum));
                }
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
                m_onError(header.value("message", "The player reported an error."));
            }
        }
    }

    void PlayerSession::load(const nlohmann::json &scene, int viewWidth, int viewHeight)
    {
        if (m_channel)
        {
            m_channel->send({{"type", "load"}, {"scene", scene}, {"viewWidth", viewWidth}, {"viewHeight", viewHeight}});
        }
    }

    void PlayerSession::setField(const std::string &nodeId, const std::string &field, const nlohmann::json &value)
    {
        if (m_channel)
        {
            m_channel->send({{"type", "setField"}, {"id", nodeId}, {"field", field}, {"value", value}});
        }
    }

    void PlayerSession::addNode(const std::string &parentId, int index, const nlohmann::json &node)
    {
        if (m_channel)
        {
            m_channel->send({{"type", "addNode"}, {"parent", parentId}, {"index", index}, {"node", node}});
        }
    }

    void PlayerSession::removeNode(const std::string &nodeId)
    {
        if (m_channel)
        {
            m_channel->send({{"type", "removeNode"}, {"id", nodeId}});
        }
    }

    void PlayerSession::moveNode(const std::string &nodeId, const std::string &parentId, int index)
    {
        if (m_channel)
        {
            m_channel->send({{"type", "moveNode"}, {"id", nodeId}, {"parent", parentId}, {"index", index}});
        }
    }

    const ClassSchema *PlayerSession::classNamed(const std::string &name) const
    {
        for (const ClassSchema &schema : m_schema)
        {
            if (schema.m_name == name)
            {
                return &schema;
            }
        }
        return nullptr;
    }

    std::vector<const ClassSchema *> PlayerSession::classChain(const std::string &name) const
    {
        std::vector<const ClassSchema *> chain;
        for (const ClassSchema *schema = classNamed(name); schema && chain.size() < m_schema.size(); schema = classNamed(schema->m_parent))
        {
            chain.push_back(schema);
        }
        return chain;
    }

    void PlayerSession::setEditorCamera(double x, double y, double zoom)
    {
        if (m_channel)
        {
            m_channel->send({{"type", "setEditorCamera"}, {"x", x}, {"y", y}, {"zoom", zoom}});
        }
    }

    void PlayerSession::setEditorCamera3D(double x, double y, double z, double rotationX, double rotationY, double fieldOfView)
    {
        if (m_channel)
        {
            m_channel->send({{"type", "setEditorCamera3D"}, {"x", x}, {"y", y}, {"z", z}, {"rotationX", rotationX}, {"rotationY", rotationY}, {"fieldOfView", fieldOfView}});
        }
    }

    void PlayerSession::setEditorView(bool is3D)
    {
        if (m_channel)
        {
            m_channel->send({{"type", "setEditorView"}, {"mode", is3D ? "3d" : "2d"}});
        }
    }

    void PlayerSession::setPreview(bool isEnabled)
    {
        if (m_channel)
        {
            m_channel->send({{"type", "setPreview"}, {"enabled", isEnabled}});
        }
    }

    void PlayerSession::resize(int width, int height)
    {
        if (m_channel)
        {
            m_channel->send({{"type", "resize"}, {"width", width}, {"height", height}});
        }
    }

    void PlayerSession::render()
    {
        if (m_channel)
        {
            m_channel->send({{"type", "render"}});
        }
    }
}
