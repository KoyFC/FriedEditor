#pragma once

#include "fried/editor/asset_browser.h"
#include "fried/editor/command_stack.h"
#include "fried/editor/gizmo.h"
#include "fried/editor/platform.h"
#include "fried/editor/player_builder.h"
#include "fried/editor/player_session.h"
#include "fried/editor/project.h"
#include "fried/editor/scene_document.h"
#include "fried/editor/viewport.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fried::editor
{
    struct HierarchyRow
    {
        std::string m_id;
        std::string m_name;
        bool m_isCamera = false;
    };

    struct FieldView
    {
        FieldSchema m_schema;

        // What the file holds, or the engine's default where it holds nothing.
        nlohmann::json m_value;
        bool m_isSet = false;

        // On a platform other than the PC, the file overrides this field for that platform, and m_value is the override.
        bool m_isOverridden = false;
    };

    struct ComponentView
    {
        std::string m_type;
        std::vector<FieldView> m_fields;
        bool m_isRemovable = true;
    };

    // Everything the editor does that is not drawing: a backend shows these models, forwards its input and calls the methods.
    class Editor
    {
    public:
        enum class Status
        {
            Idle,
            BuildingPlayer,
            StartingPlayer,
            Ready,
            Failed
        };

        enum class Pointer
        {
            Primary,
            Pan
        };

        static constexpr int s_frameWidth = 1280;
        static constexpr int s_frameHeight = 720;

        // The engine checkout a project builds against: its engine/ folder, or the folder that holds it (the engine's own sandbox). Empty when neither has an editor player.
        static std::string findEngine(const std::string &projectDirectory);

        // Reads the project and its first scene, builds the player if it is not built, and starts it.
        bool open(const std::string &projectDirectory, const std::string &enginePath, std::string *error);

        // Called often by the backend, from its own thread.
        void pump();

        Status status() const { return m_status; }
        const std::string &message() const { return m_message; }

        const Project &project() const { return m_project; }
        const std::string &enginePath() const { return m_enginePath; }
        SceneDocument &document() { return m_document; }
        const SceneDocument &document() const { return m_document; }
        CommandStack &commands() { return *m_commands; }
        const std::string &scenePath() const { return m_scenePath; }

        bool openScene(const std::string &path, std::string *error);
        bool save(std::string *error);
        std::vector<std::string> scenePaths() const;

        void rebuildPlayer();

        Platform platform() const { return m_platform; }
        void setPlatform(Platform platform);

        const std::optional<std::string> &selected() const { return m_selected; }
        void select(const std::optional<std::string> &id);

        std::vector<HierarchyRow> hierarchy() const;
        // An empty type adds an object with only its Transform.
        void addObject(const std::string &componentType);
        void removeSelected();
        void moveSelected(int delta);
        void setCameraToSelected();
        void renameSelected(const std::string &name);

        std::vector<ComponentView> inspector() const;
        std::vector<std::string> addableComponents() const;
        std::vector<std::string> componentTypes() const;
        std::optional<nlohmann::json> selectedObjectField(const std::string &field) const;
        void setObjectField(const std::string &field, const std::optional<nlohmann::json> &value);
        void setField(const std::string &type, const std::string &field, const std::optional<nlohmann::json> &value);
        // On a platform other than the PC, a field is edited as that platform's override, and this takes the override away again.
        void clearOverride(const std::string &type, const std::string &field);
        void addComponent(const std::string &type);
        void removeComponent(const std::string &type);

        // The files of both asset roots, and where one is on disk.
        std::vector<AssetEntry> assets() const;
        std::string assetFile(const std::string &root, const std::string &path) const;

        // An image dropped on an object with a Sprite becomes its texture, and anywhere else a new Sprite. A font becomes a new Text.
        void dropAsset(const std::string &root, const std::string &path, double frameX, double frameY);

        GizmoMode gizmoMode() const { return m_gizmoMode; }
        void setGizmoMode(GizmoMode mode);

        const Viewport &viewport() const { return m_viewport; }
        const Frame &frame() const { return m_frame; }

        // Counts the frames received, so a backend uploads a picture only when there is a new one.
        unsigned long frameSerial() const { return m_frameSerial; }
        DrawList overlay() const;

        void pointerDown(double frameX, double frameY, Pointer pointer);
        void pointerMove(double frameX, double frameY);
        void pointerUp();
        void wheel(double frameX, double frameY, double steps);

        // Something the backend shows has changed: a frame, the selection, the document or the status.
        std::function<void()> m_onChanged;

    private:
        void startPlayer();
        void documentChanged(const DocumentChange &change);
        void cameraChanged();
        void changed();
        TransformValues transformOf(const std::string &id) const;
        std::vector<GizmoHandle> currentHandles() const;
        void execute(std::unique_ptr<Command> command, bool merge);
        void setTransformFields(const std::string &id, const std::map<std::string, double> &fields, bool merge);
        nlohmann::json newComponent(const std::string &type) const;
        const ComponentSchema *schemaOf(const std::string &type) const;
        std::string playerLogFile() const;

        Project m_project;
        std::string m_enginePath;
        std::string m_scenePath;
        SceneDocument m_document;
        std::unique_ptr<CommandStack> m_commands;
        PlayerSession m_session;
        std::unique_ptr<PlayerBuilder> m_builder;
        Status m_status = Status::Idle;
        std::string m_message;
        Platform m_platform = Platform::Pc;
        std::optional<std::string> m_selected;
        Viewport m_viewport;
        Frame m_frame;
        unsigned long m_frameSerial = 0;
        GizmoMode m_gizmoMode = GizmoMode::Move;

        bool m_needsLoad = false;
        bool m_needsCamera = false;
        bool m_needsRender = false;
        bool m_isRenderInFlight = false;

        struct Drag
        {
            enum class Kind
            {
                None,
                Object,
                Handle,
                Pan
            };
            Kind m_kind = Kind::None;
            std::string m_id;
            double m_startWorldX = 0;
            double m_startWorldY = 0;
            double m_startX = 0;
            double m_startY = 0;
            double m_lastFrameX = 0;
            double m_lastFrameY = 0;
            bool m_hasMoved = false;
            std::optional<GizmoDrag> m_handle;
        } m_drag;
    };
}
