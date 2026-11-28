#include "fried/editor/scene_document.h"
#include "fried/core/scene_files.h"
#include "fried/core/yaml.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

namespace fried::editor
{
    using Json = nlohmann::json;

    namespace
    {
        unsigned long s_versionCounter = 0;

        // Sets or erases one key.
        void assign(Json &node, const std::string &key, const std::optional<Json> &value)
        {
            if (value)
            {
                node[key] = *value;
            }
            else
            {
                node.erase(key);
            }
        }

        bool isReserved(const std::string &field)
        {
            return field == "id" || field == "children" || field == "transform" || field == "components";
        }

        // Whether the list of components is a list of maps that each name a class, and what is wrong when it is not.
        bool checkObject(const Json &node, const std::string &id, std::string *error)
        {
            if (node.contains("transform") && (!node["transform"].is_object() || (node["transform"].contains("type") && !node["transform"]["type"].is_string())))
            {
                *error = "The transform of " + id + " is not a map with a type.";
                return false;
            }
            if (node.contains("components"))
            {
                if (!node["components"].is_array())
                {
                    *error = "The components of " + id + " are not a list.";
                    return false;
                }
                for (const Json &component : node["components"])
                {
                    if (!component.is_object() || !component.contains("type") || !component["type"].is_string())
                    {
                        *error = "A component of " + id + " has no type.";
                        return false;
                    }
                }
            }
            if (node.contains("instance"))
            {
                if (!node["instance"].is_string())
                {
                    *error = "The instance of " + id + " is not the name of a scene.";
                    return false;
                }
                if (node.contains("children"))
                {
                    *error = "The instance " + id + " has children, which an instance cannot have.";
                    return false;
                }
            }
            return true;
        }

        // The place of a component among those of its class, which is how the engine names it.
        int positionOf(const Json &list, const ComponentRef &component)
        {
            int seen = 0;
            for (std::size_t position = 0; position < list.size(); ++position)
            {
                if (list[position].value("type", "") == component.m_type)
                {
                    if (seen == component.m_index)
                    {
                        return (int)position;
                    }
                    seen++;
                }
            }
            return -1;
        }

        int indexAmongClass(const Json &list, int position)
        {
            int seen = 0;
            for (int index = 0; index < position; ++index)
            {
                if (list[index].value("type", "") == list[position].value("type", ""))
                {
                    seen++;
                }
            }
            return seen;
        }

        void collectIds(const Json &node, std::vector<std::string> *ids)
        {
            ids->push_back(node.value("id", ""));
            auto children = node.find("children");
            if (children != node.end() && children->is_array())
            {
                for (const Json &child : *children)
                {
                    collectIds(child, ids);
                }
            }
        }
    }

    namespace
    {
        bool checkSubtree(const Json &node, std::string *error)
        {
            if (!node.is_object() || !checkObject(node, node.value("id", ""), error))
            {
                return false;
            }
            auto children = node.find("children");
            if (children != node.end() && children->is_array())
            {
                for (const Json &child : *children)
                {
                    if (!checkSubtree(child, error))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        std::string slugOf(const std::string &text)
        {
            std::string slug;
            for (unsigned char c : text)
            {
                if (std::isalnum(c))
                {
                    slug += (char)std::tolower(c);
                }
                else if (!slug.empty() && slug.back() != '-')
                {
                    slug += '-';
                }
            }
            while (!slug.empty() && slug.back() == '-')
            {
                slug.pop_back();
            }
            return slug.empty() ? "root" : slug;
        }
    }

    SceneDocument SceneDocument::empty(const std::string &name)
    {
        SceneDocument document;
        document.m_data = {{"version", s_version}, {"name", name}, {"objects", Json::array()}};
        document.m_name = name;
        return document;
    }

    SceneDocument SceneDocument::withRoot(const std::string &name, const std::string &rootType)
    {
        SceneDocument document;
        Json root = {{"id", slugOf(name)}, {"name", name}};
        if (!rootType.empty() && rootType != "Transform")
        {
            root["transform"] = {{"type", rootType}};
        }
        document.m_data = {{"version", s_version}, {"name", name}, {"objects", Json::array({root})}};
        document.m_name = name;
        document.reindex();
        return document;
    }

    std::string SceneDocument::rootId() const
    {
        const Json &nodes = m_data["objects"];
        return nodes.is_array() && !nodes.empty() && nodes[0].is_object() ? nodes[0].value("id", "") : std::string();
    }

    // Walks one list of nodes, checking each and recording where it is.
    bool SceneDocument::indexList(const Json &nodes, const std::string &parent, int depth, std::string *error)
    {
        int position = 0;
        for (const Json &node : nodes)
        {
            std::string id = node.is_object() ? node.value("id", "") : "";
            if (id.empty())
            {
                *error = "An object in the scene file has no id.";
                return false;
            }
            if (!checkObject(node, id, error))
            {
                return false;
            }
            if (m_index.count(id))
            {
                *error = "The object id " + id + " is used twice.";
                return false;
            }
            m_index[id] = {parent, position++, depth};
            m_order.push_back(id);
            auto children = node.find("children");
            if (children != node.end())
            {
                if (!children->is_array())
                {
                    *error = "The children of " + id + " are not a list.";
                    return false;
                }
                if (!indexList(*children, id, depth + 1, error))
                {
                    return false;
                }
            }
        }
        return true;
    }

    void SceneDocument::reindex()
    {
        m_index.clear();
        m_order.clear();
        std::string ignored;
        indexList(m_data["objects"], "", 0, &ignored);
    }

    bool SceneDocument::parse(const std::string &text, std::string *error)
    {
        Json parsed;
        std::string problem;
        if (!fried::core::parseYaml(text, &parsed, &problem))
        {
            *error = "The scene file is not valid YAML: " + problem;
            return false;
        }
        if (!parsed.is_object())
        {
            *error = "The scene file is not a map of keys and values.";
            return false;
        }
        if (parsed.value("version", 0) != s_version)
        {
            *error = "The scene file is version " + std::to_string(parsed.value("version", 0)) + ", and this editor reads version "
                + std::to_string(s_version) + ". Run fried_import migrate-scenes on the project to rewrite its scenes.";
            return false;
        }
        if (!parsed.contains("objects") || !parsed["objects"].is_array())
        {
            *error = "The scene file has no list of objects.";
            return false;
        }

        SceneDocument next;
        next.m_data = std::move(parsed);
        if (next.m_data["objects"].size() != 1)
        {
            // A scene has exactly one root, so the objects of another file go under a plain root that changes nothing about them.
            std::vector<std::string> used;
            for (const Json &node : next.m_data["objects"])
            {
                collectIds(node, &used);
            }
            std::string name = next.m_data.value("name", "Scene");
            std::string id = slugOf(name) + "-root";
            while (std::find(used.begin(), used.end(), id) != used.end())
            {
                id += "-x";
            }
            Json root = {{"id", id}, {"name", name}};
            if (!next.m_data["objects"].empty())
            {
                root["children"] = next.m_data["objects"];
            }
            next.m_data["objects"] = Json::array({root});
            next.m_wasMigrated = true;
        }
        if (!next.indexList(next.m_data["objects"], "", 0, error))
        {
            return false;
        }
        next.m_name = next.m_data.value("name", "");
        next.m_version = ++s_versionCounter;
        next.m_onChange = std::move(m_onChange);
        *this = std::move(next);
        m_isModified = false;
        return true;
    }

    bool SceneDocument::load(const std::string &path, std::string *error)
    {
        std::ifstream file(path);
        if (!file)
        {
            *error = "Cannot open " + path + ".";
            return false;
        }
        std::stringstream text;
        text << file.rdbuf();
        return parse(text.str(), error);
    }

    bool SceneDocument::save(const std::string &path, std::string *error) const
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            *error = "Cannot write " + path + ".";
            return false;
        }
        file << fried::core::sceneText(path, m_data);
        if (!file)
        {
            *error = "Writing " + path + " failed.";
            return false;
        }
        return true;
    }

    std::string SceneDocument::dump() const
    {
        return fried::core::emitYaml(m_data, fried::core::YamlLayout::Scene);
    }

    Json *SceneDocument::listOf(const std::string &parentId)
    {
        if (parentId.empty())
        {
            return &m_data["objects"];
        }
        Json *parent = find(parentId);
        if (!parent)
        {
            return nullptr;
        }
        Json &list = (*parent)["children"];
        if (list.is_null())
        {
            list = Json::array();
        }
        return &list;
    }

    const Json *SceneDocument::listOf(const std::string &parentId) const
    {
        return const_cast<SceneDocument *>(this)->listOf(parentId);
    }

    Json *SceneDocument::find(const std::string &id)
    {
        auto found = m_index.find(id);
        if (found == m_index.end())
        {
            return nullptr;
        }
        Json *list = listOf(found->second.m_parent);
        return list && found->second.m_position < (int)list->size() ? &(*list)[found->second.m_position] : nullptr;
    }

    const Json *SceneDocument::node(const std::string &id) const
    {
        return const_cast<SceneDocument *>(this)->find(id);
    }

    std::string SceneDocument::transformTypeOf(const std::string &id) const
    {
        const Json *found = node(id);
        if (!found)
        {
            return "";
        }
        if (found->contains("transform") && (*found)["transform"].contains("type"))
        {
            return (*found)["transform"]["type"].get<std::string>();
        }
        std::string scene = instanceOf(id);
        if (!scene.empty() && m_instanceRoot)
        {
            Json root = m_instanceRoot(scene);
            if (root.is_object() && root.contains("transform") && root["transform"].contains("type"))
            {
                return root["transform"]["type"].get<std::string>();
            }
        }
        return "Transform";
    }

    std::vector<ComponentRef> SceneDocument::componentsOf(const std::string &id) const
    {
        std::vector<ComponentRef> result;
        const Json *found = node(id);
        if (!found)
        {
            return result;
        }
        auto add = [&](const Json &list) {
            for (std::size_t position = 0; position < list.size(); ++position)
            {
                ComponentRef ref;
                ref.m_type = list[position].value("type", "");
                ref.m_index = indexAmongClass(list, (int)position);
                if (std::find(result.begin(), result.end(), ref) == result.end())
                {
                    result.push_back(ref);
                }
            }
        };
        std::string scene = instanceOf(id);
        if (!scene.empty() && m_instanceRoot)
        {
            Json root = m_instanceRoot(scene);
            if (root.is_object() && root.contains("components") && root["components"].is_array())
            {
                add(root["components"]);
            }
        }
        if (found->contains("components") && (*found)["components"].is_array())
        {
            add((*found)["components"]);
        }
        return result;
    }

    std::string SceneDocument::primaryTypeOf(const std::string &id) const
    {
        std::vector<ComponentRef> components = componentsOf(id);
        return components.empty() ? transformTypeOf(id) : components.front().m_type;
    }

    std::string SceneDocument::instanceOf(const std::string &id) const
    {
        const Json *found = node(id);
        return found && found->contains("instance") && (*found)["instance"].is_string() ? (*found)["instance"].get<std::string>() : "";
    }

    std::string SceneDocument::parentOf(const std::string &id) const
    {
        auto found = m_index.find(id);
        return found == m_index.end() ? "" : found->second.m_parent;
    }

    std::vector<std::string> SceneDocument::childrenOf(const std::string &id) const
    {
        std::vector<std::string> ids;
        const Json *list = id.empty() ? &m_data["objects"] : (node(id) && node(id)->contains("children") ? &(*node(id))["children"] : nullptr);
        if (list)
        {
            for (const Json &child : *list)
            {
                ids.push_back(child.value("id", ""));
            }
        }
        return ids;
    }

    int SceneDocument::childCount(const std::string &id) const
    {
        if (id.empty())
        {
            return (int)m_data["objects"].size();
        }
        const Json *found = node(id);
        return found && found->contains("children") ? (int)(*found)["children"].size() : 0;
    }

    int SceneDocument::depthOf(const std::string &id) const
    {
        auto found = m_index.find(id);
        return found == m_index.end() ? -1 : found->second.m_depth;
    }

    int SceneDocument::indexOf(const std::string &id) const
    {
        auto found = m_index.find(id);
        return found == m_index.end() ? -1 : found->second.m_position;
    }

    bool SceneDocument::isInside(const std::string &id, const std::string &ancestor) const
    {
        for (std::string current = parentOf(id); !current.empty(); current = parentOf(current))
        {
            if (current == ancestor)
            {
                return true;
            }
        }
        return false;
    }

    std::string SceneDocument::uniqueId(const std::string &base) const
    {
        if (!m_index.count(base))
        {
            return base;
        }
        for (int number = 2;; ++number)
        {
            std::string candidate = base + "-" + std::to_string(number);
            if (!m_index.count(candidate))
            {
                return candidate;
            }
        }
    }

    std::optional<std::string> SceneDocument::cameraId() const
    {
        auto found = m_data.find("camera");
        return found != m_data.end() && found->is_string() ? std::optional<std::string>(found->get<std::string>()) : std::nullopt;
    }

    void SceneDocument::setCameraId(const std::optional<std::string> &id)
    {
        if (id)
        {
            m_data["camera"] = *id;
        }
        else
        {
            m_data.erase("camera");
        }
        changed({DocumentChange::Kind::Camera});
    }

    bool SceneDocument::insertNode(const std::string &parentId, int index, const Json &inserted)
    {
        std::vector<std::string> ids;
        collectIds(inserted, &ids);
        std::set<std::string> unique;
        for (const std::string &id : ids)
        {
            if (id.empty() || m_index.count(id) || !unique.insert(id).second)
            {
                return false;
            }
        }
        std::string problem;
        if (!checkSubtree(inserted, &problem))
        {
            return false;
        }
        if (!parentId.empty() && !contains(parentId))
        {
            return false;
        }
        Json *list = listOf(parentId);
        if (!list || index < 0 || index > (int)list->size())
        {
            return false;
        }
        list->insert(list->begin() + index, inserted);
        reindex();
        changed({DocumentChange::Kind::Added, inserted.value("id", ""), "", inserted, parentId, index});
        return true;
    }

    bool SceneDocument::removeNode(const std::string &id, Json *removed, std::string *parentId, int *index)
    {
        auto found = m_index.find(id);
        if (found == m_index.end())
        {
            return false;
        }
        *parentId = found->second.m_parent;
        *index = found->second.m_position;
        Json *list = listOf(*parentId);
        *removed = (*list)[*index];
        std::vector<std::string> gone;
        collectIds(*removed, &gone);
        list->erase(list->begin() + *index);
        auto camera = cameraId();
        if (camera && std::find(gone.begin(), gone.end(), *camera) != gone.end())
        {
            m_data.erase("camera");
        }
        // A parent left with no children stops mentioning them, so a file never holds an empty list.
        if (!parentId->empty() && list->empty())
        {
            find(*parentId)->erase("children");
        }
        reindex();
        changed({DocumentChange::Kind::Removed, id, "", Json(), *parentId, *index});
        return true;
    }

    bool SceneDocument::moveNode(const std::string &id, const std::string &parentId, int index)
    {
        if (!contains(id) || (!parentId.empty() && !contains(parentId)) || parentId == id || isInside(parentId, id))
        {
            return false;
        }
        int size = childCount(parentId) - (parentOf(id) == parentId ? 1 : 0);
        if (index < 0 || index > size)
        {
            return false;
        }

        // The removal and the insertion are one change, so only the last is reported.
        auto handler = std::move(m_onChange);
        m_onChange = nullptr;
        Json moving;
        std::string oldParent;
        int oldIndex = 0;
        std::optional<std::string> camera = cameraId();
        removeNode(id, &moving, &oldParent, &oldIndex);
        listOf(parentId)->insert(listOf(parentId)->begin() + index, moving);
        if (camera)
        {
            m_data["camera"] = *camera;
        }
        reindex();
        m_onChange = std::move(handler);
        changed({DocumentChange::Kind::Moved, id, "", Json(), parentId, index});
        return true;
    }

    std::optional<Json> SceneDocument::nodeField(const std::string &id, const std::string &field) const
    {
        const Json *found = node(id);
        if (!found || !found->contains(field))
        {
            return std::nullopt;
        }
        return (*found)[field];
    }

    bool SceneDocument::setNodeField(const std::string &id, const std::string &field, const std::optional<Json> &value)
    {
        Json *found = find(id);
        bool isInstanceChoice = field == "instance" && value && value->is_string() && found && found->contains("instance");
        if (!found || ((isReserved(field) || field == "instance") && !isInstanceChoice))
        {
            return false;
        }
        assign(*found, field, value);
        changed({DocumentChange::Kind::Field, id, field, value ? *value : Json()});
        return true;
    }

    bool SceneDocument::replaceTransform(const std::string &id, const Json &transform, Json *before)
    {
        Json *found = find(id);
        if (!found || (!transform.is_null() && !transform.is_object()))
        {
            return false;
        }
        *before = found->contains("transform") ? (*found)["transform"] : Json();
        if (transform.is_null())
        {
            found->erase("transform");
        }
        else
        {
            (*found)["transform"] = transform;
        }
        changed({DocumentChange::Kind::Structure, id});
        return true;
    }

    Json *SceneDocument::componentJson(const std::string &id, const ComponentRef &component, bool create)
    {
        Json *found = find(id);
        if (!found)
        {
            return nullptr;
        }
        bool isInstance = found->contains("instance");
        if (component.m_isTransform)
        {
            if (!found->contains("transform"))
            {
                if (!create || !isInstance)
                {
                    return nullptr;
                }
                (*found)["transform"] = Json::object();
            }
            return &(*found)["transform"];
        }
        if (!found->contains("components"))
        {
            if (!create || !isInstance)
            {
                return nullptr;
            }
            (*found)["components"] = Json::array();
        }
        Json &list = (*found)["components"];
        int position = positionOf(list, component);
        if (position < 0)
        {
            if (!create || !isInstance || component.m_index != 0)
            {
                return nullptr;
            }
            list.push_back({{"type", component.m_type}});
            position = (int)list.size() - 1;
        }
        return &list[position];
    }

    const Json *SceneDocument::componentJson(const std::string &id, const ComponentRef &component) const
    {
        return const_cast<SceneDocument *>(this)->componentJson(id, component, false);
    }

    std::optional<Json> SceneDocument::componentField(const std::string &id, const ComponentRef &component, const std::string &field) const
    {
        if (component.isObject())
        {
            return nodeField(id, field);
        }
        const Json *found = componentJson(id, component);
        if (!found || !found->contains(field) || field == "type")
        {
            return std::nullopt;
        }
        return (*found)[field];
    }

    bool SceneDocument::setComponentField(const std::string &id, const ComponentRef &component, const std::string &field, const std::optional<Json> &value)
    {
        if (component.isObject())
        {
            return setNodeField(id, field, value);
        }
        if (field == "type")
        {
            return false;
        }
        Json *found = componentJson(id, component, value.has_value());
        if (!found)
        {
            return false;
        }
        assign(*found, field, value);

        // What an instance no longer changes stops being mentioned.
        Json *object = find(id);
        if (object->contains("instance"))
        {
            if (component.m_isTransform)
            {
                if (found->empty())
                {
                    object->erase("transform");
                }
            }
            else if (found->size() == 1)
            {
                Json &list = (*object)["components"];
                list.erase(list.begin() + positionOf(list, component));
                if (list.empty())
                {
                    object->erase("components");
                }
            }
        }
        DocumentChange change{DocumentChange::Kind::Field, id, field, value ? *value : Json()};
        change.m_component = component.m_type;
        change.m_componentIndex = component.m_index;
        change.m_isTransform = component.m_isTransform;
        changed(change);
        return true;
    }

    bool SceneDocument::insertComponent(const std::string &id, int position, const Json &component)
    {
        Json *found = find(id);
        if (!found || !component.is_object() || !component.contains("type") || !component["type"].is_string())
        {
            return false;
        }
        Json &list = (*found)["components"];
        if (list.is_null())
        {
            list = Json::array();
        }
        if (position < 0 || position > (int)list.size())
        {
            return false;
        }
        list.insert(list.begin() + position, component);
        DocumentChange change{DocumentChange::Kind::ComponentAdded, id, "", component, "", position};
        change.m_component = component["type"].get<std::string>();
        change.m_componentIndex = indexAmongClass(list, position);
        changed(change);
        return true;
    }

    bool SceneDocument::removeComponent(const std::string &id, const ComponentRef &component, Json *removed, int *position)
    {
        Json *found = find(id);
        if (!found || component.m_isTransform || !found->contains("components"))
        {
            return false;
        }
        Json &list = (*found)["components"];
        int at = positionOf(list, component);
        if (at < 0)
        {
            return false;
        }
        *removed = list[at];
        *position = at;
        list.erase(list.begin() + at);
        if (list.empty())
        {
            found->erase("components");
        }
        DocumentChange change{DocumentChange::Kind::ComponentRemoved, id, "", Json(), "", at};
        change.m_component = component.m_type;
        change.m_componentIndex = component.m_index;
        changed(change);
        return true;
    }

    void SceneDocument::changed(const DocumentChange &change)
    {
        m_isModified = true;
        m_version = ++s_versionCounter;
        if (m_onChange)
        {
            m_onChange(change);
        }
    }
}
