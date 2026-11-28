#pragma once

#include "fried/editor/command_stack.h"

#include <map>
#include <optional>
#include <set>

namespace fried::editor
{
    // Gives a field of an object, its transform or one of its components a value, or with no value takes it out of the file so the engine's default applies. The name is a field of the object.
    class SetField : public Command
    {
    public:
        SetField(std::string id, ComponentRef component, std::string field, std::optional<nlohmann::json> value);

        std::string label() const override { return "Change " + m_field; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;
        bool mergeWith(const Command &later) override;

    private:
        std::string m_id;
        ComponentRef m_component;
        std::string m_field;
        std::optional<nlohmann::json> m_value;
        std::optional<nlohmann::json> m_before;
    };

    // Takes fields out of a component's data, so the engine's defaults (or the instanced scene's values) apply again, as one step.
    class ClearFields : public Command
    {
    public:
        ClearFields(std::string id, ComponentRef component, std::vector<std::string> fields);

        std::string label() const override { return "Revert " + (m_fields.size() == 1 ? m_fields.front() : std::string("fields")); }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_id;
        ComponentRef m_component;
        std::vector<std::string> m_fields;
        std::vector<std::optional<nlohmann::json>> m_before;
    };

    // Sets several numeric fields at once, so a drag of an object or a handle is one step.
    class SetFields : public Command
    {
    public:
        SetFields(std::string id, ComponentRef component, std::map<std::string, double> fields);

        std::string label() const override { return "Change " + (m_fields.size() == 1 ? m_fields.begin()->first : std::string("object")); }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;
        bool mergeWith(const Command &later) override;

    private:
        std::string m_id;
        ComponentRef m_component;
        std::map<std::string, double> m_fields;
        std::map<std::string, std::optional<nlohmann::json>> m_before;
    };

    class AddObject : public Command
    {
    public:
        AddObject(std::string parentId, int index, nlohmann::json node, std::string label = "Add object");

        std::string label() const override { return m_label; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_parentId;
        int m_index;
        nlohmann::json m_node;
        std::string m_label;
    };

    // Takes the object's children with it, and its place as the scene's camera, and gives them back on undo.
    class RemoveObject : public Command
    {
    public:
        explicit RemoveObject(std::string id);

        std::string label() const override { return "Remove object"; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_id;
        std::string m_parentId;
        nlohmann::json m_node;
        int m_index = 0;
        bool m_wasCamera = false;
        std::optional<std::string> m_camera;
    };

    // Reorders an object among its siblings or moves it under another object.
    class MoveObject : public Command
    {
    public:
        MoveObject(std::string id, std::string parentId, int index);

        std::string label() const override { return "Move object"; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_id;
        std::string m_parentId;
        int m_index;
        std::string m_beforeParentId;
        int m_beforeIndex = 0;
    };

    // Gives an object a transform of another class. It keeps the fields the new class also has and drops the others, and undo brings all of them back. The caller says which fields the new class has, since only the player knows. An empty type is a plain transform.
    class ChangeTransformType : public Command
    {
    public:
        ChangeTransformType(std::string id, std::string type, std::set<std::string> keptFields);

        std::string label() const override { return "Change transform to " + (m_type.empty() ? std::string("Transform") : m_type); }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_id;
        std::string m_type;
        std::set<std::string> m_keptFields;
        nlohmann::json m_before;
    };

    class AddComponent : public Command
    {
    public:
        AddComponent(std::string id, nlohmann::json component, int position = -1);

        std::string label() const override { return "Add " + m_component.value("type", "component"); }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_id;
        nlohmann::json m_component;
        int m_position;
        ComponentRef m_ref;
    };

    // Keeps the component and its place in the list, so undo puts it back exactly where it was.
    class RemoveComponent : public Command
    {
    public:
        RemoveComponent(std::string id, ComponentRef component);

        std::string label() const override { return "Remove " + m_component.m_type; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_id;
        ComponentRef m_component;
        nlohmann::json m_removed;
        int m_position = 0;
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
