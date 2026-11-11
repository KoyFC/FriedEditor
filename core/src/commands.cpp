#include "fried/editor/commands.h"

namespace fried::editor
{
    SetComponentField::SetComponentField(std::string id, std::string type, std::string field, std::optional<nlohmann::json> value)
        : m_id(std::move(id)), m_type(std::move(type)), m_field(std::move(field)), m_value(std::move(value))
    {
    }

    bool SetComponentField::apply(SceneDocument &document)
    {
        m_before = document.componentField(m_id, m_type, m_field);
        return document.setComponentField(m_id, m_type, m_field, m_value);
    }

    void SetComponentField::revert(SceneDocument &document)
    {
        document.setComponentField(m_id, m_type, m_field, m_before);
    }

    bool SetComponentField::mergeWith(const Command &later)
    {
        auto *next = dynamic_cast<const SetComponentField *>(&later);
        if (!next || next->m_id != m_id || next->m_type != m_type || next->m_field != m_field)
        {
            return false;
        }
        m_value = next->m_value;
        return true;
    }

    SetObjectField::SetObjectField(std::string id, std::string field, std::optional<nlohmann::json> value)
        : m_id(std::move(id)), m_field(std::move(field)), m_value(std::move(value))
    {
    }

    bool SetObjectField::apply(SceneDocument &document)
    {
        m_before = document.objectField(m_id, m_field);
        return document.setObjectField(m_id, m_field, m_value);
    }

    void SetObjectField::revert(SceneDocument &document)
    {
        document.setObjectField(m_id, m_field, m_before);
    }

    bool SetObjectField::mergeWith(const Command &later)
    {
        auto *next = dynamic_cast<const SetObjectField *>(&later);
        if (!next || next->m_id != m_id || next->m_field != m_field)
        {
            return false;
        }
        m_value = next->m_value;
        return true;
    }

    SetPosition::SetPosition(std::string id, double x, double y)
        : m_id(std::move(id)), m_x(x), m_y(y)
    {
    }

    bool SetPosition::apply(SceneDocument &document)
    {
        m_beforeX = document.componentField(m_id, "Transform", "x");
        m_beforeY = document.componentField(m_id, "Transform", "y");
        return document.setComponentField(m_id, "Transform", "x", nlohmann::json(m_x))
            && document.setComponentField(m_id, "Transform", "y", nlohmann::json(m_y));
    }

    void SetPosition::revert(SceneDocument &document)
    {
        document.setComponentField(m_id, "Transform", "x", m_beforeX);
        document.setComponentField(m_id, "Transform", "y", m_beforeY);
    }

    bool SetPosition::mergeWith(const Command &later)
    {
        auto *next = dynamic_cast<const SetPosition *>(&later);
        if (!next || next->m_id != m_id)
        {
            return false;
        }
        m_x = next->m_x;
        m_y = next->m_y;
        return true;
    }

    SetTransformFields::SetTransformFields(std::string id, std::map<std::string, double> fields)
        : m_id(std::move(id)), m_fields(std::move(fields))
    {
    }

    bool SetTransformFields::apply(SceneDocument &document)
    {
        m_before.clear();
        for (const auto &[field, value] : m_fields)
        {
            m_before[field] = document.componentField(m_id, "Transform", field);
            if (!document.setComponentField(m_id, "Transform", field, nlohmann::json(value)))
            {
                return false;
            }
        }
        return true;
    }

    void SetTransformFields::revert(SceneDocument &document)
    {
        for (const auto &[field, before] : m_before)
        {
            document.setComponentField(m_id, "Transform", field, before);
        }
    }

    bool SetTransformFields::mergeWith(const Command &later)
    {
        auto *next = dynamic_cast<const SetTransformFields *>(&later);
        if (!next || next->m_id != m_id)
        {
            return false;
        }
        for (const auto &[field, value] : next->m_fields)
        {
            if (m_fields.find(field) == m_fields.end())
            {
                return false;
            }
        }
        for (const auto &[field, value] : next->m_fields)
        {
            m_fields[field] = value;
        }
        return true;
    }

    AddObject::AddObject(int index, nlohmann::json object)
        : m_index(index), m_object(std::move(object))
    {
    }

    bool AddObject::apply(SceneDocument &document)
    {
        return document.insertObject(m_index, m_object);
    }

    void AddObject::revert(SceneDocument &document)
    {
        nlohmann::json removed;
        int index = 0;
        document.removeObject(m_object.value("id", ""), &removed, &index);
    }

    RemoveObject::RemoveObject(std::string id)
        : m_id(std::move(id))
    {
    }

    bool RemoveObject::apply(SceneDocument &document)
    {
        m_wasCamera = document.cameraId() == m_id;
        if (!document.removeObject(m_id, &m_object, &m_index))
        {
            return false;
        }
        return true;
    }

    void RemoveObject::revert(SceneDocument &document)
    {
        document.insertObject(m_index, m_object);
        if (m_wasCamera)
        {
            document.setCameraId(m_id);
        }
    }

    MoveObject::MoveObject(std::string id, int index)
        : m_id(std::move(id)), m_index(index)
    {
    }

    bool MoveObject::apply(SceneDocument &document)
    {
        m_before = document.indexOf(m_id);
        return m_before != m_index && document.moveObject(m_id, m_index);
    }

    void MoveObject::revert(SceneDocument &document)
    {
        document.moveObject(m_id, m_before);
    }

    AddComponent::AddComponent(std::string id, int index, nlohmann::json component)
        : m_id(std::move(id)), m_index(index), m_component(std::move(component))
    {
    }

    bool AddComponent::apply(SceneDocument &document)
    {
        return document.insertComponent(m_id, m_index, m_component);
    }

    void AddComponent::revert(SceneDocument &document)
    {
        nlohmann::json removed;
        int index = 0;
        document.removeComponent(m_id, m_component.value("type", ""), &removed, &index);
    }

    RemoveComponent::RemoveComponent(std::string id, std::string type)
        : m_id(std::move(id)), m_type(std::move(type))
    {
    }

    bool RemoveComponent::apply(SceneDocument &document)
    {
        if (!document.removeComponent(m_id, m_type, &m_component, &m_index))
        {
            return false;
        }
        return true;
    }

    void RemoveComponent::revert(SceneDocument &document)
    {
        document.insertComponent(m_id, m_index, m_component);
    }

    SetCamera::SetCamera(std::optional<std::string> id)
        : m_id(std::move(id))
    {
    }

    bool SetCamera::apply(SceneDocument &document)
    {
        m_before = document.cameraId();
        if (m_before == m_id)
        {
            return false;
        }
        document.setCameraId(m_id);
        return true;
    }

    void SetCamera::revert(SceneDocument &document)
    {
        document.setCameraId(m_before);
    }
}
