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
            // A node changed class, so the whole scene has to be shown again.
            Structure,

            // A node, with its children as m_value, was put in the tree at m_index under m_parentId.
            Added,
            Removed,
            Moved,

            // One value changed, which can be applied in place.
            Field,

            // Which node is the scene's camera changed, which nothing shown depends on.
            Camera
        };

        Kind m_kind = Kind::Structure;
        std::string m_nodeId;
        std::string m_field;
        nlohmann::json m_value;
        std::string m_parentId;
        int m_index = 0;
    };

    // A scene file in memory, the only copy of it the editor edits. A scene is a tree of nodes: each has a type, an id, and fields, and may have children. Every edit is a method here, which is what undo is written against, and each one reports itself.
    class SceneDocument
    {
    public:
        static constexpr int s_version = 2;

        static SceneDocument empty(const std::string &name);

        // A new scene as the editor makes one: its one root node, of the given type, named like the scene.
        static SceneDocument withRoot(const std::string &name, const std::string &rootType);

        // The id of the scene's only top-level node, which every scene has. A file with none or several is given a Node root holding them when it is read, and says so here.
        std::string rootId() const;
        bool wasMigrated() const { return m_wasMigrated; }

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
        // The class of the node. An instance of another scene has the class of that scene's root, which the document cannot know, so it asks m_instanceType.
        std::string typeOf(const std::string &id) const;

        // The name of the scene an entry instances, or empty for a node of its own.
        std::string instanceOf(const std::string &id) const;

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

        // Changes with every edit and is never the same for two documents, so what is derived from a document can tell whether it is out of date.
        unsigned long version() const { return m_version; }

        bool isModified() const { return m_isModified; }
        void markSaved() { m_isModified = false; }

        std::function<void(const DocumentChange &)> m_onChange;

        // The class of the root of a scene by its name, empty when there is none.
        std::function<std::string(const std::string &)> m_instanceType;

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
        bool m_wasMigrated = false;
        bool m_isModified = false;
        unsigned long m_version = 0;
        std::unordered_map<std::string, Location> m_index;
        std::vector<std::string> m_order;
    };
}
