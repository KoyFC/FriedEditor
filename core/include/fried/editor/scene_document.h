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
            // An object changed the class of its transform, so the whole scene has to be shown again.
            Structure,

            // An object, with its children as m_value, was put in the tree at m_index under m_parentId.
            Added,
            Removed,
            Moved,

            // One value changed, which can be applied in place. m_component says whose, and is empty for the object's own.
            Field,

            // A component, as m_value, was put in the object's list at m_index, or taken out of it.
            ComponentAdded,
            ComponentRemoved,

            // Which object is the scene's camera changed, which nothing shown depends on.
            Camera
        };

        Kind m_kind = Kind::Structure;
        std::string m_nodeId;
        std::string m_field;
        nlohmann::json m_value;
        std::string m_parentId;
        int m_index = 0;
        std::string m_component;
        int m_componentIndex = 0;
        bool m_isTransform = false;
    };

    // Which part of an object a field belongs to. With no type it is the object itself, its name and whether it is active. The transform is not in the list of components, so it is named by its flag, and a component is named by its class and its place among the object's components of that class.
    struct ComponentRef
    {
        std::string m_type;
        int m_index = 0;
        bool m_isTransform = false;

        bool isObject() const { return m_type.empty() && !m_isTransform; }
        bool operator==(const ComponentRef &other) const { return m_type == other.m_type && m_index == other.m_index && m_isTransform == other.m_isTransform; }
    };

    // A scene file in memory, the only copy of it the editor edits. A scene is a tree of objects: each has an id, a transform, a list of components with a type and fields each, and may have children. Every edit is a method here, which is what undo is written against, and each one reports itself.
    class SceneDocument
    {
    public:
        static constexpr int s_version = 3;

        static SceneDocument empty(const std::string &name);

        // A new scene as the editor makes one: its one root object, with a transform of the given type or a plain one when it is empty, named like the scene.
        static SceneDocument withRoot(const std::string &name, const std::string &rootType);

        // The id of the scene's only top-level node, which every scene has. A file with none or several is given a plain root holding them when it is read, and says so here.
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
        // The class of the object's transform, Transform when it has none. An instance of another scene has the transform of that scene's root, which the document cannot know, so it asks m_instanceRoot.
        std::string transformTypeOf(const std::string &id) const;

        // The components in the order the object holds them, an instance's own after those of the scene it instances.
        std::vector<ComponentRef> componentsOf(const std::string &id) const;

        // The first component's class, or the transform's when the object has none, which is what stands for the object in a list.
        std::string primaryTypeOf(const std::string &id) const;

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

        // An absent value is a field the file does not mention, which the engine fills with its own default. These are the object's own fields: its name, whether it is active, and the scene it instances.
        std::optional<nlohmann::json> nodeField(const std::string &id, const std::string &field) const;
        bool setNodeField(const std::string &id, const std::string &field, const std::optional<nlohmann::json> &value);

        // The same for a field of the transform or of a component. An instance does not repeat what the scene it instances says, so the transform or the component it changes is made when a value is set and goes when the last value does.
        std::optional<nlohmann::json> componentField(const std::string &id, const ComponentRef &component, const std::string &field) const;
        bool setComponentField(const std::string &id, const ComponentRef &component, const std::string &field, const std::optional<nlohmann::json> &value);

        // The component, with its type, is put in the object's list at the place, which is what the file keeps, and the place it was at is said when it is taken out.
        bool insertComponent(const std::string &id, int position, const nlohmann::json &component);
        bool removeComponent(const std::string &id, const ComponentRef &component, nlohmann::json *removed, int *position);

        // Gives the object another transform, null for a plain one, and says what it was. Its children stay.
        bool replaceTransform(const std::string &id, const nlohmann::json &transform, nlohmann::json *before);

        // Changes with every edit and is never the same for two documents, so what is derived from a document can tell whether it is out of date.
        unsigned long version() const { return m_version; }

        bool isModified() const { return m_isModified; }
        void markSaved() { m_isModified = false; }

        std::function<void(const DocumentChange &)> m_onChange;

        // The root object of a scene by the scene's name, null when there is none.
        std::function<nlohmann::json(const std::string &)> m_instanceRoot;

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
        nlohmann::json *componentJson(const std::string &id, const ComponentRef &component, bool create);
        const nlohmann::json *componentJson(const std::string &id, const ComponentRef &component) const;

        nlohmann::json m_data;
        std::string m_name;
        bool m_wasMigrated = false;
        bool m_isModified = false;
        unsigned long m_version = 0;
        std::unordered_map<std::string, Location> m_index;
        std::vector<std::string> m_order;
    };
}
