#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace fried::editor
{
    // What a change to the document means for whoever is showing it.
    struct DocumentChange
    {
        enum class Kind
        {
            // Objects or components were added, removed or reordered, so the whole scene has to be shown again.
            Structure,

            // One value changed, which can be applied in place.
            Field
        };

        Kind m_kind = Kind::Structure;
        std::string m_objectId;

        // Empty for a field of the object itself.
        std::string m_component;
        std::string m_field;
        nlohmann::json m_value;
    };

    // A scene file in memory, the only copy of it the editor edits. Every edit is a method here, which is what undo is written against, and each one reports itself.
    class SceneDocument
    {
    public:
        static constexpr int s_version = 1;

        static SceneDocument empty(const std::string &name);

        bool parse(const std::string &text, std::string *error);
        bool load(const std::string &path, std::string *error);
        bool save(const std::string &path, std::string *error) const;

        // The file's text: tabs, sorted keys and a final newline, so the same scene is always the same bytes.
        std::string dump() const;
        const nlohmann::json &data() const { return m_data; }

        const std::string &name() const { return m_name; }

        std::vector<std::string> objectIds() const;
        const nlohmann::json *object(const std::string &id) const;
        int indexOf(const std::string &id) const;
        std::string uniqueId(const std::string &base) const;

        std::optional<std::string> cameraId() const;
        void setCameraId(const std::optional<std::string> &id);

        bool insertObject(int index, const nlohmann::json &object);
        bool removeObject(const std::string &id, nlohmann::json *removed, int *index);
        bool moveObject(const std::string &id, int index);

        // A field of the object itself: name, active or priority.
        std::optional<nlohmann::json> objectField(const std::string &id, const std::string &field) const;
        bool setObjectField(const std::string &id, const std::string &field, const std::optional<nlohmann::json> &value);

        bool insertComponent(const std::string &id, int index, const nlohmann::json &component);
        bool removeComponent(const std::string &id, const std::string &type, nlohmann::json *removed, int *index);

        // An absent value is a field the file does not mention, which the engine fills with its own default.
        std::optional<nlohmann::json> componentField(const std::string &id, const std::string &type, const std::string &field) const;
        bool setComponentField(const std::string &id, const std::string &type, const std::string &field, const std::optional<nlohmann::json> &value);

        bool isModified() const { return m_isModified; }
        void markSaved() { m_isModified = false; }

        std::function<void(const DocumentChange &)> m_onChange;

    private:
        nlohmann::json *findObject(const std::string &id);
        nlohmann::json *findComponent(const std::string &id, const std::string &type);
        void changed(const DocumentChange &change);

        nlohmann::json m_data;
        std::string m_name;
        bool m_isModified = false;
    };
}
