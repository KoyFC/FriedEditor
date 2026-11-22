#include "fried/editor/commands.h"

namespace fried::editor
{
    SetNodeField::SetNodeField(std::string id, std::string field, std::optional<nlohmann::json> value)
        : m_id(std::move(id)), m_field(std::move(field)), m_value(std::move(value))
    {
    }

    bool SetNodeField::apply(SceneDocument &document)
    {
        m_before = document.nodeField(m_id, m_field);
        return document.setNodeField(m_id, m_field, m_value);
    }

    void SetNodeField::revert(SceneDocument &document)
    {
        document.setNodeField(m_id, m_field, m_before);
    }

    bool SetNodeField::mergeWith(const Command &later)
    {
        auto *next = dynamic_cast<const SetNodeField *>(&later);
        if (!next || next->m_id != m_id || next->m_field != m_field)
        {
            return false;
        }
        m_value = next->m_value;
        return true;
    }

    ClearNodeFields::ClearNodeFields(std::string id, std::vector<std::string> fields)
        : m_id(std::move(id)), m_fields(std::move(fields))
    {
    }

    bool ClearNodeFields::apply(SceneDocument &document)
    {
        m_before.clear();
        for (const std::string &field : m_fields)
        {
            m_before.push_back(document.nodeField(m_id, field));
            if (!document.setNodeField(m_id, field, std::nullopt))
            {
                return false;
            }
        }
        return true;
    }

    void ClearNodeFields::revert(SceneDocument &document)
    {
        for (std::size_t index = 0; index < m_fields.size() && index < m_before.size(); ++index)
        {
            document.setNodeField(m_id, m_fields[index], m_before[index]);
        }
    }

    SetNodeFields::SetNodeFields(std::string id, std::map<std::string, double> fields)
        : m_id(std::move(id)), m_fields(std::move(fields))
    {
    }

    bool SetNodeFields::apply(SceneDocument &document)
    {
        m_before.clear();
        for (const auto &[field, value] : m_fields)
        {
            m_before[field] = document.nodeField(m_id, field);
            if (!document.setNodeField(m_id, field, nlohmann::json(value)))
            {
                return false;
            }
        }
        return true;
    }

    void SetNodeFields::revert(SceneDocument &document)
    {
        for (const auto &[field, before] : m_before)
        {
            document.setNodeField(m_id, field, before);
        }
    }

    bool SetNodeFields::mergeWith(const Command &later)
    {
        auto *next = dynamic_cast<const SetNodeFields *>(&later);
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

    AddNode::AddNode(std::string parentId, int index, nlohmann::json node, std::string label)
        : m_parentId(std::move(parentId)), m_index(index), m_node(std::move(node)), m_label(std::move(label))
    {
    }

    bool AddNode::apply(SceneDocument &document)
    {
        return document.insertNode(m_parentId, m_index, m_node);
    }

    void AddNode::revert(SceneDocument &document)
    {
        nlohmann::json removed;
        std::string parentId;
        int index = 0;
        document.removeNode(m_node.value("id", ""), &removed, &parentId, &index);
    }

    RemoveNode::RemoveNode(std::string id)
        : m_id(std::move(id))
    {
    }

    bool RemoveNode::apply(SceneDocument &document)
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

    void RemoveNode::revert(SceneDocument &document)
    {
        document.insertNode(m_parentId, m_index, m_node);
        if (m_wasCamera)
        {
            document.setCameraId(m_camera);
        }
    }

    MoveNode::MoveNode(std::string id, std::string parentId, int index)
        : m_id(std::move(id)), m_parentId(std::move(parentId)), m_index(index)
    {
    }

    bool MoveNode::apply(SceneDocument &document)
    {
        m_beforeParentId = document.parentOf(m_id);
        m_beforeIndex = document.indexOf(m_id);
        if (m_beforeParentId == m_parentId && m_beforeIndex == m_index)
        {
            return false;
        }
        return document.moveNode(m_id, m_parentId, m_index);
    }

    void MoveNode::revert(SceneDocument &document)
    {
        document.moveNode(m_id, m_beforeParentId, m_beforeIndex);
    }

    ChangeNodeType::ChangeNodeType(std::string id, std::string type, std::set<std::string> keptFields)
        : m_id(std::move(id)), m_type(std::move(type)), m_keptFields(std::move(keptFields))
    {
    }

    bool ChangeNodeType::apply(SceneDocument &document)
    {
        const nlohmann::json *current = document.node(m_id);
        if (!current || current->value("type", "") == m_type)
        {
            return false;
        }
        nlohmann::json next = {{"id", m_id}, {"type", m_type}};
        for (auto field = current->begin(); field != current->end(); ++field)
        {
            if (field.key() != "id" && field.key() != "type" && field.key() != "children" && (field.key() == "name" || m_keptFields.count(field.key())))
            {
                next[field.key()] = field.value();
            }
        }
        return document.replaceNode(m_id, next, &m_before);
    }

    void ChangeNodeType::revert(SceneDocument &document)
    {
        nlohmann::json ignored;
        document.replaceNode(m_id, m_before, &ignored);
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
