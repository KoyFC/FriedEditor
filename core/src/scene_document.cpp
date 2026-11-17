#include "fried/editor/scene_document.h"

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
            return field == "id" || field == "type" || field == "children";
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

    SceneDocument SceneDocument::empty(const std::string &name)
    {
        SceneDocument document;
        document.m_data = {{"version", s_version}, {"name", name}, {"nodes", Json::array()}};
        document.m_name = name;
        return document;
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
                *error = "A node in the scene file has no id.";
                return false;
            }
            if (!node.contains("type") || !node["type"].is_string())
            {
                *error = "The node " + id + " has no type.";
                return false;
            }
            if (m_index.count(id))
            {
                *error = "The node id " + id + " is used twice.";
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
        indexList(m_data["nodes"], "", 0, &ignored);
    }

    bool SceneDocument::parse(const std::string &text, std::string *error)
    {
        Json parsed = Json::parse(text, nullptr, false);
        if (!parsed.is_object())
        {
            *error = "The scene file is not a JSON object.";
            return false;
        }
        if (parsed.value("version", 0) != s_version)
        {
            *error = "The scene file is version " + std::to_string(parsed.value("version", 0)) + ", and this editor reads version "
                + std::to_string(s_version) + ". Scenes are rewritten by hand when the engine's format changes.";
            return false;
        }
        if (!parsed.contains("nodes") || !parsed["nodes"].is_array())
        {
            *error = "The scene file has no list of nodes.";
            return false;
        }

        SceneDocument next;
        next.m_data = std::move(parsed);
        if (!next.indexList(next.m_data["nodes"], "", 0, error))
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
        file << dump();
        if (!file)
        {
            *error = "Writing " + path + " failed.";
            return false;
        }
        return true;
    }

    std::string SceneDocument::dump() const
    {
        return m_data.dump(1, '\t') + "\n";
    }

    Json *SceneDocument::listOf(const std::string &parentId)
    {
        if (parentId.empty())
        {
            return &m_data["nodes"];
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

    std::string SceneDocument::typeOf(const std::string &id) const
    {
        const Json *found = node(id);
        return found ? found->value("type", "") : "";
    }

    std::string SceneDocument::parentOf(const std::string &id) const
    {
        auto found = m_index.find(id);
        return found == m_index.end() ? "" : found->second.m_parent;
    }

    std::vector<std::string> SceneDocument::childrenOf(const std::string &id) const
    {
        std::vector<std::string> ids;
        const Json *list = id.empty() ? &m_data["nodes"] : (node(id) && node(id)->contains("children") ? &(*node(id))["children"] : nullptr);
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
            return (int)m_data["nodes"].size();
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
        if (!inserted.contains("type") || !inserted["type"].is_string())
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
        if (!found || isReserved(field))
        {
            return false;
        }
        assign(*found, field, value);
        changed({DocumentChange::Kind::Field, id, field, value ? *value : Json()});
        return true;
    }

    bool SceneDocument::replaceNode(const std::string &id, const Json &contents, Json *before)
    {
        Json *found = find(id);
        if (!found || !contents.is_object() || contents.value("id", "") != id || !contents.contains("type"))
        {
            return false;
        }
        *before = *found;
        before->erase("children");
        Json next = contents;
        next.erase("children");
        if (found->contains("children"))
        {
            next["children"] = (*found)["children"];
        }
        *found = std::move(next);
        changed({DocumentChange::Kind::Structure, id});
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
