#pragma once

#include "fried/editor/asset_browser.h"
#include "fried/editor/asset_validation.h"
#include "fried/editor/command_stack.h"
#include "fried/editor/editor_state.h"
#include "fried/editor/gizmo.h"
#include "fried/editor/game_runner.h"
#include "fried/editor/platform.h"
#include "fried/editor/player_builder.h"
#include "fried/editor/player_session.h"
#include "fried/editor/project.h"
#include "fried/editor/scene_document.h"
#include "fried/editor/viewport.h"

#include <chrono>
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
    };

    // What the backend shows while something is being built, or after it failed, until it is dismissed.
    struct BuildProgress
    {
        bool m_isVisible = false;
        bool m_isFailed = false;
        bool m_canStop = false;
        std::string m_title;
        std::string m_stepLabel;
        int m_step = 0;
        int m_steps = 0;
        std::string m_message;

        // The end of the build's log, which keeps growing while it runs.
        std::string m_log;
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
        void setMessage(const std::string &message) { m_message = message; }

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

        BuildProgress buildProgress() const;
        // Hides a failed build's window. A build that is going on cannot be dismissed, only stopped.
        void dismissBuild();

        // Saves the scene, builds the project's game for the PC and runs it. Playing again while it runs does nothing.
        void play();
        void stopGame();
        GameRunner::State gameState() const { return m_game.state(); }

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
        void addComponent(const std::string &type);
        void removeComponent(const std::string &type);

        // The files of both asset roots, and where one is on disk.
        std::vector<AssetEntry> assets() const;
        std::string assetFile(const std::string &root, const std::string &path) const;

        // An image dropped on an object with a Sprite becomes its texture, and anywhere else a new Sprite. A font becomes a new Text.
        void dropAsset(const std::string &root, const std::string &path, double frameX, double frameY);

        // What is wrong with the project's assets for the platform being previewed, and putting one right.
        std::vector<AssetIssue> issues() const;
        bool fixIssue(const AssetIssue &issue, std::string *error);

        GizmoMode gizmoMode() const { return m_gizmoMode; }
        void setGizmoMode(GizmoMode mode);

        // The size in pixels of the area the backend shows the frame in. The player draws frames of that size, so nothing is scaled and no pixel is drawn that is not seen. A change is sent once it has held still for a moment, so dragging a panel's edge does not resize the player on every step.
        void setViewSize(int width, int height);

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
        DrawList anchorOverlay(int viewWidth, int viewHeight) const;

        // What a drag asks of an object's position, as the fields to write. A RectTransform's position is a result of its layout, so moving it changes its offsets, or its insets where it stretches.
        std::map<std::string, double> dragFields(const std::string &id, std::map<std::string, double> fields);
        std::vector<GizmoHandle> currentHandles() const;
        std::vector<GizmoHandle> handlesFor(const Viewport &view) const;
        double gameCameraZoom() const;
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
        GameRunner m_game;
        bool m_isFailureShown = false;
        std::string m_failedTitle;
        std::string m_failedLog;
        std::string m_failedMessage;
        Status m_status = Status::Idle;
        std::string m_message;
        Platform m_platform = Platform::Pc;
        std::optional<std::string> m_selected;
        Viewport m_viewport;

        // The view the player drew the frame on screen with. The overlay is drawn with it, not with the view the user has moved on to since, so the two stay together while panning.
        Viewport m_frameView;
        Viewport m_sentView;
        Viewport m_inFlightView;
        Frame m_frame;
        unsigned long m_frameSerial = 0;
        GizmoMode m_gizmoMode = GizmoMode::Move;

        int m_wantedWidth = 0;
        int m_wantedHeight = 0;
        int m_requestedWidth = 0;
        int m_requestedHeight = 0;
        std::chrono::steady_clock::time_point m_wantedSince;

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
            double m_appliedX = 0;
            double m_appliedY = 0;
            double m_lastFrameX = 0;
            double m_lastFrameY = 0;
            bool m_hasMoved = false;
            std::optional<GizmoDrag> m_handle;
        } m_drag;
    };
}
