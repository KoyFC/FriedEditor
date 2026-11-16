#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fried::editor
{
    // What a change to the document means for whoever is showing it.
    struct DocumentChange
    {
        enum class Kind
        {
            // Nodes were added, removed, moved or changed type, so the whole scene has to be shown again.
            Structure,

            // One value changed, which can be applied in place.
            Field
        };

        Kind m_kind = Kind::Structure;
        std::string m_nodeId;
        std::string m_field;
        nlohmann::json m_value;
    };

    // A scene file in memory, the only copy of it the editor edits. A scene is a tree of nodes: each has a type, an id, and fields, and may have children. Every edit is a method here, which is what undo is written against, and each one reports itself.
    class SceneDocument
    {
    public:
        static constexpr int s_version = 2;

        static SceneDocument empty(const std::string &name);

        bool parse(const std::string &text, std::string *error);
        bool load(const std::string &path, std::string *error);
        bool save(const std::string &path, std::string *error) const;

        // The file's text: tabs, sorted keys and a final newline, so the same scene is always the same bytes.
        std::string dump() const;
        const nlohmann::json &data() const { return m_data; }

        const std::string &name() const { return m_name; }

        // Every node, parents before their children, in the order the scene draws them.
        const std::vector<std::string> &nodeIds() const { return m_order; }
        std::size_t nodeCount() const { return m_order.size(); }
        const nlohmann::json *node(const std::string &id) const;
        bool contains(const std::string &id) const { return m_index.count(id) > 0; }
        std::string typeOf(const std::string &id) const;

        // The root has the empty id: its children are the scene's own nodes.
        std::string parentOf(const std::string &id) const;
        std::vector<std::string> childrenOf(const std::string &id) const;
        int childCount(const std::string &id) const;
        int depthOf(const std::string &id) const;

        // Where a node is among its parent's children.
        int indexOf(const std::string &id) const;
        bool isInside(const std::string &id, const std::string &ancestor) const;
        std::string uniqueId(const std::string &base) const;

        std::optional<std::string> cameraId() const;
        void setCameraId(const std::optional<std::string> &id);

        // The node may carry children of its own, as when a removed subtree is put back, and every id in it has to be new.
        bool insertNode(const std::string &parentId, int index, const nlohmann::json &node);

        // Takes the whole subtree, and the scene's camera with it when it was in it.
        bool removeNode(const std::string &id, nlohmann::json *removed, std::string *parentId, int *index);

        // The index is the node's place among its new parent's children once it has left the old place.
        bool moveNode(const std::string &id, const std::string &parentId, int index);

        // An absent value is a field the file does not mention, which the engine fills with its own default. The name is a field like the others.
        std::optional<nlohmann::json> nodeField(const std::string &id, const std::string &field) const;
        bool setNodeField(const std::string &id, const std::string &field, const std::optional<nlohmann::json> &value);

        // Gives a node other contents, and says what they were. Its children stay.
        bool replaceNode(const std::string &id, const nlohmann::json &contents, nlohmann::json *before);

        bool isModified() const { return m_isModified; }
        void markSaved() { m_isModified = false; }

        std::function<void(const DocumentChange &)> m_onChange;

    private:
        struct Location
        {
            std::string m_parent;
            int m_position = 0;
            int m_depth = 0;
        };

        bool indexList(const nlohmann::json &nodes, const std::string &parent, int depth, std::string *error);
        void reindex();
        nlohmann::json *find(const std::string &id);
        nlohmann::json *listOf(const std::string &parentId);
        const nlohmann::json *listOf(const std::string &parentId) const;
        void changed(const DocumentChange &change);

        nlohmann::json m_data;
        std::string m_name;
        bool m_isModified = false;
        std::unordered_map<std::string, Location> m_index;
        std::vector<std::string> m_order;
    };
}
