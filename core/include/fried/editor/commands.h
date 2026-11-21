#pragma once

#include "fried/editor/command_stack.h"

#include <map>
#include <optional>
#include <set>

namespace fried::editor
{
    // Gives a node's field a value, or with no value takes it out of the file so the engine's default applies. The name is a field too.
    class SetNodeField : public Command
    {
    public:
        SetNodeField(std::string id, std::string field, std::optional<nlohmann::json> value);

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

    // Sets several numeric fields at once, so a drag of a node or a handle is one step.
    class SetNodeFields : public Command
    {
    public:
        SetNodeFields(std::string id, std::map<std::string, double> fields);

        std::string label() const override { return "Change " + (m_fields.size() == 1 ? m_fields.begin()->first : std::string("node")); }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;
        bool mergeWith(const Command &later) override;

    private:
        std::string m_id;
        std::map<std::string, double> m_fields;
        std::map<std::string, std::optional<nlohmann::json>> m_before;
    };

    class AddNode : public Command
    {
    public:
        AddNode(std::string parentId, int index, nlohmann::json node, std::string label = "Add node");

        std::string label() const override { return m_label; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_parentId;
        int m_index;
        nlohmann::json m_node;
        std::string m_label;
    };

    // Takes the node's children with it, and its place as the scene's camera, and gives them back on undo.
    class RemoveNode : public Command
    {
    public:
        explicit RemoveNode(std::string id);

        std::string label() const override { return "Remove node"; }
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

    // Reorders a node among its siblings or moves it under another node.
    class MoveNode : public Command
    {
    public:
        MoveNode(std::string id, std::string parentId, int index);

        std::string label() const override { return "Move node"; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_id;
        std::string m_parentId;
        int m_index;
        std::string m_beforeParentId;
        int m_beforeIndex = 0;
    };

    // Gives a node another class. It keeps the fields the new class also has and drops the others, and undo brings all of them back. The caller says which fields the new class has, since only the player knows.
    class ChangeNodeType : public Command
    {
    public:
        ChangeNodeType(std::string id, std::string type, std::set<std::string> keptFields);

        std::string label() const override { return "Change type to " + m_type; }
        bool apply(SceneDocument &document) override;
        void revert(SceneDocument &document) override;

    private:
        std::string m_id;
        std::string m_type;
        std::set<std::string> m_keptFields;
        nlohmann::json m_before;
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
