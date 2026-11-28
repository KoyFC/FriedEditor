#include "fried/editor/commands.h"

namespace fried::editor
{
    SetField::SetField(std::string id, ComponentRef component, std::string field, std::optional<nlohmann::json> value)
        : m_id(std::move(id)), m_component(std::move(component)), m_field(std::move(field)), m_value(std::move(value))
    {
    }

    bool SetField::apply(SceneDocument &document)
    {
        m_before = document.componentField(m_id, m_component, m_field);
        return document.setComponentField(m_id, m_component, m_field, m_value);
    }

    void SetField::revert(SceneDocument &document)
    {
        document.setComponentField(m_id, m_component, m_field, m_before);
    }

    bool SetField::mergeWith(const Command &later)
    {
        auto *next = dynamic_cast<const SetField *>(&later);
        if (!next || next->m_id != m_id || !(next->m_component == m_component) || next->m_field != m_field)
        {
            return false;
        }
        m_value = next->m_value;
        return true;
    }

    ClearFields::ClearFields(std::string id, ComponentRef component, std::vector<std::string> fields)
        : m_id(std::move(id)), m_component(std::move(component)), m_fields(std::move(fields))
    {
    }

    bool ClearFields::apply(SceneDocument &document)
    {
        m_before.clear();
        for (const std::string &field : m_fields)
        {
            m_before.push_back(document.componentField(m_id, m_component, field));
            if (!document.setComponentField(m_id, m_component, field, std::nullopt))
            {
                return false;
            }
        }
        return true;
    }

    void ClearFields::revert(SceneDocument &document)
    {
        for (std::size_t index = 0; index < m_fields.size() && index < m_before.size(); ++index)
        {
            document.setComponentField(m_id, m_component, m_fields[index], m_before[index]);
        }
    }

    SetFields::SetFields(std::string id, ComponentRef component, std::map<std::string, double> fields)
        : m_id(std::move(id)), m_component(std::move(component)), m_fields(std::move(fields))
    {
    }

    bool SetFields::apply(SceneDocument &document)
    {
        m_before.clear();
        for (const auto &[field, value] : m_fields)
        {
            m_before[field] = document.componentField(m_id, m_component, field);
            if (!document.setComponentField(m_id, m_component, field, nlohmann::json(value)))
            {
                return false;
            }
        }
        return true;
    }

    void SetFields::revert(SceneDocument &document)
    {
        for (const auto &[field, before] : m_before)
        {
            document.setComponentField(m_id, m_component, field, before);
        }
    }

    bool SetFields::mergeWith(const Command &later)
    {
        auto *next = dynamic_cast<const SetFields *>(&later);
        if (!next || next->m_id != m_id || !(next->m_component == m_component))
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

    AddObject::AddObject(std::string parentId, int index, nlohmann::json node, std::string label)
        : m_parentId(std::move(parentId)), m_index(index), m_node(std::move(node)), m_label(std::move(label))
    {
    }

    bool AddObject::apply(SceneDocument &document)
    {
        return document.insertNode(m_parentId, m_index, m_node);
    }

    void AddObject::revert(SceneDocument &document)
    {
        nlohmann::json removed;
        std::string parentId;
        int index = 0;
        document.removeNode(m_node.value("id", ""), &removed, &parentId, &index);
    }

    RemoveObject::RemoveObject(std::string id)
        : m_id(std::move(id))
    {
    }

    bool RemoveObject::apply(SceneDocument &document)
    {
        std::optional<std::string> camera = document.cameraId();
        bool inCamera = false;
        if (camera)
        {
            inCamera = *camera == m_id || document.isInside(*camera, m_id);
        }
        if (!document.removeNode(m_id, &m_node, &m_parentId, &m_index))
        {
            return false;
        }
        m_wasCamera = inCamera;
        m_camera = inCamera ? camera : std::nullopt;
        return true;
    }

    void RemoveObject::revert(SceneDocument &document)
    {
        document.insertNode(m_parentId, m_index, m_node);
        if (m_wasCamera)
        {
            document.setCameraId(m_camera);
        }
    }

    MoveObject::MoveObject(std::string id, std::string parentId, int index)
        : m_id(std::move(id)), m_parentId(std::move(parentId)), m_index(index)
    {
    }

    bool MoveObject::apply(SceneDocument &document)
    {
        m_beforeParentId = document.parentOf(m_id);
        m_beforeIndex = document.indexOf(m_id);
        if (m_beforeParentId == m_parentId && m_beforeIndex == m_index)
        {
            return false;
        }
        return document.moveNode(m_id, m_parentId, m_index);
    }

    void MoveObject::revert(SceneDocument &document)
    {
        document.moveNode(m_id, m_beforeParentId, m_beforeIndex);
    }

    ChangeTransformType::ChangeTransformType(std::string id, std::string type, std::set<std::string> keptFields)
        : m_id(std::move(id)), m_type(std::move(type)), m_keptFields(std::move(keptFields))
    {
    }

    bool ChangeTransformType::apply(SceneDocument &document)
    {
        const nlohmann::json *current = document.node(m_id);
        if (!current)
        {
            return false;
        }
        std::string currentType = document.transformTypeOf(m_id);
        if (currentType == (m_type.empty() ? "Transform" : m_type))
        {
            return false;
        }
        nlohmann::json next = nullptr;
        if (!m_type.empty() && m_type != "Transform")
        {
            next = {{"type", m_type}};
            if (current->contains("transform"))
            {
                for (auto field = (*current)["transform"].begin(); field != (*current)["transform"].end(); ++field)
                {
                    if (field.key() != "type" && m_keptFields.count(field.key()))
                    {
                        next[field.key()] = field.value();
                    }
                }
            }
        }
        return document.replaceTransform(m_id, next, &m_before);
    }

    void ChangeTransformType::revert(SceneDocument &document)
    {
        nlohmann::json ignored;
        document.replaceTransform(m_id, m_before, &ignored);
    }

    AddComponent::AddComponent(std::string id, nlohmann::json component, int position)
        : m_id(std::move(id)), m_component(std::move(component)), m_position(position)
    {
    }

    bool AddComponent::apply(SceneDocument &document)
    {
        const nlohmann::json *object = document.node(m_id);
        if (!object)
        {
            return false;
        }
        int size = object->contains("components") ? (int)(*object)["components"].size() : 0;
        if (m_position < 0 || m_position > size)
        {
            m_position = size;
        }
        if (!document.insertComponent(m_id, m_position, m_component))
        {
            return false;
        }
        m_ref.m_type = m_component.value("type", "");
        m_ref.m_index = 0;
        for (int index = 0; index < m_position; ++index)
        {
            if ((*document.node(m_id))["components"][index].value("type", "") == m_ref.m_type)
            {
                m_ref.m_index++;
            }
        }
        return true;
    }

    void AddComponent::revert(SceneDocument &document)
    {
        nlohmann::json removed;
        int position = 0;
        document.removeComponent(m_id, m_ref, &removed, &position);
    }

    RemoveComponent::RemoveComponent(std::string id, ComponentRef component)
        : m_id(std::move(id)), m_component(std::move(component))
    {
    }

    bool RemoveComponent::apply(SceneDocument &document)
    {
        return document.removeComponent(m_id, m_component, &m_removed, &m_position);
    }

    void RemoveComponent::revert(SceneDocument &document)
    {
        document.insertComponent(m_id, m_position, m_removed);
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
