#include "fried/editor/scene_document.h"

#include <fstream>
#include <set>
#include <sstream>

namespace fried::editor
{
    using Json = nlohmann::json;

    namespace
    {
        Json *findComponentIn(Json &object, const std::string &type)
        {
            for (Json &component : object["components"])
            {
                if (component.value("type", "") == type)
                {
                    return &component;
                }
            }
            return nullptr;
        }

        // Sets or erases one key, and reports whether the object changed.
        void assign(Json &object, const std::string &key, const std::optional<Json> &value)
        {
            if (value)
            {
                object[key] = *value;
            }
            else
            {
                object.erase(key);
            }
        }
    }

    SceneDocument SceneDocument::empty(const std::string &name)
    {
        SceneDocument document;
        document.m_data = {{"version", s_version}, {"name", name}, {"objects", Json::array()}};
        document.m_name = name;
        return document;
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
                + std::to_string(s_version) + ".";
            return false;
        }
        if (!parsed.contains("objects") || !parsed["objects"].is_array())
        {
            *error = "The scene file has no list of objects.";
            return false;
        }
        for (Json &object : parsed["objects"])
        {
            if (!object.is_object() || !object.value("id", "").size())
            {
                *error = "An object in the scene file has no id.";
                return false;
            }
            if (!object.contains("components"))
            {
                object["components"] = Json::array();
            }
        }
        m_data = std::move(parsed);
        m_name = m_data.value("name", "");
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

    std::vector<std::string> SceneDocument::objectIds() const
    {
        std::vector<std::string> ids;
        for (const Json &object : m_data["objects"])
        {
            ids.push_back(object.value("id", ""));
        }
        return ids;
    }

    const Json *SceneDocument::object(const std::string &id) const
    {
        for (const Json &candidate : m_data["objects"])
        {
            if (candidate.value("id", "") == id)
            {
                return &candidate;
            }
        }
        return nullptr;
    }

    int SceneDocument::indexOf(const std::string &id) const
    {
        int index = 0;
        for (const Json &candidate : m_data["objects"])
        {
            if (candidate.value("id", "") == id)
            {
                return index;
            }
            ++index;
        }
        return -1;
    }

    std::string SceneDocument::uniqueId(const std::string &base) const
    {
        std::set<std::string> taken;
        for (const std::string &id : objectIds())
        {
            taken.insert(id);
        }
        if (!taken.count(base))
        {
            return base;
        }
        for (int number = 2;; ++number)
        {
            std::string candidate = base + "-" + std::to_string(number);
            if (!taken.count(candidate))
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
        changed({DocumentChange::Kind::Structure});
    }

    bool SceneDocument::insertObject(int index, const Json &object)
    {
        std::string id = object.value("id", "");
        Json &objects = m_data["objects"];
        if (id.empty() || this->object(id) || index < 0 || index > (int)objects.size())
        {
            return false;
        }
        objects.insert(objects.begin() + index, object);
        changed({DocumentChange::Kind::Structure, id});
        return true;
    }

    bool SceneDocument::removeObject(const std::string &id, Json *removed, int *index)
    {
        int position = indexOf(id);
        if (position < 0)
        {
            return false;
        }
        Json &objects = m_data["objects"];
        *removed = objects[position];
        *index = position;
        objects.erase(objects.begin() + position);
        if (cameraId() == id)
        {
            m_data.erase("camera");
        }
        changed({DocumentChange::Kind::Structure, id});
        return true;
    }

    bool SceneDocument::moveObject(const std::string &id, int index)
    {
        Json &objects = m_data["objects"];
        int position = indexOf(id);
        if (position < 0 || index < 0 || index >= (int)objects.size())
        {
            return false;
        }
        Json moved = objects[position];
        objects.erase(objects.begin() + position);
        objects.insert(objects.begin() + index, moved);
        changed({DocumentChange::Kind::Structure, id});
        return true;
    }

    Json *SceneDocument::findObject(const std::string &id)
    {
        for (Json &candidate : m_data["objects"])
        {
            if (candidate.value("id", "") == id)
            {
                return &candidate;
            }
        }
        return nullptr;
    }

    Json *SceneDocument::findComponent(const std::string &id, const std::string &type)
    {
        Json *object = findObject(id);
        return object ? findComponentIn(*object, type) : nullptr;
    }

    std::optional<Json> SceneDocument::objectField(const std::string &id, const std::string &field) const
    {
        const Json *found = object(id);
        if (!found || !found->contains(field))
        {
            return std::nullopt;
        }
        return (*found)[field];
    }

    bool SceneDocument::setObjectField(const std::string &id, const std::string &field, const std::optional<Json> &value)
    {
        Json *found = findObject(id);
        if (!found || field == "id" || field == "components")
        {
            return false;
        }
        assign(*found, field, value);
        changed({DocumentChange::Kind::Field, id, "", field, value ? *value : Json()});
        return true;
    }

    bool SceneDocument::insertComponent(const std::string &id, int index, const Json &component)
    {
        Json *found = findObject(id);
        std::string type = component.value("type", "");
        if (!found || type.empty() || findComponentIn(*found, type))
        {
            return false;
        }
        Json &components = (*found)["components"];
        if (index < 0 || index > (int)components.size())
        {
            return false;
        }
        components.insert(components.begin() + index, component);
        changed({DocumentChange::Kind::Structure, id});
        return true;
    }

    bool SceneDocument::removeComponent(const std::string &id, const std::string &type, Json *removed, int *index)
    {
        Json *found = findObject(id);
        if (!found || type == "Transform")
        {
            return false;
        }
        Json &components = (*found)["components"];
        for (std::size_t position = 0; position < components.size(); ++position)
        {
            if (components[position].value("type", "") == type)
            {
                *removed = components[position];
                *index = (int)position;
                components.erase(components.begin() + position);
                changed({DocumentChange::Kind::Structure, id});
                return true;
            }
        }
        return false;
    }

    std::optional<Json> SceneDocument::componentField(const std::string &id, const std::string &type, const std::string &field) const
    {
        const Json *found = object(id);
        if (!found)
        {
            return std::nullopt;
        }
        for (const Json &component : (*found)["components"])
        {
            if (component.value("type", "") == type)
            {
                return component.contains(field) ? std::optional<Json>(component[field]) : std::nullopt;
            }
        }
        return std::nullopt;
    }

    bool SceneDocument::setComponentField(const std::string &id, const std::string &type, const std::string &field,
                                          const std::optional<Json> &value)
    {
        Json *component = findComponent(id, type);
        if (!component || field == "type")
        {
            return false;
        }
        assign(*component, field, value);
        changed({DocumentChange::Kind::Field, id, type, field, value ? *value : Json()});
        return true;
    }

    std::optional<Json> SceneDocument::overrideField(const std::string &platform, const std::string &id, const std::string &type,
                                                     const std::string &field) const
    {
        auto platforms = m_data.find("platforms");
        if (platforms == m_data.end() || !platforms->contains(platform) || !(*platforms)[platform].contains(id)
            || !(*platforms)[platform][id].contains(type) || !(*platforms)[platform][id][type].contains(field))
        {
            return std::nullopt;
        }
        return (*platforms)[platform][id][type][field];
    }

    bool SceneDocument::setOverrideField(const std::string &platform, const std::string &id, const std::string &type, const std::string &field,
                                         const std::optional<Json> &value)
    {
        if (!findComponent(id, type))
        {
            return false;
        }
        if (value)
        {
            m_data["platforms"][platform][id][type][field] = *value;
        }
        else if (overrideField(platform, id, type, field))
        {
            // An override that is cleared takes with it every level that is left empty, so the file stays as small as it was.
            Json &fields = m_data["platforms"][platform][id][type];
            fields.erase(field);
            if (fields.empty())
            {
                m_data["platforms"][platform][id].erase(type);
                if (m_data["platforms"][platform][id].empty())
                {
                    m_data["platforms"][platform].erase(id);
                    if (m_data["platforms"][platform].empty())
                    {
                        m_data["platforms"].erase(platform);
                        if (m_data["platforms"].empty())
                        {
                            m_data.erase("platforms");
                        }
                    }
                }
            }
        }
        changed({DocumentChange::Kind::Structure, id, type, field});
        return true;
    }

    void SceneDocument::changed(const DocumentChange &change)
    {
        m_isModified = true;
        if (m_onChange)
        {
            m_onChange(change);
        }
    }
}
