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
        options.m_environment = {{"SDL_VIDEODRIVER", "dummy"}, {"SDL_AUDIODRIVER", "dummy"}, {"SDL_RENDER_DRIVER", "software"}};
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
            for (const auto &component : listOf(header, "components"))
            {
                ComponentSchema schema;
                schema.m_name = component.value("name", "");
                for (const auto &field : listOf(component, "fields"))
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
                frame.m_bounds.push_back({bounds.value("id", ""), bounds.value("x", 0.0), bounds.value("y", 0.0), bounds.value("width", 0.0),
                                          bounds.value("height", 0.0)});
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

    void PlayerSession::load(const nlohmann::json &scene, const std::string &platform)
    {
        if (m_channel)
        {
            m_channel->send({{"type", "load"}, {"scene", scene}, {"platform", platform.empty() ? nlohmann::json() : nlohmann::json(platform)}});
        }
    }

    void PlayerSession::setField(const std::string &objectId, const std::string &component, const std::string &field, const nlohmann::json &value)
    {
        if (m_channel)
        {
            m_channel->send({{"type", "setField"}, {"id", objectId}, {"component", component}, {"field", field}, {"value", value}});
        }
    }

    void PlayerSession::setEditorCamera(double x, double y, double zoom)
    {
        if (m_channel)
        {
            m_channel->send({{"type", "setEditorCamera"}, {"x", x}, {"y", y}, {"zoom", zoom}});
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
