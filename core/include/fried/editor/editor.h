#pragma once

#include "fried/editor/asset_browser.h"
#include "fried/editor/core_types.h"
#include "fried/editor/command_stack.h"
#include "fried/editor/editor_state.h"
#include "fried/editor/gizmo.h"
#include "fried/editor/game_runner.h"
#include "fried/editor/game_session.h"
#include "fried/editor/log_tail.h"
#include "fried/editor/core_types.h"
#include "fried/editor/player_builder.h"
#include "fried/editor/player_session.h"
#include "fried/editor/project_settings.h"
#include "fried/editor/scene_document.h"
#include "fried/editor/viewport.h"
#include "fried/editor/viewport3d.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fried::editor
{
    // One node of the scene's tree as the hierarchy lists it, parents before their children.
    struct HierarchyRow
    {
        std::string m_id;
        std::string m_name;
        std::string m_type;
        bool m_isCamera = false;

        // The scene this node instances, empty for a node of the scene's own.
        std::string m_instance;
        int m_depth = 0;
        bool m_hasChildren = false;
    };

    // Something wrong with a scene that the editor can say how to put right.
    struct SceneIssue
    {
        std::string m_node;
        std::string m_scene;
        std::string m_message;
    };

    // Where an anchor sits along one axis of the screen: at its start, middle or end, or spread across all of it.
    enum class AnchorAxis
    {
        Start,
        Center,
        End,
        Stretch,
        Custom
    };

    // What the backend draws of the selected Control: the rectangle its anchors refer to, which is its parent's, the box the control takes in it and where its anchors and pivot are.
    struct RectPreview
    {
        double m_originX = 0;
        double m_originY = 0;
        double m_viewWidth = 0;
        double m_viewHeight = 0;
        double m_x = 0;
        double m_y = 0;
        double m_width = 0;
        double m_height = 0;
        double m_anchorMinX = 0;
        double m_anchorMinY = 0;
        double m_anchorMaxX = 0;
        double m_anchorMaxY = 0;
        double m_pivotX = 0;
        double m_pivotY = 0;
        AnchorAxis m_horizontal = AnchorAxis::Start;
        AnchorAxis m_vertical = AnchorAxis::Start;
        bool m_isBelow = false;
    };

    struct FieldView
    {
        FieldSchema m_schema;

        // What the file holds, or the engine's default where it holds nothing.
        nlohmann::json m_value;
        bool m_isSet = false;
    };

    // What the backend shows while something is being built, or after it ended, until it is dismissed.
    struct BuildProgress
    {
        bool m_isVisible = false;
        bool m_isFailed = false;

        // A build of the project for a platform stays on screen when it ends well too, so its output can be read.
        bool m_isFinished = false;
        bool m_canStop = false;
        std::string m_title;
        std::string m_stepLabel;
        int m_step = 0;
        int m_steps = 0;
        std::string m_message;

        // The game the build made, for a build that ended well. Empty when there is nothing to show.
        std::string m_builtFile;

        // What the build has printed, which keeps growing while it runs. Null when there is only the message.
        const std::vector<fried::core::OutputLine> *m_lines = nullptr;
    };

    // What one class of the selected node's chain declares, with the node's values for it. The inspector shows one section for each, the node's own class first.
    struct ClassView
    {
        std::string m_class;
        std::vector<FieldView> m_fields;
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
            Pan,
            Look
        };

        // Whether the viewport shows the scene's plane or its 3D space. A scene that has a Node3D starts in 3D.
        enum class ViewMode
        {
            TwoD,
            ThreeD
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

        // Opens the scene in a tab of its own, or goes to its tab when it is open. The scenes already open keep their changes and their undo history.
        bool openScene(const std::string &path, std::string *error);

        // Writes a new scene file in the project's scenes, with one root node of a class, and opens it. The name is the file's.
        bool newScene(const std::string &name, const std::string &rootType, std::string *error);

        // The scenes open, in the order of their tabs. While the game is on a scene with no tab, none of them is the one shown and activeSceneIndex is -1.
        const std::vector<std::string> &openScenes() const { return m_tabs; }
        int activeSceneIndex() const;
        bool isSceneModified(const std::string &path) const;
        bool hasModifiedScenes() const;

        // Closes a tab and drops what was changed in it and not saved, which is for the caller to have asked about. The last tab cannot be closed, and none can while the game runs.
        bool closeScene(const std::string &path, std::string *error);

        bool save(std::string *error);
        bool saveScene(const std::string &path, std::string *error);
        bool saveAll(std::string *error);
        // Saves the scene being edited under a new name in the project's scenes and edits that file from then on, as Godot's Save Scene As does. The old file stays as it was last saved. False with the reason in error when the name is not a file name or is taken.
        bool saveSceneAs(const std::string &name, std::string *error);
        std::vector<std::string> scenePaths() const;

        // The scenes opened last, the latest first, that are still in the project.
        std::vector<std::string> recentScenes() const;

        void rebuildPlayer();

        BuildProgress buildProgress() const;
        // Hides a failed build's window. A build that is going on cannot be dismissed, only stopped.
        void dismissBuild();

        // Stops whichever build is going on, of the game or of the project for a platform.
        void stopBuild();

        // The project's own settings, which are saved as they are changed, and its builds for each platform.
        ProjectSettings &projectSettings() { return m_settings; }

        // The names the engine writes input actions with, once its player has said them.
        const InputNames &inputNames() const { return m_session.inputNames(); }
        ProjectBuild &projectBuild() { return m_projectBuild; }

        // Saves the scene and builds the project for a platform. False with the reason in error when it cannot, or while something else is being built.
        bool buildProject(fried::core::BuildTarget target, std::string *error);

        // Saves the scene, builds the project's game for the PC and runs it in a window of its own. Does nothing while it runs.
        void buildAndRun();
        void stopGame();
        GameRunner::State gameState() const { return m_game.state(); }

        // Runs the project's game inside the editor, from the scene being edited or from the project's start scene. The project's player is built first if its code changed. What is edited while it runs is taken back when it stops, and until then the scene cannot be saved or changed for another.
        void startPlay(bool isFromStartScene);
        void stopPlay();
        bool isPlaying() const { return m_isPlaying; }

        // While the game runs: pausing it, letting one frame through while it is paused, and how fast its time goes compared to real time, which the editor remembers from one run to the next.
        bool isPaused() const { return m_isPlaying && m_play.isPaused(); }
        void setPaused(bool isPaused);
        void stepFrame();
        double playSpeed() const { return m_playSpeed; }
        void setPlaySpeed(double scale);
        // True from pressing Play until the game is on screen, which includes building the player.
        bool isStartingPlay() const { return m_pendingPlay.has_value() || (m_isPlaying && m_play.state() == GameSession::State::Waiting); }
        GameSession &gameSession() { return m_play; }
        // Whether the scene view is on screen, which is when the running game reports every node and not only the selected one.
        void setSceneWatched(bool isWatched) { m_isSceneWatched = isWatched; }
        // The last frame the game sent, and a number that changes with each.
        const GameFrame &gameFrame() const { return m_gameFrame; }
        unsigned long long gameFrameSerial() const { return m_gameFrameSerial; }

        Platform platform() const { return m_platform; }
        void setPlatform(Platform platform);

        const std::optional<std::string> &selected() const { return m_selected; }
        void select(const std::optional<std::string> &id);

        // Worked out again only after the document changed.
        const std::vector<HierarchyRow> &hierarchy() const;

        // Adds a node of a class as the last child of a node, or at the top of the scene with no parent, and selects it. A node that is placed by a position starts at the center of the view.
        void addNode(const std::string &type, const std::optional<std::string> &parentId = std::nullopt);
        void removeSelected();

        // Adds a scene of the project as one node under a node, or at the top of the scene with no parent, and selects it. False with the reason in error for a scene that cannot be instanced here.
        bool addInstance(const std::string &scenePath, const std::optional<std::string> &parentId, std::string *error);

        // The scene an instance in the open scene is of, and the file it is in.
        std::string instanceOf(const std::string &id) const { return m_document.instanceOf(id); }
        std::string scenePathOf(const std::string &name) const;

        // Why the scene cannot be instanced in the scene being edited: it is not there, it has more than one top-level node, or it would hold itself. Empty when it can.
        std::string instanceProblem(const std::string &sceneName) const;

        // The instances of the open scene that cannot be built, each fixed by removing it or by choosing another scene for it.
        std::vector<SceneIssue> sceneIssues() const;

        // What is wrong with the project's input actions, one sentence each. Empty until the player has told the names of the keys, since they cannot be checked before.
        std::vector<std::string> inputIssues() const;
        void chooseInstanceScene(const std::string &id, const std::string &sceneName);

        // Puts a copy of the selected node and what is under it next to it, with new ids and a name that is not its siblings'.
        bool duplicateSelected();

        // The selected node and what is under it as text for the system clipboard, which is empty when nothing is selected. The editor keeps the copy too.
        std::string copySelected();

        // Pastes what the text holds, or the last copy when it holds none, next to the selected node, or into the scene when none is selected.
        bool paste(const std::string &clipboardText);

        // Moves the selected node among its siblings.
        void moveSelected(int delta);

        // Moves a node under another, or to the top of the scene with an empty parent. The index is its place among the new siblings as they are now.
        void reparent(const std::string &id, const std::string &parentId, int index);
        void changeType(const std::string &id, const std::string &type);
        void setCameraToSelected();
        void rename(const std::string &id, const std::string &name);
        void renameSelected(const std::string &name);

        // The selected node's class sections, the node's own class first. Worked out again only after the document or the selection changed.
        const std::vector<ClassView> &inspector() const;

        // Every class the player knows, each after the class it extends, which is what a menu of node types is made from.
        const std::vector<ClassSchema> &classes() const { return m_session.schema(); }
        bool isClassOf(const std::string &type, const std::string &ancestor) const;

        // The class and the ones it extends, its own name first, which is how a class without a look of its own takes its parent's.
        std::vector<std::string> classChain(const std::string &type) const;
        // Takes the fields out of the selected node's data, in one step to undo.
        void revertFields(const std::vector<std::string> &fields);
        void setField(const std::string &field, const std::optional<nlohmann::json> &value);

        // The selected Control's anchors and box for the backend to draw, when it is one and the player has shown it.
        std::optional<RectPreview> rectPreview() const;

        // Anchors the control to a place in its parent, as Unity's presets do, and moves the offsets so that it stays where it is. Custom leaves that axis as it is.
        void setAnchorPreset(AnchorAxis horizontal, AnchorAxis vertical);

        // Moves the pivot, and the offsets with it so that the control stays where it is.
        void setPivot(double x, double y);

        // The files of the assets of the game and the engine and the game's source, listed again when asked to or after a couple of seconds, and where one is on disk.
        const std::vector<AssetEntry> &assets() const;
        void refreshAssets() { m_assetsAt = std::chrono::steady_clock::time_point(); }
        std::string assetFile(const std::string &root, const std::string &path) const;

        // The folders of the game's assets and the engine's, empty ones included, written like "game/sprites".
        const std::vector<std::string> &assetDirectories() const;

        // Changes to the game's asset files, named by reference ("game/sprites/hero.png"). Scene files hold the references that follow a moved file, so a move first saves the open scenes and then reads them again, and the undo history of the open scenes is dropped. The scenes' own files stay where they are.
        bool createAssetFolder(const std::string &folder, const std::string &name, std::string *error);
        bool moveAsset(const std::string &reference, const std::string &folder, const std::string &name, std::string *error);
        bool deleteAsset(const std::string &reference, std::string *error);
        bool duplicateAsset(const std::string &reference, std::string *error);

        // Writes a new script into a folder of the game's source, as "src/nodes/enemies", that extends a class of the engine or of the game. Scripts are found by scenes only under src/nodes. A class is known to the editor once the player is built again.
        bool createScript(const std::string &folder, const std::string &name, const std::string &baseClass, std::string *error);

        // The scene files that name the asset, or a file under it when it is a folder.
        std::vector<std::string> assetUsers(const std::string &reference) const;

        // An image dropped on a node that has a texture becomes its texture, and anywhere else a new Sprite. A font becomes a new Label.
        void dropAsset(const std::string &root, const std::string &path, double frameX, double frameY);

        // Puts an asset in the tree as a child of a node, or at the top with no parent: a scene is instanced, an image becomes a Sprite and a font a Label. Anything else is left alone.
        void addAssetNode(const std::string &root, const std::string &path, const std::string &parentId);

        // What is wrong with the project's assets for the platform being previewed, and putting one right.
        std::vector<AssetIssue> issues() const;
        bool fixIssue(const AssetIssue &issue, std::string *error);

        // The grid drawn over a 2D scene, and what dragging a node snaps to: positions to its step, rotations to degrees, scales to tenths. Holding the snap key does the opposite of what snapping is set to.
        struct GridSettings
        {
            bool m_isShown = true;
            bool m_isSnapping = false;
            double m_step = 16;
            double m_rotationStep = 15;
            double m_scaleStep = 0.1;
        };
        const GridSettings &grid() const { return m_grid; }
        void setGrid(const GridSettings &grid);
        void setSnapHeld(bool isHeld) { m_isSnapHeld = isHeld; }

        GizmoMode gizmoMode() const { return m_gizmoMode; }
        void setGizmoMode(GizmoMode mode);

        // The size in pixels of the area the backend shows the frame in. The player draws frames of that size, so nothing is scaled and no pixel is drawn that is not seen. A change is sent once it has held still for a moment, so dragging a panel's edge does not resize the player on every step.
        void setViewSize(int width, int height);

        // Asks for the scene as the game would show it, at the size of the game's display on the platform, which is what the Game tab shows while the game is not running. The Scene view and the preview share the one player, so only one of them is drawn at a time.
        void setGamePreview(bool isWanted);
        const Frame &previewFrame() const { return m_previewFrame; }
        unsigned long long previewFrameSerial() const { return m_previewFrameSerial; }

        const Viewport &viewport() const { return m_viewport; }
        const Viewport3D &viewport3D() const { return m_viewport3D; }

        // What the game printed, and what its build did, as far as the log has it.
        LogTail &gameLog() { return m_gameLog; }
        const LogTail &gameLog() const { return m_gameLog; }

        ViewMode viewMode() const { return m_viewMode; }

        // Remembers where the view of this scene is, its mode, camera and selected node, which opening the scene again puts back. The editor keeps it by itself a moment after the view stops moving, and this is for when it is closing.
        void saveView();
        void setViewMode(ViewMode mode);

        // Moves the 3D camera along where it looks, across it and up, for the seconds a key was held. The speed grows with the distance to what the camera looks at, and fast is for a shift held.
        void fly(double forward, double right, double up, double seconds, bool fast);

        // Brings the selected node to the middle of the view: the 3D camera turns to look at it, and the 2D one moves to it and zooms out if it is larger than the view.
        void focusSelected();

        // A locked node cannot be picked in the view or moved there, but is still chosen in the hierarchy and edited in the inspector. Which are locked is the editor's and not the scene's, so the game never sees it, and it is remembered for each scene.
        bool isLocked(const std::string &id) const { return m_locked.count(id) > 0; }
        void setLocked(const std::string &id, bool isLocked);

        // The view as it is when a scene is first opened.
        void resetView();

        // Zooms the 2D view about its middle to this much, 1 being the game's own pixels.
        void zoomTo(double zoom);
        const Frame &frame() const { return m_frame; }

        // Counts the frames received, so a backend uploads a picture only when there is a new one.
        unsigned long frameSerial() const { return m_frameSerial; }
        DrawList overlay() const;

        void pointerDown(double frameX, double frameY, Pointer pointer);
        void pointerMove(double frameX, double frameY);
        void pointerUp();
        void wheel(double frameX, double frameY, double steps);

        // What a press at a point would grab, or what the drag in progress holds, so a backend can show it with the pointer.
        struct PointerTarget
        {
            enum class Kind
            {
                None,
                Object,
                Handle,
                Pan
            };
            Kind m_kind = Kind::None;
            GizmoHandle::Kind m_handle = GizmoHandle::Kind::MoveX;
        };
        PointerTarget pointerTarget(double frameX, double frameY) const;

        // A change of the selected node's transform that follows the pointer without a button held, as Blender's G, R and S keys do.
        enum class Modal
        {
            Move,
            Rotate,
            Scale
        };

        // Starts one at a point of the frame. Only in the 2D view, with a node selected that is not locked.
        bool beginModal(Modal kind, double frameX, double frameY);
        bool isModal() const { return m_modal.has_value(); }

        // Limits the change to one axis, "x" or "y", and frees it when it is asked for again.
        void modalAxis(char axis);

        // Types a number that replaces the pointer: pixels for a move, degrees for a rotation, a factor for a scale.
        void modalType(char character);
        void modalBackspace();
        void modalMove(double frameX, double frameY);
        void confirmModal();

        // Puts the node back as it was and leaves nothing to undo or redo.
        void cancelModal();

        // What the change in progress is, as "Move X: 12", for the status line.
        std::string modalText() const;

        // Whether a point is inside the selected node's box, whatever is drawn over it.
        bool isOverSelected(double frameX, double frameY) const;

        // Something the backend shows has changed: a frame, the selection, the document or the status.
        std::function<void()> m_onChanged;

        // The handles of the gizmo on the selected node, as they are placed in a view.
        std::vector<GizmoHandle> handlesFor(const Viewport &view) const;

    private:
        // A scene open in a tab that is not the one being edited, with its changes and its history.
        struct OpenScene
        {
            std::string m_path;
            SceneDocument m_document;
            std::unique_ptr<CommandStack> m_commands;
        };

        void startPlayer();
        void reloadScenes();
        // The directory a reference to the game's own files is relative to, the assets or the source, and the path in it. The engine's files are read only.
        bool ownedPath(const std::string &reference, std::string *directory, std::string *path, std::string *error) const;
        void activateParked(const std::string &path);
        void bind(SceneDocument &document);
        std::optional<nlohmann::json> sceneRoot(const std::string &sceneName) const;
        bool reaches(const std::string &from, const std::string &target, int depth) const;
        void rememberTabs();
        void launchPlay(bool isFromStartScene);
        // The game moved to another scene: the editor shows that one, as Unity's hierarchy does, until the game comes back to the first or stops.
        void followGameScene(const std::string &name);
        void restoreOrigin(bool isHolding);
        void adoptScene(SceneDocument document, const std::string &path);
        void chooseViewMode();
        void rememberScene();
        void loadLocked();
        void restoreView();
        DrawList overlay3D() const;
        std::optional<std::string> hit3D(double frameX, double frameY) const;
        const ObjectBounds3D *bounds3DOf(const std::string &id) const;
        std::vector<AxisHandle> handles3D() const;
        void projectChanged();
        void showFailure(const std::string &title, const std::string &message, const std::vector<fried::core::OutputLine> *lines);
        void documentChanged(const DocumentChange &change);
        void cameraChanged();
        void changed();
        TransformValues transformOf(const std::string &id) const;
        DrawList anchorOverlay() const;
        bool isControl(const std::string &id) const;
        const ObjectBounds *boundsOf(const std::string &id) const;

        // What a drag asks of a node's position, as the fields to write. A Control's position is a result of its layout, so moving it changes its offsets, or its insets where it stretches, and a Node2D under another is moved by what the parent's rotation and scale turn the drag into.
        std::map<std::string, double> dragFields(const std::string &id, std::map<std::string, double> fields);
        std::vector<GizmoHandle> currentHandles() const;
        double gameCameraZoom() const;
        void execute(std::unique_ptr<Command> command, bool merge);
        void setTransformFields(const std::string &id, const std::map<std::string, double> &fields, bool merge);
        nlohmann::json newNode(const std::string &type) const;
        std::string playerLogFile() const;
        std::map<std::string, double> snapped(std::map<std::string, double> fields) const;
        bool insertCopy(const nlohmann::json &source, const std::string &parentId, int index, const std::string &label);

        Project m_project;
        ProjectSettings m_settings{m_project};
        ProjectBuild m_projectBuild{m_project};
        std::string m_enginePath;
        fried::core::Toolchains m_toolchains;
        std::string m_scenePath;
        SceneDocument m_document;
        std::unique_ptr<CommandStack> m_commands;
        PlayerSession m_session;
        std::unique_ptr<PlayerBuilder> m_builder;
        GameRunner m_game;
        GameSession m_play;
        bool m_isPlaying = false;
        bool m_isSceneWatched = false;
        // The scene that was open when the game started, kept aside while the game is on another one.
        struct PlayOrigin
        {
            std::string m_path;
            SceneDocument m_document;
            std::unique_ptr<CommandStack> m_commands;
        };
        std::optional<PlayOrigin> m_origin;
        std::vector<std::string> m_tabs;
        mutable std::map<std::string, std::pair<std::filesystem::file_time_type, nlohmann::json>> m_sceneFiles;
        std::map<std::string, OpenScene> m_parked;
        // The values the running game last reported for each node, over what the file holds. They show in the inspector and in the scene view while it runs.
        std::map<std::string, nlohmann::json> m_liveFields;
        unsigned long long m_liveSerial = 0;
        mutable unsigned long long m_inspectorLiveSerial = 0;
        // Set while the player is rebuilt so that the game can start with it, true when it is to start from the project's start scene.
        std::optional<bool> m_pendingPlay;
        GameFrame m_gameFrame;
        unsigned long long m_gameFrameSerial = 0;
        bool m_isFailureShown = false;
        std::string m_failedTitle;
        std::vector<fried::core::OutputLine> m_failedLines;
        std::string m_failedMessage;
        Status m_status = Status::Idle;
        std::string m_message;
        Platform m_platform = Platform::Pc;
        std::optional<std::string> m_selected;
        nlohmann::json m_clipboard;
        std::set<std::string> m_locked;
        LogTail m_gameLog;
        std::chrono::steady_clock::time_point m_logPolledAt;
        Viewport m_viewport;

        // The view the player drew the frame on screen with. The overlay is drawn with it, not with the view the user has moved on to since, so the two stay together while panning.
        Viewport m_frameView;
        Viewport m_sentView;
        Viewport m_inFlightView;
        ViewMode m_viewMode = ViewMode::TwoD;
        bool m_isViewDirty = false;
        std::chrono::steady_clock::time_point m_viewChangedAt;
        Viewport3D m_viewport3D;
        Viewport3D m_frameView3D;
        Viewport3D m_sentView3D;
        Viewport3D m_inFlightView3D;
        Frame m_frame;
        unsigned long m_frameSerial = 0;
        GizmoMode m_gizmoMode = GizmoMode::Move;
        GridSettings m_grid;
        bool m_isSnapHeld = false;

        void wantSize(int width, int height);
        bool m_isPreviewing = false;
        bool m_isPreviewInFlight = false;
        bool m_needsPreviewMode = false;
        int m_sceneWidth = 0;
        int m_sceneHeight = 0;
        Frame m_previewFrame;
        unsigned long long m_previewFrameSerial = 0;
        int m_wantedWidth = 0;
        int m_wantedHeight = 0;
        int m_requestedWidth = 0;
        int m_requestedHeight = 0;
        std::chrono::steady_clock::time_point m_wantedSince;

        mutable std::vector<HierarchyRow> m_hierarchyRows;
        mutable unsigned long m_hierarchyVersion = 0;
        mutable std::vector<ClassView> m_inspectorViews;
        mutable unsigned long m_inspectorVersion = 0;
        mutable std::optional<std::string> m_inspectorSelected;
        mutable std::size_t m_inspectorSchemaSize = 0;
        double m_playSpeed = 1.0;
        mutable std::vector<AssetEntry> m_assetsCache;
        mutable std::vector<std::string> m_directoriesCache;
        mutable std::chrono::steady_clock::time_point m_assetsAt;
        bool m_mayResync = false;

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
                Pan,
                Look,
                Handle3D
            };
            Kind m_kind = Kind::None;
            std::string m_id;
            double m_startWorldX = 0;
            double m_startWorldY = 0;
            double m_startX = 0;
            double m_startY = 0;
            double m_appliedX = 0;
            double m_appliedY = 0;
            double m_originX = 0;
            double m_originY = 0;
            double m_startFrameX = 0;
            double m_startFrameY = 0;
            double m_lastFrameX = 0;
            double m_lastFrameY = 0;
            bool m_hasMoved = false;
            std::optional<GizmoDrag> m_handle;
            std::optional<AxisHandle> m_axisHandle;
            double m_startValue = 0;
        } m_drag;

        struct ModalState
        {
            Modal m_kind = Modal::Move;
            char m_axis = 0;
            std::string m_typed;
            std::string m_id;
            TransformValues m_start;
            double m_startFrameX = 0;
            double m_startFrameY = 0;
            double m_originFrameX = 0;
            double m_originFrameY = 0;
            double m_frameX = 0;
            double m_frameY = 0;
        };
        std::optional<ModalState> m_modal;

        void applyModal();
    };
}
