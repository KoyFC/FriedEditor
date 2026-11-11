#pragma once

#include "fried/editor/command_stack.h"

#include <map>
#include <optional>

namespace fried::editor
{
    // Gives a component's field a value, or with no value takes it out of the file so the engine's default applies.
    class SetComponentField : public Command
    {
    public:
        SetComponentField(std::string id, std::string type, std::string field, std::optional<nlohmann::json> value);

        std::string label() const override { return "Change " + m_field; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;
        bool mergeWith(const Command &later) override;

    private:
        std::string m_id;
        std::string m_type;
        std::string m_field;
        std::optional<nlohmann::json> m_value;
        std::optional<nlohmann::json> m_before;
    };

    // The same for what the object itself holds: name, active and priority.
    class SetObjectField : public Command
    {
    public:
        SetObjectField(std::string id, std::string field, std::optional<nlohmann::json> value);

        std::string label() const override { return "Change " + m_field; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;
        bool mergeWith(const Command &later) override;

    private:
        std::string m_id;
        std::string m_field;
        std::optional<nlohmann::json> m_value;
        std::optional<nlohmann::json> m_before;
    };

    // Moves an object by setting its Transform's x and y together, so dragging it is one step.
    class SetPosition : public Command
    {
    public:
        SetPosition(std::string id, double x, double y);

        std::string label() const override { return "Move object"; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;
        bool mergeWith(const Command &later) override;

    private:
        std::string m_id;
        double m_x;
        double m_y;
        std::optional<nlohmann::json> m_beforeX;
        std::optional<nlohmann::json> m_beforeY;
    };

    // Sets several Transform fields at once, so a drag of a handle is one step.
    class SetTransformFields : public Command
    {
    public:
        SetTransformFields(std::string id, std::map<std::string, double> fields);

        std::string label() const override { return "Transform object"; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;
        bool mergeWith(const Command &later) override;

    private:
        std::string m_id;
        std::map<std::string, double> m_fields;
        std::map<std::string, std::optional<nlohmann::json>> m_before;
    };

    class AddObject : public Command
    {
    public:
        AddObject(int index, nlohmann::json object);

        std::string label() const override { return "Add object"; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        int m_index;
        nlohmann::json m_object;
    };

    // Takes its place as the scene's camera with it, and gives them back on undo.
    class RemoveObject : public Command
    {
    public:
        explicit RemoveObject(std::string id);

        std::string label() const override { return "Remove object"; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_id;
        nlohmann::json m_object;
        int m_index = 0;
        bool m_wasCamera = false;
    };

    class MoveObject : public Command
    {
    public:
        MoveObject(std::string id, int index);

        std::string label() const override { return "Reorder object"; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_id;
        int m_index;
        int m_before = 0;
    };

    class AddComponent : public Command
    {
    public:
        AddComponent(std::string id, int index, nlohmann::json component);

        std::string label() const override { return "Add component"; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_id;
        int m_index;
        nlohmann::json m_component;
    };

    class RemoveComponent : public Command
    {
    public:
        RemoveComponent(std::string id, std::string type);

        std::string label() const override { return "Remove component"; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_id;
        std::string m_type;
        nlohmann::json m_component;
        int m_index = 0;
    };

    class SetCamera : public Command
    {
    public:
        explicit SetCamera(std::optional<std::string> id);

        std::string label() const override { return "Set camera"; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::optional<std::string> m_id;
        std::optional<std::string> m_before;
    };
}
