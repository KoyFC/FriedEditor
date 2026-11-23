#include "fried/editor/editor.h"

#include "fried/editor/asset_files.h"
#include "fried/editor/commands.h"
#include "fried/editor/editor_state.h"
#include "fried/editor/script_files.h"

#include "fried/core/engine_location.h"
#include <fstream>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <set>

namespace fried::editor
{
    using Json = nlohmann::json;
    namespace fs = std::filesystem;

    // An engine from before the editor player existed cannot show a scene, so it does not count.
    std::string Editor::findEngine(const std::string &projectDirectory)
    {
        std::string engine = fried::core::findEngine(projectDirectory);
        std::error_code ignored;
        return fs::exists(fs::path(engine) / "editor" / "player" / "build.hxml", ignored) ? engine : std::string();
    }

    bool Editor::open(const std::string &projectDirectory, const std::string &enginePath, std::string *error)
    {
        stopPlay();
        m_pendingPlay.reset();
        if (!m_project.load(projectDirectory, error))
        {
            return false;
        }
        m_enginePath = enginePath;
        m_toolchains = machineToolchains();
        m_settings.m_onSaved = [this] { projectChanged(); };
        m_settings.m_projectClasses = [this] {
            std::vector<std::string> names;
            for (const ClassSchema &schema : m_session.schema())
            {
                if (!schema.m_isBuiltin)
                {
                    names.push_back(schema.m_name);
                }
            }
            return names;
        };

        std::vector<std::string> scenes = scenePaths();
        if (scenes.empty())
        {
            m_scenePath = m_project.scenesDirectory() + "/main.scene.json";
            m_document = SceneDocument::withRoot("Main", "Node2D");
        }
        else
        {
            // The scene that was open last time, when it is still there.
            std::string chosen = scenes.front();
            // Without a scene remembered, the project's own first scene is the one it starts with.
            std::string remembered = EditorState::lastScene(m_project.directory());
            if (remembered.empty() && !m_project.startScene().empty())
            {
                remembered = m_project.startScene() + ".scene.json";
            }
            for (const std::string &path : scenes)
            {
                if (!remembered.empty() && fs::path(path).filename() == remembered)
                {
                    chosen = path;
                }
            }
            if (!m_document.load(chosen, error))
            {
                return false;
            }
            m_scenePath = chosen;
            rememberScene();
            loadLocked();
        }

        m_commands = std::make_unique<CommandStack>(m_document);
        if (m_document.wasMigrated())
        {
            m_commands->markUnsaved();
        }
        m_commands->m_onChange = [this] { changed(); };
        m_document.m_onChange = [this](const DocumentChange &change) { documentChanged(change); };
        bind(m_document);
        m_parked.clear();
        m_tabs.clear();
        Json openScenes = EditorState::projectValue(m_project.directory(), "openScenes");
        if (openScenes.is_array())
        {
            std::error_code ignored;
            for (const Json &name : openScenes)
            {
                std::string path = m_project.scenesDirectory() + "/" + (name.is_string() ? name.get<std::string>() : std::string());
                if (!name.is_string() || std::find(m_tabs.begin(), m_tabs.end(), path) != m_tabs.end())
                {
                    continue;
                }
                if (path == m_scenePath)
                {
                    m_tabs.push_back(path);
                    continue;
                }
                OpenScene opened;
                opened.m_path = path;
                std::string ignoredError;
                if (fs::exists(path, ignored) && opened.m_document.load(path, &ignoredError))
                {
                    bind(opened.m_document);
                    opened.m_commands = std::make_unique<CommandStack>(m_document);
                    m_parked[path] = std::move(opened);
                    m_tabs.push_back(path);
                }
            }
        }
        if (std::find(m_tabs.begin(), m_tabs.end(), m_scenePath) == m_tabs.end())
        {
            m_tabs.push_back(m_scenePath);
        }
        m_viewport.setFrameSize(s_frameWidth, s_frameHeight);
        m_viewport.setCamera(s_frameWidth / 2.0, s_frameHeight / 2.0, 1.0);
        Json grid = EditorState::projectValue(m_project.directory(), "grid");
        if (grid.is_object())
        {
            m_grid.m_isShown = grid.value("shown", m_grid.m_isShown);
            m_grid.m_isSnapping = grid.value("snap", m_grid.m_isSnapping);
            m_grid.m_step = std::max(1.0, grid.value("step", m_grid.m_step));
            m_grid.m_rotationStep = std::max(0.1, grid.value("rotation", m_grid.m_rotationStep));
            m_grid.m_scaleStep = std::max(0.001, grid.value("scale", m_grid.m_scaleStep));
        }

        if (isPlayerStale(m_enginePath, m_project.directory(), m_toolchains))
        {
            rebuildPlayer();
        }
        else
        {
            startPlayer();
        }
        return true;
    }

    std::string Editor::playerLogFile() const
    {
        return (fs::temp_directory_path() / "fried-editor-player-build.log").string();
    }

    BuildProgress Editor::buildProgress() const
    {
        BuildProgress progress;
        if (m_builder)
        {
            progress.m_isVisible = true;
            progress.m_title = "Building the editor player";
            progress.m_step = (int)m_builder->step();
            progress.m_steps = (int)m_builder->stepCount();
            progress.m_stepLabel = m_builder->stepLabel();
            progress.m_lines = &m_builder->lines();
        }
        else if (m_game.state() == GameRunner::State::Building)
        {
            progress.m_isVisible = true;
            progress.m_canStop = true;
            progress.m_title = "Building the game";
            progress.m_step = (int)m_game.step();
            progress.m_steps = (int)m_game.stepCount();
            progress.m_stepLabel = m_game.stepLabel();
            progress.m_lines = m_game.lines();
        }
        else if (m_projectBuild.state() != ProjectBuild::State::Idle)
        {
            const fried::core::StepRunner &runner = m_projectBuild.runner();
            std::string target = fried::core::buildTargetName(m_projectBuild.target());
            progress.m_isVisible = true;
            progress.m_canStop = m_projectBuild.isRunning();
            progress.m_isFailed = runner.state() == ProjectBuild::State::Failed;
            progress.m_isFinished = !m_projectBuild.isRunning();
            progress.m_title = m_projectBuild.isRunning() ? "Building for " + target : progress.m_isFailed ? "Building for " + target + " failed" : "Built for " + target;
            progress.m_step = (int)runner.step();
            progress.m_steps = (int)runner.stepCount();
            progress.m_stepLabel = runner.stepLabel();
            progress.m_message = runner.failure();
            progress.m_builtFile = progress.m_isFinished && !progress.m_isFailed ? m_projectBuild.builtFile() : std::string();
            progress.m_lines = &runner.lines();
        }
        else if (m_isFailureShown)
        {
            progress.m_isVisible = true;
            progress.m_isFailed = true;
            progress.m_isFinished = true;
            progress.m_title = m_failedTitle;
            progress.m_message = m_failedMessage;
            progress.m_lines = m_failedLines.empty() ? nullptr : &m_failedLines;
        }
        return progress;
    }

    void Editor::dismissBuild()
    {
        m_isFailureShown = false;
        m_projectBuild.dismiss();
        changed();
    }

    void Editor::showFailure(const std::string &title, const std::string &message, const std::vector<fried::core::OutputLine> *lines)
    {
        m_isFailureShown = true;
        m_failedTitle = title;
        m_failedMessage = message;
        m_failedLines = lines ? *lines : std::vector<fried::core::OutputLine>();
    }

    void Editor::stopBuild()
    {
        if (m_projectBuild.isRunning())
        {
            m_projectBuild.cancel();
            changed();
        }
        else
        {
            stopGame();
        }
    }

    // The scene is laid out against the display, so a change to the project is a reason to lay it out again.
    void Editor::projectChanged()
    {
        if (m_isPlaying)
        {
            m_play.setInput(m_project.inputBlock());
        }
        m_needsLoad = true;
        changed();
    }

    bool Editor::buildProject(fried::core::BuildTarget target, std::string *error)
    {
        if (m_isPlaying)
        {
            *error = "Stop the game to build the project.";
            return false;
        }
        if (m_builder || m_game.state() == GameRunner::State::Building || m_projectBuild.isRunning())
        {
            *error = "Something else is being built.";
            return false;
        }
        if (!m_commands->isClean() && !save(error))
        {
            return false;
        }
        m_isFailureShown = false;
        if (!m_projectBuild.start(target, error))
        {
            return false;
        }
        m_message = "Building for " + fried::core::buildTargetName(target);
        changed();
        return true;
    }

    void Editor::rebuildPlayer()
    {
        m_isFailureShown = false;
        m_toolchains = machineToolchains();
        std::string error;
        std::vector<SubprocessOptions> steps = hasProjectPlayer(m_project.directory())
            ? PlayerBuilder::projectSteps(m_enginePath, m_project.directory(), playerLogFile(), m_toolchains, &error)
            : PlayerBuilder::engineSteps(m_enginePath, playerLogFile(), m_toolchains, &error);
        if (steps.empty())
        {
            m_status = Status::Failed;
            m_message = error;
            showFailure("The editor player cannot be built", error, nullptr);
            changed();
            return;
        }
        m_builder = std::make_unique<PlayerBuilder>(std::move(steps));
        m_status = Status::BuildingPlayer;
        m_message = "Building the editor player, the log is " + playerLogFile();
        changed();
    }

    void Editor::startPlayer()
    {
        m_session = PlayerSession();
        m_session.m_onReady = [this] {
            m_status = Status::Ready;
            m_message = "Playing on " + platformLabel(m_platform);
            chooseViewMode();
            m_needsLoad = true;
            m_needsCamera = true;
            m_needsPreviewMode = true;
            m_needsRender = true;
            if (m_pendingPlay)
            {
                bool isFromStartScene = *m_pendingPlay;
                m_pendingPlay.reset();
                launchPlay(isFromStartScene);
            }
            changed();
        };
        m_session.m_onFrame = [this](const Frame &frame) {
            m_isRenderInFlight = false;
            if (m_isPreviewInFlight)
            {
                m_previewFrame = frame;
                ++m_previewFrameSerial;
                changed();
                return;
            }
            m_frame = frame;
            m_frameView = m_inFlightView;
            m_frameView.setFrameSize(frame.m_width, frame.m_height);
            m_frameView3D = m_inFlightView3D;
            m_frameView3D.setFrameSize(frame.m_width, frame.m_height);
            m_viewport3D.setFrameSize(frame.m_width, frame.m_height);
            ++m_frameSerial;
            m_viewport.setFrameSize(frame.m_width, frame.m_height);
            changed();
        };
        m_session.m_onError = [this](const std::string &message) {
            m_message = message;
            m_isRenderInFlight = false;
            // An edit the player could not make leaves it out of step with the document, so it is shown the whole scene once more. Only after such an edit, so a scene it cannot load does not ask again.
            if (m_mayResync)
            {
                m_mayResync = false;
                m_needsLoad = true;
            }
            if (m_session.state() == PlayerSession::State::Closed)
            {
                m_status = Status::Failed;
            }
            changed();
        };

        std::string error;
        if (!m_session.launch(playerExecutablePath(m_enginePath, m_project.directory(), m_toolchains), m_enginePath, m_project.directory(), s_frameWidth, s_frameHeight, &error))
        {
            m_status = Status::Failed;
            m_message = error;
        }
        else
        {
            m_status = Status::StartingPlayer;
            m_message = "Starting the editor player";
        }
        changed();
    }

    void Editor::buildAndRun()
    {
        if (m_isPlaying)
        {
            m_message = "Stop the game to build and run it.";
            changed();
            return;
        }
        if (m_game.state() == GameRunner::State::Building || m_game.state() == GameRunner::State::Running)
        {
            return;
        }
        m_isFailureShown = false;
        std::string error;
        if (!m_commands->isClean() && !save(&error))
        {
            m_message = error;
            changed();
            return;
        }
        std::string log = (fs::temp_directory_path() / "fried-editor-game.log").string();
        m_gameLog.follow(log);
        m_gameLog.clear();
        m_toolchains = machineToolchains();
        if (m_game.start(m_project.directory(), log, m_toolchains, &error))
        {
            m_message = "Building the game, the log is " + log;
        }
        else
        {
            m_message = error;
            showFailure("The game cannot be built", error, nullptr);
        }
        changed();
    }

    void Editor::stopGame()
    {
        m_game.stop();
        m_message = "Stopped the game";
        changed();
    }

    void Editor::startPlay(bool isFromStartScene)
    {
        if (m_isPlaying || m_pendingPlay || m_builder || m_status != Status::Ready || m_game.state() == GameRunner::State::Building)
        {
            return;
        }
        if (!hasProjectPlayer(m_project.directory()))
        {
            m_message = "The project has no editor.hxml, so its game cannot run inside the editor. Build and run it instead.";
            changed();
            return;
        }
        m_isFailureShown = false;
        m_toolchains = machineToolchains();
        if (isPlayerStale(m_enginePath, m_project.directory(), m_toolchains))
        {
            // The game has to be the code on disk, and a player that is running cannot be replaced on every system, so the editor's own goes first.
            m_pendingPlay = isFromStartScene;
            m_session = PlayerSession();
            rebuildPlayer();
            return;
        }
        launchPlay(isFromStartScene);
    }

    void Editor::launchPlay(bool isFromStartScene)
    {
        std::string log = (fs::temp_directory_path() / "fried-editor-game.log").string();
        m_gameLog.follow(log);
        m_gameLog.clear();

        m_play = GameSession();
        m_play.m_onFrame = [this](GameFrame &&frame) {
            m_gameFrame = std::move(frame);
            ++m_gameFrameSerial;
            changed();
        };
        m_origin.reset();
        m_liveFields.clear();
        ++m_liveSerial;
        m_play.m_onSceneChanged = [this](const std::string &name) { followGameScene(name); };
        m_play.m_onState = [this](const Json &nodes) {
            for (const Json &entry : nodes)
            {
                if (!entry.is_object() || !entry.contains("id") || !entry.contains("fields") || !entry["fields"].is_object())
                {
                    continue;
                }
                const std::string id = entry["id"].get<std::string>();
                Json &live = m_liveFields[id];
                for (const auto &field : entry["fields"].items())
                {
                    live[field.key()] = field.value();
                }
                if (m_selected && *m_selected == id)
                {
                    ++m_liveSerial;
                }
            }
            // The scene view is the editor's own player, shown what the game reports. A whole scene on its way takes the values with it.
            if (m_status == Status::Ready && !m_needsLoad)
            {
                m_session.setFields(nodes);
            }
            m_needsRender = true;
            changed();
        };
        m_play.m_onError = [this](const std::string &message) {
            m_message = message;
            changed();
        };

        std::string name = fs::path(m_scenePath).filename().string();
        const std::string suffix = ".scene.json";
        if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
        {
            name.resize(name.size() - suffix.size());
        }
        std::string error;
        if (!m_play.listen(name, m_document.data(), isFromStartScene, &error)
            || !m_play.launch(playerExecutablePath(m_enginePath, m_project.directory(), m_toolchains), m_enginePath, m_project.directory(), log, &error))
        {
            m_message = error;
            showFailure("The game cannot start", error, nullptr);
            changed();
            return;
        }
        m_play.setSpeed(m_playSpeed);
        m_play.setInput(m_project.inputBlock());
        m_commands->hold();
        m_isPlaying = true;
        m_gameFrame = GameFrame();
        m_message = "Playing the game";
        changed();
    }

    void Editor::setPaused(bool isPaused)
    {
        if (m_isPlaying)
        {
            m_play.setPaused(isPaused);
            m_message = isPaused ? "Paused the game" : "Playing the game";
            changed();
        }
    }

    void Editor::stepFrame()
    {
        if (m_isPlaying && m_play.isPaused())
        {
            m_play.stepFrame();
        }
    }

    void Editor::setPlaySpeed(double scale)
    {
        m_playSpeed = scale;
        if (m_isPlaying)
        {
            m_play.setSpeed(scale);
        }
        changed();
    }

    void Editor::stopPlay()
    {
        m_pendingPlay.reset();
        if (!m_isPlaying)
        {
            return;
        }
        m_isPlaying = false;
        m_play.stop();
        m_liveFields.clear();
        ++m_liveSerial;
        m_needsLoad = true;
        // What was edited while it ran goes with it, and the edit player, which was told of each change, is told of the undoing. A scene the game moved to is dropped for the one that was open.
        if (m_origin)
        {
            restoreOrigin(false);
        }
        else
        {
            m_commands->revertHeld();
        }
        m_message = "Stopped the game";
        changed();
    }

    void Editor::followGameScene(const std::string &name)
    {
        std::string path = m_project.scenesDirectory() + "/" + name + ".scene.json";
        if (!m_isPlaying || name.empty() || path == m_scenePath)
        {
            return;
        }
        if (m_origin && path == m_origin->m_path)
        {
            restoreOrigin(true);
            return;
        }
        SceneDocument loaded;
        std::string error;
        if (!loaded.load(path, &error))
        {
            m_message = "The game is on a scene the editor cannot read: " + error;
            changed();
            return;
        }
        // What was edited on the scene being left is taken back first, without telling the game, which has already left it.
        m_isPlaying = false;
        m_commands->revertHeld();
        m_isPlaying = true;
        saveView();
        if (!m_origin)
        {
            m_origin = PlayOrigin{m_scenePath, m_document, std::move(m_commands)};
        }
        adoptScene(std::move(loaded), path);
        m_commands->hold();
    }

    void Editor::restoreOrigin(bool isHolding)
    {
        saveView();
        PlayOrigin origin = std::move(*m_origin);
        m_origin.reset();
        m_document.m_onChange = nullptr;
        m_document = std::move(origin.m_document);
        m_document.m_onChange = [this](const DocumentChange &change) { documentChanged(change); };
        m_commands = std::move(origin.m_commands);
        m_commands->m_onChange = [this] { changed(); };
        m_scenePath = origin.m_path;
        loadLocked();
        m_selected.reset();
        chooseViewMode();
        m_liveFields.clear();
        ++m_liveSerial;
        m_needsLoad = true;
        m_needsCamera = true;
        if (isHolding)
        {
            m_commands->hold();
        }
        changed();
    }

    void Editor::adoptScene(SceneDocument document, const std::string &path)
    {
        m_document.m_onChange = nullptr;
        m_document = std::move(document);
        m_document.m_onChange = [this](const DocumentChange &change) { documentChanged(change); };
        bind(m_document);
        m_commands = std::make_unique<CommandStack>(m_document);
        m_commands->m_onChange = [this] { changed(); };
        m_scenePath = path;
        loadLocked();
        m_selected.reset();
        chooseViewMode();
        m_liveFields.clear();
        ++m_liveSerial;
        m_needsLoad = true;
        m_needsCamera = true;
        changed();
    }

    void Editor::setViewSize(int width, int height)
    {
        m_sceneWidth = width;
        m_sceneHeight = height;
        if (!m_isPreviewing)
        {
            wantSize(width, height);
        }
    }

    void Editor::setGamePreview(bool isWanted)
    {
        if (isWanted == m_isPreviewing)
        {
            return;
        }
        m_isPreviewing = isWanted;
        m_needsPreviewMode = true;
        if (isWanted)
        {
            std::pair<int, int> display = m_project.logicalSize(m_platform);
            wantSize(display.first, display.second);
        }
        else if (m_sceneWidth > 0)
        {
            wantSize(m_sceneWidth, m_sceneHeight);
        }
        m_needsRender = true;
        changed();
    }

    void Editor::wantSize(int width, int height)
    {
        width = std::clamp(width / 2 * 2, 160, 4096);
        height = std::clamp(height / 2 * 2, 120, 4096);
        if (width != m_wantedWidth || height != m_wantedHeight)
        {
            m_wantedWidth = width;
            m_wantedHeight = height;
            m_wantedSince = std::chrono::steady_clock::now();
        }
    }

    void Editor::pump()
    {
        // The game's log is read a few times a second while it is being written, and once more after it stops.
        auto now = std::chrono::steady_clock::now();
        if (now - m_logPolledAt > std::chrono::milliseconds(100))
        {
            m_logPolledAt = now;
            if (m_gameLog.poll())
            {
                changed();
            }
        }

        // The view is written once it has stopped moving, so dragging it does not write the file at every frame.
        if (m_isViewDirty && std::chrono::steady_clock::now() - m_viewChangedAt > std::chrono::milliseconds(500))
        {
            saveView();
        }

        if (m_projectBuild.isRunning() && m_projectBuild.pump() != ProjectBuild::State::Running)
        {
            m_message = m_projectBuild.state() == ProjectBuild::State::Succeeded ? "Built for " + fried::core::buildTargetName(m_projectBuild.target()) : m_projectBuild.runner().failure();
            changed();
        }

        GameRunner::State before = m_game.state();
        std::vector<fried::core::OutputLine> gameLines = m_game.lines() ? *m_game.lines() : std::vector<fried::core::OutputLine>();
        GameRunner::State after = m_game.pump();
        if (after != before)
        {
            if (after == GameRunner::State::Running)
            {
                m_message = "Playing the game";
            }
            else if (after == GameRunner::State::Failed)
            {
                m_message = m_game.failure();
                showFailure("Building the game failed", m_game.failure(), &gameLines);
            }
            else if (after == GameRunner::State::Idle)
            {
                m_message = "The game closed";
            }
            changed();
        }

        if (m_isPlaying)
        {
            m_play.setWatch(m_selected.value_or(std::string()), m_isSceneWatched);
            m_play.pump();
            if (m_play.state() == GameSession::State::Closed)
            {
                stopPlay();
            }
        }

        if (m_builder)
        {
            PlayerBuilder::State state = m_builder->pump();
            if (state == PlayerBuilder::State::Failed)
            {
                m_pendingPlay.reset();
                m_status = Status::Failed;
                m_message = m_builder->failure() + " The log is " + playerLogFile();
                showFailure("Building the editor player failed", m_builder->failure(), &m_builder->lines());
                m_builder.reset();
                changed();
            }
            else if (state == PlayerBuilder::State::Succeeded)
            {
                m_builder.reset();
                startPlayer();
            }
            return;
        }

        m_session.pump();
        if (m_status != Status::Ready)
        {
            return;
        }
        if (m_wantedWidth > 0 && (m_wantedWidth != m_requestedWidth || m_wantedHeight != m_requestedHeight)
            && std::chrono::steady_clock::now() - m_wantedSince > std::chrono::milliseconds(150))
        {
            bool isFirst = m_requestedWidth == 0;
            m_requestedWidth = m_wantedWidth;
            m_requestedHeight = m_wantedHeight;
            // The player starts at the size the editor first launched it with, so only a different size is worth asking for.
            if (!isFirst || m_wantedWidth != s_frameWidth || m_wantedHeight != s_frameHeight)
            {
                m_session.resize(m_wantedWidth, m_wantedHeight);
                m_needsRender = true;
            }
        }
        if (m_needsLoad)
        {
            m_needsLoad = false;
            // The scene is laid out against the size the game's screen has on the platform being previewed.
            std::pair<int, int> logicalView = m_project.logicalSize(m_platform);
            m_session.load(m_document.data(), logicalView.first, logicalView.second);
            if (m_isPlaying)
            {
                // The game is what is on show, not the file, so what it last reported goes over the new scene.
                Json restored = Json::array();
                for (const auto &entry : m_liveFields)
                {
                    restored.push_back({{"id", entry.first}, {"fields", entry.second}});
                }
                m_session.setFields(restored);
            }
            m_needsRender = true;
        }
        if (m_needsPreviewMode)
        {
            m_needsPreviewMode = false;
            m_session.setPreview(m_isPreviewing);
        }
        if (m_needsCamera)
        {
            m_needsCamera = false;
            m_session.setEditorView(m_viewMode == ViewMode::ThreeD);
            m_session.setEditorCamera(m_viewport.cameraX(), m_viewport.cameraY(), m_viewport.zoom());
            m_session.setEditorCamera3D(m_viewport3D.x(), m_viewport3D.y(), m_viewport3D.z(), m_viewport3D.rotationX(), m_viewport3D.rotationY(),
                                        m_viewport3D.fieldOfView());
            m_sentView = m_viewport;
            m_sentView3D = m_viewport3D;
            m_needsRender = true;
        }
        if (m_needsRender && !m_isRenderInFlight)
        {
            m_needsRender = false;
            m_isRenderInFlight = true;
            m_inFlightView = m_sentView;
            m_inFlightView3D = m_sentView3D;
            m_isPreviewInFlight = m_isPreviewing;
            m_session.render();
        }
    }

    void Editor::loadLocked()
    {
        m_locked.clear();
        Json remembered = EditorState::projectValue(m_project.directory(), "locked");
        std::string name = fs::path(m_scenePath).filename().string();
        if (remembered.is_object() && remembered.contains(name) && remembered[name].is_array())
        {
            for (const Json &id : remembered[name])
            {
                if (id.is_string() && m_document.contains(id.get<std::string>()))
                {
                    m_locked.insert(id.get<std::string>());
                }
            }
        }
    }

    void Editor::setLocked(const std::string &id, bool isLocked)
    {
        if (!m_document.contains(id) || this->isLocked(id) == isLocked)
        {
            return;
        }
        if (isLocked)
        {
            m_locked.insert(id);
        }
        else
        {
            m_locked.erase(id);
        }
        if (isLocked && m_selected == id)
        {
            m_drag = Drag();
        }
        Json remembered = EditorState::projectValue(m_project.directory(), "locked");
        if (!remembered.is_object())
        {
            remembered = Json::object();
        }
        std::vector<std::string> ids;
        for (const std::string &locked : m_locked)
        {
            if (m_document.contains(locked))
            {
                ids.push_back(locked);
            }
        }
        remembered[fs::path(m_scenePath).filename().string()] = Json(ids);
        EditorState::setProjectValue(m_project.directory(), "locked", remembered);
        m_needsRender = true;
        changed();
    }

    void Editor::rememberScene()
    {
        std::string name = fs::path(m_scenePath).filename().string();
        EditorState::setLastScene(m_project.directory(), name);
        std::vector<std::string> recent = {name};
        Json remembered = EditorState::projectValue(m_project.directory(), "recentScenes");
        if (remembered.is_array())
        {
            for (const Json &other : remembered)
            {
                if (other.is_string() && other.get<std::string>() != name && recent.size() < 8)
                {
                    recent.push_back(other.get<std::string>());
                }
            }
        }
        EditorState::setProjectValue(m_project.directory(), "recentScenes", Json(recent));
    }

    std::vector<std::string> Editor::recentScenes() const
    {
        std::vector<std::string> paths;
        Json remembered = EditorState::projectValue(m_project.directory(), "recentScenes");
        if (!remembered.is_array())
        {
            return paths;
        }
        std::error_code ignored;
        for (const Json &name : remembered)
        {
            std::string path = m_project.scenesDirectory() + "/" + (name.is_string() ? name.get<std::string>() : std::string());
            if (name.is_string() && fs::exists(path, ignored))
            {
                paths.push_back(path);
            }
        }
        return paths;
    }

    std::vector<std::string> Editor::scenePaths() const
    {
        std::vector<std::string> paths;
        std::error_code ignored;
        for (const auto &entry : fs::directory_iterator(m_project.scenesDirectory(), ignored))
        {
            std::string name = entry.path().filename().string();
            if (name.size() > 11 && name.compare(name.size() - 11, 11, ".scene.json") == 0)
            {
                paths.push_back(entry.path().string());
            }
        }
        std::sort(paths.begin(), paths.end());
        return paths;
    }

    bool Editor::openScene(const std::string &chosen, std::string *error)
    {
        std::string path = fs::path(chosen).lexically_normal().string();
        if (m_isPlaying)
        {
            *error = "Stop the game to open another scene.";
            return false;
        }
        if (path == m_scenePath && std::find(m_tabs.begin(), m_tabs.end(), path) != m_tabs.end())
        {
            return true;
        }
        if (m_parked.find(path) == m_parked.end())
        {
            OpenScene opened;
            opened.m_path = path;
            if (!opened.m_document.load(path, error))
            {
                return false;
            }
            bind(opened.m_document);
            opened.m_commands = std::make_unique<CommandStack>(m_document);
            if (opened.m_document.wasMigrated())
            {
                opened.m_commands->markUnsaved();
            }
            m_parked[path] = std::move(opened);
            m_tabs.push_back(path);
        }
        activateParked(path);
        return true;
    }

    bool Editor::newScene(const std::string &chosen, const std::string &rootType, std::string *error)
    {
        std::size_t first = chosen.find_first_not_of(" \t");
        std::string name = first == std::string::npos ? std::string() : chosen.substr(first, chosen.find_last_not_of(" \t") - first + 1);
        if (name.empty() || name.find_first_of("/\\") != std::string::npos || name.front() == '.')
        {
            *error = "A scene needs a name that is a file name, with no folders in it.";
            return false;
        }
        std::string path = scenePathOf(name);
        std::error_code ignored;
        if (fs::exists(path, ignored))
        {
            *error = "The project already has a scene named " + name + ".";
            return false;
        }
        if (m_session.classChain(rootType).empty())
        {
            *error = "The player does not know a class named " + rootType + ".";
            return false;
        }
        fs::create_directories(m_project.scenesDirectory(), ignored);
        if (!SceneDocument::withRoot(name, rootType).save(path, error))
        {
            return false;
        }
        return openScene(path, error);
    }

    // Puts the scene being edited aside, with its changes and its history, and edits the one of the tab.
    void Editor::activateParked(const std::string &path)
    {
        saveView();
        OpenScene next = std::move(m_parked[path]);
        m_parked.erase(path);
        m_document.m_onChange = nullptr;
        OpenScene current;
        current.m_path = m_scenePath;
        current.m_document = std::move(m_document);
        current.m_commands = std::move(m_commands);
        if (std::find(m_tabs.begin(), m_tabs.end(), m_scenePath) != m_tabs.end())
        {
            m_parked[m_scenePath] = std::move(current);
        }
        m_document = std::move(next.m_document);
        m_document.m_onChange = [this](const DocumentChange &change) { documentChanged(change); };
        m_commands = std::move(next.m_commands);
        m_commands->m_onChange = [this] { changed(); };
        m_scenePath = path;
        rememberScene();
        rememberTabs();
        loadLocked();
        m_selected.reset();
        chooseViewMode();
        m_needsLoad = true;
        // The view the scene remembers is not the one the player has, so it is sent with the scene.
        m_needsCamera = true;
        changed();
    }

    void Editor::rememberTabs()
    {
        Json names = Json::array();
        for (const std::string &path : m_tabs)
        {
            names.push_back(fs::path(path).filename().string());
        }
        EditorState::setProjectValue(m_project.directory(), "openScenes", names);
    }

    int Editor::activeSceneIndex() const
    {
        auto found = std::find(m_tabs.begin(), m_tabs.end(), m_scenePath);
        return found == m_tabs.end() ? -1 : (int)(found - m_tabs.begin());
    }

    bool Editor::isSceneModified(const std::string &path) const
    {
        if (path == m_scenePath)
        {
            return !m_commands->isClean();
        }
        auto found = m_parked.find(path);
        return found != m_parked.end() && !found->second.m_commands->isClean();
    }

    bool Editor::hasModifiedScenes() const
    {
        for (const std::string &path : m_tabs)
        {
            if (isSceneModified(path))
            {
                return true;
            }
        }
        return false;
    }

    bool Editor::closeScene(const std::string &path, std::string *error)
    {
        if (m_isPlaying)
        {
            *error = "Stop the game to close a scene.";
            return false;
        }
        auto found = std::find(m_tabs.begin(), m_tabs.end(), path);
        if (found == m_tabs.end())
        {
            return true;
        }
        if (m_tabs.size() == 1)
        {
            *error = "The last open scene stays open.";
            return false;
        }
        if (path == m_scenePath)
        {
            // The tab to the right takes over, or the one to the left when it was the last.
            auto neighbor = found + 1 != m_tabs.end() ? found + 1 : found - 1;
            activateParked(*neighbor);
        }
        m_parked.erase(path);
        m_tabs.erase(std::find(m_tabs.begin(), m_tabs.end(), path));
        rememberTabs();
        changed();
        return true;
    }

    void Editor::saveView()
    {
        // Before the player is ready the view is still the default one, which is not what was remembered.
        if (m_status != Status::Ready || m_scenePath.empty() || m_project.directory().empty())
        {
            return;
        }
        m_isViewDirty = false;
        Json views = EditorState::projectValue(m_project.directory(), "views");
        if (!views.is_object())
        {
            views = Json::object();
        }
        Json view = {{"mode", m_viewMode == ViewMode::ThreeD ? "3d" : "2d"},
                     {"x", m_viewport.cameraX()},
                     {"y", m_viewport.cameraY()},
                     {"zoom", m_viewport.zoom()},
                     {"target", {m_viewport3D.targetX(), m_viewport3D.targetY(), m_viewport3D.targetZ()}},
                     {"distance", m_viewport3D.distance()},
                     {"yaw", m_viewport3D.rotationY()},
                     {"pitch", m_viewport3D.rotationX()}};
        if (m_selected)
        {
            view["selected"] = *m_selected;
        }
        views[fs::path(m_scenePath).filename().string()] = view;
        EditorState::setProjectValue(m_project.directory(), "views", views);
    }

    void Editor::restoreView()
    {
        Json views = EditorState::projectValue(m_project.directory(), "views");
        std::string name = fs::path(m_scenePath).filename().string();
        if (!views.is_object() || !views.contains(name) || !views[name].is_object())
        {
            // A scene that was never opened starts from the default view, not from the last scene's.
            m_viewport.setCamera(s_frameWidth / 2.0, s_frameHeight / 2.0, 1.0);
            Viewport3D fresh;
            fresh.setFrameSize(m_viewport3D.frameWidth(), m_viewport3D.frameHeight());
            m_viewport3D = fresh;
            m_isViewDirty = false;
            return;
        }
        const Json &view = views[name];
        auto number = [&](const char *key, double fallback) { return view.contains(key) && view[key].is_number() ? view[key].get<double>() : fallback; };
        if (view.contains("mode") && view["mode"].is_string())
        {
            m_viewMode = view["mode"].get<std::string>() == "3d" ? ViewMode::ThreeD : ViewMode::TwoD;
        }
        m_viewport.setCamera(number("x", m_viewport.cameraX()), number("y", m_viewport.cameraY()), std::clamp(number("zoom", m_viewport.zoom()), Viewport::s_minZoom, Viewport::s_maxZoom));
        if (view.contains("target") && view["target"].is_array() && view["target"].size() == 3)
        {
            const Json &target = view["target"];
            m_viewport3D.set(target[0].get<double>(), target[1].get<double>(), target[2].get<double>(), number("distance", m_viewport3D.distance()), number("yaw", m_viewport3D.rotationY()),
                             number("pitch", m_viewport3D.rotationX()));
        }
        if (view.contains("selected") && view["selected"].is_string() && m_document.node(view["selected"].get<std::string>()))
        {
            m_selected = view["selected"].get<std::string>();
        }
        m_isViewDirty = false;
    }

    void Editor::chooseViewMode()
    {
        m_viewMode = ViewMode::TwoD;
        for (const std::string &id : m_document.nodeIds())
        {
            if (isClassOf(m_document.typeOf(id), "Node3D"))
            {
                m_viewMode = ViewMode::ThreeD;
                break;
            }
        }
        restoreView();
    }

    void Editor::setViewMode(ViewMode mode)
    {
        if (mode != m_viewMode)
        {
            m_viewMode = mode;
            m_gizmoMode = GizmoMode::Move;
            m_drag = Drag();
            cameraChanged();
        }
    }

    void Editor::fly(double forward, double right, double up, double seconds, bool fast)
    {
        if (m_viewMode != ViewMode::ThreeD)
        {
            return;
        }
        double speed = std::max(m_viewport3D.distance(), 2.0) * (fast ? 3.0 : 1.0);
        m_viewport3D.fly(forward * speed * seconds, right * speed * seconds, up * speed * seconds);
        cameraChanged();
    }

    void Editor::focusSelected()
    {
        if (!m_selected)
        {
            return;
        }
        if (m_viewMode == ViewMode::ThreeD)
        {
            if (const ObjectBounds3D *box = bounds3DOf(*m_selected))
            {
                m_viewport3D.focus(box->m_worldX, box->m_worldY, box->m_worldZ);
                cameraChanged();
            }
            return;
        }
        if (const ObjectBounds *box = boundsOf(*m_selected))
        {
            double zoom = m_viewport.zoom();
            if (box->m_width > 0 && box->m_height > 0)
            {
                zoom = std::min(zoom, std::min(m_viewport.frameWidth() * 0.8 / box->m_width, m_viewport.frameHeight() * 0.8 / box->m_height));
            }
            m_viewport.setCamera(box->m_x + box->m_width / 2, box->m_y + box->m_height / 2, std::max(zoom, Viewport::s_minZoom));
            cameraChanged();
        }
    }

    void Editor::resetView()
    {
        if (m_viewMode == ViewMode::ThreeD)
        {
            Viewport3D fresh;
            fresh.setFrameSize(m_viewport3D.frameWidth(), m_viewport3D.frameHeight());
            m_viewport3D = fresh;
        }
        else
        {
            m_viewport.setCamera(s_frameWidth / 2.0, s_frameHeight / 2.0, 1.0);
        }
        cameraChanged();
    }

    void Editor::zoomTo(double zoom)
    {
        if (m_viewMode == ViewMode::ThreeD || zoom <= 0)
        {
            return;
        }
        m_viewport.zoomAt(m_viewport.frameWidth() / 2.0, m_viewport.frameHeight() / 2.0, zoom / m_viewport.zoom());
        cameraChanged();
    }

    const ObjectBounds3D *Editor::bounds3DOf(const std::string &id) const
    {
        for (const ObjectBounds3D &box : m_frame.m_bounds3D)
        {
            if (box.m_id == id)
            {
                return &box;
            }
        }
        return nullptr;
    }

    std::optional<std::string> Editor::hit3D(double frameX, double frameY) const
    {
        const ObjectBounds3D *nearest = nullptr;
        for (const ObjectBounds3D &box : m_frame.m_bounds3D)
        {
            if (frameX >= box.m_x && frameX < box.m_x + box.m_width && frameY >= box.m_y && frameY < box.m_y + box.m_height
                && (!nearest || box.m_depth < nearest->m_depth) && !isLocked(box.m_id))
            {
                nearest = &box;
            }
        }
        return nearest ? std::optional<std::string>(nearest->m_id) : std::nullopt;
    }

    DrawList Editor::overlay3D() const
    {
        DrawList list = m_frameView3D.grid();
        if (m_selected)
        {
            if (const ObjectBounds3D *box = bounds3DOf(*m_selected))
            {
                list.push_back({DrawCommand::Kind::Rect, box->m_x, box->m_y, box->m_width, box->m_height, 0xffffc800});
            }
        }
        for (const Frustum &frustum : m_frame.m_frusta)
        {
            // The selected camera's view is drawn brighter than the others.
            unsigned int color = m_selected && *m_selected == frustum.m_id ? 0xffffc800 : 0xc0e0e0e0;
            const std::vector<double> &c = frustum.m_corners;
            auto edge = [&](int from, int to) {
                m_frameView3D.addLine(list, c[from * 3], c[from * 3 + 1], c[from * 3 + 2], c[to * 3], c[to * 3 + 1], c[to * 3 + 2], color);
            };
            for (int plane = 0; plane < 8; plane += 4)
            {
                for (int corner = 0; corner < 4; ++corner)
                {
                    edge(plane + corner, plane + (corner + 1) % 4);
                }
            }
            for (int corner = 0; corner < 4; ++corner)
            {
                edge(corner, 4 + corner);
            }
        }
        DrawList arrows = Gizmo3D::draw(handles3D());
        list.insert(list.end(), arrows.begin(), arrows.end());
        return list;
    }

    std::vector<AxisHandle> Editor::handles3D() const
    {
        if (m_selected && !isLocked(*m_selected))
        {
            if (const ObjectBounds3D *box = bounds3DOf(*m_selected))
            {
                return Gizmo3D::handles(*box);
            }
        }
        return {};
    }

    bool Editor::save(std::string *error)
    {
        if (m_isPlaying)
        {
            *error = "Stop the game to save the scene.";
            return false;
        }
        std::error_code ignored;
        fs::create_directories(fs::path(m_scenePath).parent_path(), ignored);
        if (!m_document.save(m_scenePath, error))
        {
            return false;
        }
        m_commands->markClean();
        m_needsLoad = m_needsLoad || m_tabs.size() > 1;
        changed();
        return true;
    }

    bool Editor::saveScene(const std::string &path, std::string *error)
    {
        if (path == m_scenePath)
        {
            return save(error);
        }
        auto found = m_parked.find(path);
        if (found == m_parked.end())
        {
            return true;
        }
        std::error_code ignored;
        fs::create_directories(fs::path(path).parent_path(), ignored);
        if (!found->second.m_document.save(path, error))
        {
            return false;
        }
        found->second.m_commands->markClean();
        m_needsLoad = true;
        changed();
        return true;
    }

    bool Editor::saveAll(std::string *error)
    {
        for (const std::string &path : m_tabs)
        {
            if (isSceneModified(path) && !saveScene(path, error))
            {
                return false;
            }
        }
        return true;
    }

    void Editor::setPlatform(Platform platform)
    {
        m_platform = platform;
        m_needsLoad = true;
        if (m_isPreviewing)
        {
            std::pair<int, int> display = m_project.logicalSize(m_platform);
            wantSize(display.first, display.second);
        }
        if (m_status == Status::Ready)
        {
            m_message = "Playing on " + platformLabel(m_platform);
        }
        changed();
    }

    void Editor::select(const std::optional<std::string> &id)
    {
        m_selected = id;
        m_isViewDirty = true;
        m_viewChangedAt = std::chrono::steady_clock::now();
        m_needsRender = true;
        changed();
    }

    const std::vector<HierarchyRow> &Editor::hierarchy() const
    {
        if (m_hierarchyVersion == m_document.version() && m_hierarchyVersion != 0)
        {
            return m_hierarchyRows;
        }
        m_hierarchyRows.clear();
        m_hierarchyRows.reserve(m_document.nodeCount());
        std::optional<std::string> camera = m_document.cameraId();
        for (const std::string &id : m_document.nodeIds())
        {
            const Json *node = m_document.node(id);
            std::string name = node && node->contains("name") && (*node)["name"].is_string() ? (*node)["name"].get<std::string>() : id;
            m_hierarchyRows.push_back({id, std::move(name), m_document.typeOf(id), camera == id, m_document.instanceOf(id), m_document.depthOf(id), m_document.childCount(id) > 0});
        }
        m_hierarchyVersion = m_document.version();
        return m_hierarchyRows;
    }

    bool Editor::isClassOf(const std::string &type, const std::string &ancestor) const
    {
        for (const ClassSchema *schema : m_session.classChain(type))
        {
            if (schema->m_name == ancestor)
            {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> Editor::classChain(const std::string &type) const
    {
        std::vector<std::string> names;
        for (const ClassSchema *schema : m_session.classChain(type))
        {
            names.push_back(schema->m_name);
        }
        return names;
    }

    bool Editor::isControl(const std::string &id) const
    {
        return isClassOf(m_document.typeOf(id), "Control");
    }

    const ObjectBounds *Editor::boundsOf(const std::string &id) const
    {
        for (const ObjectBounds &box : m_frame.m_bounds)
        {
            if (box.m_id == id)
            {
                return &box;
            }
        }
        return nullptr;
    }

    // A new node holds what its class says by default, except what every node has, which the file leaves out.
    Json Editor::newNode(const std::string &type) const
    {
        Json node = {{"type", type}};
        for (const ClassSchema *schema : m_session.classChain(type))
        {
            if (schema->m_name == "Node" || schema->m_name == "CanvasItem")
            {
                continue;
            }
            for (const FieldSchema &field : schema->m_fields)
            {
                if (!field.m_default.is_null())
                {
                    node[field.m_name] = field.m_default;
                }
            }
        }
        return node;
    }

    void Editor::addNode(const std::string &type, const std::optional<std::string> &parentId)
    {
        std::string base = type;
        std::transform(base.begin(), base.end(), base.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        std::string id = m_document.uniqueId(base);

        Json node = newNode(type);
        node["id"] = id;
        node["name"] = type;
        if (isClassOf(type, "Node2D"))
        {
            node["x"] = std::round(m_viewport.cameraX());
            node["y"] = std::round(m_viewport.cameraY());
        }
        else if (isClassOf(type, "Node3D"))
        {
            node["x"] = std::round(m_viewport3D.targetX() * 100) / 100;
            node["y"] = std::round(m_viewport3D.targetY() * 100) / 100;
            node["z"] = std::round(m_viewport3D.targetZ() * 100) / 100;
        }
        std::string parent = parentId ? *parentId : m_document.rootId();
        if (m_commands->execute(std::make_unique<AddNode>(parent, m_document.childCount(parent), node)))
        {
            select(id);
        }
    }

    namespace
    {
        constexpr const char *s_clipboardPrefix = "fried-nodes:";

        // The text without the number at its end, and without the separator before it when there is one: "post-3" with a dash is "post", and "Enemy2" with none is "Enemy".
        std::string withoutNumber(const std::string &text, char separator)
        {
            std::size_t end = text.size();
            while (end > 0 && std::isdigit((unsigned char)text[end - 1]))
            {
                --end;
            }
            if (end == text.size() || end == 0)
            {
                return text;
            }
            if (separator != 0)
            {
                if (text[end - 1] != separator || end == 1)
                {
                    return text;
                }
                --end;
            }
            return text.substr(0, end);
        }

        void renumber(Json *node, const SceneDocument &document, std::set<std::string> *taken)
        {
            std::string base = withoutNumber(node->value("id", std::string("node")), '-');
            std::string id = base;
            for (int number = 2; document.contains(id) || taken->count(id); ++number)
            {
                id = base + "-" + std::to_string(number);
            }
            taken->insert(id);
            (*node)["id"] = id;
            if (node->contains("children") && (*node)["children"].is_array())
            {
                for (Json &child : (*node)["children"])
                {
                    renumber(&child, document, taken);
                }
            }
        }
    }

    bool Editor::insertCopy(const Json &source, const std::string &parentId, int index, const std::string &label)
    {
        Json copy = source;
        std::set<std::string> taken;
        renumber(&copy, m_document, &taken);

        // The name of the copy is not one of its siblings': "Enemy" gives "Enemy2", and "Enemy2" gives "Enemy3".
        std::set<std::string> siblings;
        for (const std::string &sibling : m_document.childrenOf(parentId))
        {
            Json name = m_document.nodeField(sibling, "name").value_or(Json());
            siblings.insert(name.is_string() ? name.get<std::string>() : sibling);
        }
        if (copy.contains("name") && copy["name"].is_string() && siblings.count(copy["name"].get<std::string>()))
        {
            std::string base = withoutNumber(copy["name"].get<std::string>(), 0);
            std::string name = base;
            for (int number = 2; siblings.count(name); ++number)
            {
                name = base + std::to_string(number);
            }
            copy["name"] = name;
        }

        std::string id = copy.value("id", "");
        if (!m_commands->execute(std::make_unique<AddNode>(parentId, index, copy, label)))
        {
            return false;
        }
        select(id);
        return true;
    }

    bool Editor::duplicateSelected()
    {
        if (!m_selected || !m_document.node(*m_selected))
        {
            return false;
        }
        if (*m_selected == m_document.rootId())
        {
            m_message = "The root node cannot be duplicated, a scene has only one.";
            changed();
            return false;
        }
        return insertCopy(*m_document.node(*m_selected), m_document.parentOf(*m_selected), m_document.indexOf(*m_selected) + 1, "Duplicate node");
    }

    std::string Editor::copySelected()
    {
        if (!m_selected || !m_document.node(*m_selected))
        {
            return std::string();
        }
        m_clipboard = *m_document.node(*m_selected);
        return s_clipboardPrefix + m_clipboard.dump();
    }

    bool Editor::paste(const std::string &clipboardText)
    {
        Json source = m_clipboard;
        std::string prefix = s_clipboardPrefix;
        if (clipboardText.compare(0, prefix.size(), prefix) == 0)
        {
            Json parsed = Json::parse(clipboardText.substr(prefix.size()), nullptr, false);
            if (parsed.is_object() && parsed.contains("type") && parsed["type"].is_string())
            {
                source = parsed;
            }
        }
        if (!source.is_object())
        {
            return false;
        }
        std::string parent = m_selected ? m_document.parentOf(*m_selected) : std::string();
        int index = m_selected ? m_document.indexOf(*m_selected) + 1 : 0;
        if (parent.empty())
        {
            // Beside the root there is no place, so a copy goes inside it.
            parent = m_document.rootId();
            index = m_document.childCount(parent);
        }
        return insertCopy(source, parent, index, "Paste node");
    }

    namespace
    {
        std::string sceneNameOf(const std::string &path)
        {
            std::string name = fs::path(path).filename().string();
            const std::string suffix = ".scene.json";
            return name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0 ? name.substr(0, name.size() - suffix.size()) : name;
        }

        void collectInstances(const Json &node, std::vector<std::string> *names)
        {
            if (node.contains("instance") && node["instance"].is_string())
            {
                names->push_back(node["instance"].get<std::string>());
            }
            if (node.contains("children") && node["children"].is_array())
            {
                for (const Json &child : node["children"])
                {
                    collectInstances(child, names);
                }
            }
        }
    }

    void Editor::bind(SceneDocument &document)
    {
        document.m_instanceType = [this](const std::string &name) {
            std::optional<Json> root = sceneRoot(name);
            return root && root->contains("type") && (*root)["type"].is_string() ? (*root)["type"].get<std::string>() : std::string();
        };
    }

    std::string Editor::scenePathOf(const std::string &name) const
    {
        return m_project.scenesDirectory() + "/" + name + ".scene.json";
    }

    // The only top-level node of a scene, as the scene being edited or another open one has it now and as the file has it otherwise. Nothing when there is no such scene or it has not exactly one.
    std::optional<Json> Editor::sceneRoot(const std::string &sceneName) const
    {
        std::string path = scenePathOf(sceneName);
        const Json *data = nullptr;
        if (path == m_scenePath)
        {
            data = &m_document.data();
        }
        else if (auto parked = m_parked.find(path); parked != m_parked.end())
        {
            data = &parked->second.m_document.data();
        }
        else
        {
            std::error_code ignored;
            auto written = fs::last_write_time(path, ignored);
            if (ignored)
            {
                return std::nullopt;
            }
            auto cached = m_sceneFiles.find(path);
            if (cached == m_sceneFiles.end() || cached->second.first != written)
            {
                std::ifstream file(path);
                m_sceneFiles[path] = std::make_pair(written, file ? Json::parse(file, nullptr, false) : Json());
                cached = m_sceneFiles.find(path);
            }
            data = &cached->second.second;
        }
        if (!data->is_object() || !data->contains("nodes") || !(*data)["nodes"].is_array() || (*data)["nodes"].size() != 1 || !(*data)["nodes"][0].is_object())
        {
            return std::nullopt;
        }
        return (*data)["nodes"][0];
    }

    // Whether the scene, or a scene it instances at any depth, instances the target.
    bool Editor::reaches(const std::string &from, const std::string &target, int depth) const
    {
        std::optional<Json> root = sceneRoot(from);
        if (!root || depth > 16)
        {
            return false;
        }
        std::vector<std::string> names;
        collectInstances(*root, &names);
        for (const std::string &name : names)
        {
            if (name == target || reaches(name, target, depth + 1))
            {
                return true;
            }
        }
        return false;
    }

    std::string Editor::instanceProblem(const std::string &sceneName) const
    {
        std::error_code ignored;
        std::string path = scenePathOf(sceneName);
        if (path != m_scenePath && m_parked.find(path) == m_parked.end() && !fs::exists(path, ignored))
        {
            return "The project has no scene named " + sceneName + ".";
        }
        std::optional<Json> root = sceneRoot(sceneName);
        if (!root)
        {
            return "The scene " + sceneName + " has more or fewer than one top-level node, and only a scene with exactly one can be instanced.";
        }
        if (root->contains("instance") && !root->contains("type"))
        {
            return "The root of " + sceneName + " is itself an instance, which is not supported.";
        }
        std::string current = sceneNameOf(m_scenePath);
        if (sceneName == current || reaches(sceneName, current, 0))
        {
            return "Instancing " + sceneName + " here would make " + current + " contain itself.";
        }
        return "";
    }

    bool Editor::addInstance(const std::string &scenePath, const std::optional<std::string> &parentId, std::string *error)
    {
        std::string name = sceneNameOf(scenePath);
        std::string problem = instanceProblem(name);
        if (!problem.empty())
        {
            *error = problem;
            return false;
        }
        Json root = *sceneRoot(name);
        std::string id = m_document.uniqueId(root.value("id", name));
        Json node = {{"id", id}, {"instance", name}};
        std::string type = root.value("type", "");
        if (isClassOf(type, "Node2D"))
        {
            node["x"] = std::round(m_viewport.cameraX());
            node["y"] = std::round(m_viewport.cameraY());
        }
        else if (isClassOf(type, "Node3D"))
        {
            node["x"] = std::round(m_viewport3D.targetX() * 100) / 100;
            node["y"] = std::round(m_viewport3D.targetY() * 100) / 100;
            node["z"] = std::round(m_viewport3D.targetZ() * 100) / 100;
        }
        std::string parent = parentId ? *parentId : m_document.rootId();
        if (!m_commands->execute(std::make_unique<AddNode>(parent, m_document.childCount(parent), node, "Instance scene")))
        {
            *error = "The scene could not be added.";
            return false;
        }
        select(id);
        return true;
    }

    std::vector<SceneIssue> Editor::sceneIssues() const
    {
        std::vector<SceneIssue> issues;
        for (const std::string &id : m_document.nodeIds())
        {
            std::string scene = m_document.instanceOf(id);
            std::string problem = scene.empty() ? std::string() : instanceProblem(scene);
            if (!problem.empty())
            {
                issues.push_back({id, scene, problem});
            }
        }
        return issues;
    }

    std::vector<std::string> Editor::inputIssues() const
    {
        if (m_session.inputNames().isEmpty())
        {
            return {};
        }
        return checkInputBlock(m_project.inputBlock(), m_session.inputNames());
    }

    void Editor::chooseInstanceScene(const std::string &id, const std::string &sceneName)
    {
        m_commands->execute(std::make_unique<SetNodeField>(id, "instance", Json(sceneName)));
    }

    void Editor::removeSelected()
    {
        if (m_selected && *m_selected == m_document.rootId())
        {
            m_message = "The root node cannot be deleted, every scene has one.";
            changed();
            return;
        }
        if (m_selected && m_commands->execute(std::make_unique<RemoveNode>(*m_selected)))
        {
            select(std::nullopt);
        }
    }

    void Editor::moveSelected(int delta)
    {
        if (m_selected && *m_selected != m_document.rootId())
        {
            int target = m_document.indexOf(*m_selected) + delta;
            std::string parent = m_document.parentOf(*m_selected);
            if (target >= 0 && target < m_document.childCount(parent))
            {
                m_commands->execute(std::make_unique<MoveNode>(*m_selected, parent, target));
            }
        }
    }

    void Editor::reparent(const std::string &id, const std::string &parentId, int index)
    {
        if (parentId.empty() || id == m_document.rootId())
        {
            // The root has no sibling to be placed beside, and nothing but the root is at the top.
            return;
        }
        // The place is counted among the siblings as they are now, so a node moved later in its own list leaves a gap behind it.
        if (m_document.parentOf(id) == parentId && m_document.indexOf(id) >= 0 && m_document.indexOf(id) < index)
        {
            --index;
        }
        m_commands->execute(std::make_unique<MoveNode>(id, parentId, index));
    }

    void Editor::changeType(const std::string &id, const std::string &type)
    {
        std::set<std::string> kept;
        for (const ClassSchema *schema : m_session.classChain(type))
        {
            for (const FieldSchema &field : schema->m_fields)
            {
                kept.insert(field.m_name);
            }
        }
        m_commands->execute(std::make_unique<ChangeNodeType>(id, type, kept));
    }

    void Editor::setCameraToSelected()
    {
        if (m_selected)
        {
            m_commands->execute(std::make_unique<SetCamera>(m_selected));
        }
    }

    void Editor::rename(const std::string &id, const std::string &name)
    {
        m_commands->execute(std::make_unique<SetNodeField>(id, "name", Json(name)));
    }

    void Editor::renameSelected(const std::string &name)
    {
        if (m_selected)
        {
            rename(*m_selected, name);
        }
    }

    const std::vector<ClassView> &Editor::inspector() const
    {
        if (m_inspectorVersion == m_document.version() && m_inspectorVersion != 0 && m_inspectorSelected == m_selected
            && m_inspectorSchemaSize == m_session.schema().size() && m_inspectorLiveSerial == m_liveSerial)
        {
            return m_inspectorViews;
        }
        std::vector<ClassView> &views = m_inspectorViews;
        views.clear();
        m_inspectorVersion = m_document.version();
        m_inspectorSelected = m_selected;
        m_inspectorSchemaSize = m_session.schema().size();
        m_inspectorLiveSerial = m_liveSerial;
        const Json *live = nullptr;
        if (m_isPlaying && m_selected)
        {
            auto found = m_liveFields.find(*m_selected);
            live = found != m_liveFields.end() ? &found->second : nullptr;
        }
        const Json *node = m_selected ? m_document.node(*m_selected) : nullptr;
        if (!node)
        {
            return views;
        }
        std::string instanced = m_document.instanceOf(*m_selected);
        std::optional<Json> base = instanced.empty() ? std::nullopt : sceneRoot(instanced);
        std::vector<const ClassSchema *> chain = m_session.classChain(m_document.typeOf(*m_selected));
        for (std::size_t position = 0; position < chain.size(); ++position)
        {
            const ClassSchema *schema = chain[position];
            ClassView view;
            view.m_class = schema->m_name;
            // The name belongs to every node, so it is the first field of the section of the class every node is.
            if (position + 1 == chain.size())
            {
                FieldView name;
                name.m_schema = {"name", "string", {}, Json(*m_selected)};
                name.m_isSet = node->contains("name");
                name.m_value = name.m_isSet ? (*node)["name"] : base && base->contains("name") ? (*base)["name"] : Json(*m_selected);
                view.m_fields.push_back(std::move(name));
            }
            for (const FieldSchema &field : schema->m_fields)
            {
                FieldView fieldView;
                fieldView.m_schema = field;
                fieldView.m_isSet = node->contains(field.m_name);
                fieldView.m_value = fieldView.m_isSet ? (*node)[field.m_name] : base && base->contains(field.m_name) ? (*base)[field.m_name] : field.m_default;
                if (live && live->contains(field.m_name))
                {
                    fieldView.m_value = (*live)[field.m_name];
                }
                view.m_fields.push_back(std::move(fieldView));
            }
            views.push_back(std::move(view));
        }
        return views;
    }

    void Editor::setField(const std::string &field, const std::optional<Json> &value)
    {
        if (m_selected)
        {
            m_commands->execute(std::make_unique<SetNodeField>(*m_selected, field, value), true);
        }
    }

    void Editor::revertFields(const std::vector<std::string> &fields)
    {
        if (m_selected && !fields.empty())
        {
            m_commands->execute(std::make_unique<ClearNodeFields>(*m_selected, fields));
        }
    }

    void Editor::setTransformFields(const std::string &id, const std::map<std::string, double> &fields, bool merge)
    {
        m_commands->execute(std::make_unique<SetNodeFields>(id, fields), merge);
    }

    const std::vector<AssetEntry> &Editor::assets() const
    {
        auto now = std::chrono::steady_clock::now();
        if (m_assetsAt == std::chrono::steady_clock::time_point() || now - m_assetsAt > std::chrono::seconds(2))
        {
            m_assetsCache = listAssets("game", m_project.assetsDirectory());
            std::vector<AssetEntry> engine = listAssets("engine", m_enginePath + "/assets");
            m_assetsCache.insert(m_assetsCache.end(), engine.begin(), engine.end());
            m_directoriesCache = listAssetDirectories("game", m_project.assetsDirectory());
            std::vector<std::string> engineFolders = listAssetDirectories("engine", m_enginePath + "/assets");
            m_directoriesCache.insert(m_directoriesCache.end(), engineFolders.begin(), engineFolders.end());
            std::vector<AssetEntry> sources = listAssets("src", m_project.directory() + "/src");
            m_assetsCache.insert(m_assetsCache.end(), sources.begin(), sources.end());
            std::vector<std::string> sourceFolders = listAssetDirectories("src", m_project.directory() + "/src");
            m_directoriesCache.insert(m_directoriesCache.end(), sourceFolders.begin(), sourceFolders.end());
            m_assetsAt = now;
        }
        return m_assetsCache;
    }

    const std::vector<std::string> &Editor::assetDirectories() const
    {
        assets();
        return m_directoriesCache;
    }

    bool Editor::ownedPath(const std::string &reference, std::string *directory, std::string *path, std::string *error) const
    {
        if (reference == "game" || reference == "src")
        {
            *directory = reference == "game" ? m_project.assetsDirectory() : m_project.directory() + "/src";
            path->clear();
            return true;
        }
        std::string root;
        if (!parseAssetReference(reference, &root, path) || (root != "game" && root != "src"))
        {
            *error = "Only the game's own files can be changed, the engine's are read only.";
            return false;
        }
        *directory = root == "game" ? m_project.assetsDirectory() : m_project.directory() + "/src";
        return true;
    }

    std::vector<std::string> Editor::assetUsers(const std::string &reference) const
    {
        std::string directory, path, ignored;
        return ownedPath(reference, &directory, &path, &ignored) && directory == m_project.assetsDirectory() && !path.empty() ? scenesUsingAsset(m_project.scenesDirectory(), path) : std::vector<std::string>();
    }

    bool Editor::createAssetFolder(const std::string &folder, const std::string &name, std::string *error)
    {
        std::string directory, parent;
        if (!ownedPath(folder, &directory, &parent, error))
        {
            return false;
        }
        if (!isAssetName(name))
        {
            *error = "A folder needs a name that is a file name, with no folders in it.";
            return false;
        }
        if (!fried::editor::createAssetFolder(directory, parent.empty() ? name : parent + "/" + name, error))
        {
            return false;
        }
        m_assetsAt = std::chrono::steady_clock::time_point();
        changed();
        return true;
    }

    bool Editor::moveAsset(const std::string &reference, const std::string &folder, const std::string &name, std::string *error)
    {
        std::string directory, destinationDirectory, from, destination;
        if (!ownedPath(reference, &directory, &from, error) || !ownedPath(folder, &destinationDirectory, &destination, error))
        {
            return false;
        }
        if (directory != destinationDirectory)
        {
            *error = "A file stays in the assets or in the source, it cannot go from one to the other.";
            return false;
        }
        const bool isSource = directory != m_project.assetsDirectory();
        std::string newName = name.empty() ? fs::path(from).filename().string() : name;
        if (!isAssetName(newName))
        {
            *error = "A name is a file name, with no folders in it.";
            return false;
        }
        std::string to = destination.empty() ? newName : destination + "/" + newName;
        if (from.empty() || to == from)
        {
            return true;
        }
        if (!isSource && (isSceneAssetPath(from) || isSceneAssetPath(to)))
        {
            *error = "Scenes keep their place in the scenes folder, where the game finds them by name.";
            return false;
        }
        const fs::path fromPath(from);
        const fs::path toPath(to);
        const bool isScript = isSource && fromPath.extension() == ".hx";
        if (isScript && (toPath.extension() != ".hx" || !isScriptName(toPath.stem().string())))
        {
            *error = "A script is named like a class: it starts with a capital letter, has only letters, digits and underscores, and ends in .hx.";
            return false;
        }
        if (!saveAll(error))
        {
            return false;
        }
        int rewritten = 0;
        if (!fried::editor::moveAsset(directory, m_project.scenesDirectory(), from, to, &rewritten, error))
        {
            return false;
        }
        if (isSource)
        {
            // Where a script is says its package, and its name says its class, which the scenes use as the type of a node.
            std::error_code ignored;
            std::vector<fs::path> moved;
            if (fs::is_directory(fs::path(directory) / to, ignored))
            {
                for (fs::recursive_directory_iterator it(fs::path(directory) / to, ignored), end; it != end; it.increment(ignored))
                {
                    if (it->is_regular_file(ignored) && it->path().extension() == ".hx")
                    {
                        moved.push_back(it->path());
                    }
                }
            }
            else if (isScript)
            {
                moved.push_back(fs::path(directory) / to);
            }
            for (const fs::path &file : moved)
            {
                std::string package = packageOfFolder(fs::relative(file.parent_path(), directory, ignored).generic_string());
                package = package == "." ? std::string() : package;
                std::string oldName = isScript ? fromPath.stem().string() : file.stem().string();
                fixScript(file.string(), package, oldName, file.stem().string());
            }
            if (isScript && fromPath.stem() != toPath.stem())
            {
                rewritten += renameSceneType(m_project.scenesDirectory(), fromPath.stem().string(), toPath.stem().string());
            }
        }
        m_assetsAt = std::chrono::steady_clock::time_point();
        reloadScenes();
        m_message = rewritten == 0 ? "Moved " + from + " to " + to
                                   : "Moved " + from + " to " + to + ", and " + std::to_string(rewritten) + (rewritten == 1 ? " reference" : " references") + " in the scenes follow it";
        changed();
        return true;
    }

    bool Editor::deleteAsset(const std::string &reference, std::string *error)
    {
        std::string directory, path;
        if (!ownedPath(reference, &directory, &path, error))
        {
            return false;
        }
        if (!fried::editor::deleteAsset(directory, path, error))
        {
            return false;
        }
        m_assetsAt = std::chrono::steady_clock::time_point();
        m_message = "Deleted " + reference;
        changed();
        return true;
    }

    bool Editor::duplicateAsset(const std::string &reference, std::string *error)
    {
        std::string directory, path;
        if (!ownedPath(reference, &directory, &path, error) || path.empty())
        {
            return false;
        }
        const bool isSource = directory != m_project.assetsDirectory();
        if (!isSource && isSceneAssetPath(path))
        {
            *error = "Scenes keep their place in the scenes folder, where the game finds them by name.";
            return false;
        }
        fs::path source(path);
        std::string stem = source.stem().string();
        std::string extension = source.extension().string();
        std::string to;
        const bool isScript = isSource && extension == ".hx";
        for (int number = 1;; ++number)
        {
            // A copy of a script is a class, so its name has no spaces.
            std::string name = isScript ? stem + "Copy" + (number > 1 ? std::to_string(number) : std::string()) + extension
                                        : stem + " copy" + (number > 1 ? " " + std::to_string(number) : std::string()) + extension;
            to = (source.parent_path() / name).generic_string();
            std::error_code ignored;
            if (!fs::exists(fs::path(directory) / to, ignored))
            {
                break;
            }
        }
        if (!fried::editor::copyAsset(directory, path, to, error))
        {
            return false;
        }
        if (isScript)
        {
            fixScript((fs::path(directory) / to).string(), packageOfFolder(fs::path(to).parent_path().generic_string()), stem, fs::path(to).stem().string());
        }
        m_assetsAt = std::chrono::steady_clock::time_point();
        changed();
        return true;
    }

    bool Editor::createScript(const std::string &folder, const std::string &name, const std::string &baseClass, std::string *error)
    {
        std::string directory, path;
        if (!ownedPath(folder, &directory, &path, error))
        {
            return false;
        }
        if (directory == m_project.assetsDirectory())
        {
            *error = "Scripts go in the game's source, not in its assets.";
            return false;
        }
        std::string base = baseClass == "Node" || baseClass.empty() ? std::string() : findClassModule({directory, m_enginePath + "/src"}, baseClass);
        if (base.empty() && baseClass != "Node" && !baseClass.empty())
        {
            *error = "The file of " + baseClass + " is not in the source of the game or the engine.";
            return false;
        }
        if (!fried::editor::createScript(directory, path, name, base, error))
        {
            return false;
        }
        m_assetsAt = std::chrono::steady_clock::time_point();
        m_message = "Created " + folder + "/" + name + ".hx. Build the player again for the editor to know the class.";
        changed();
        return true;
    }

    void Editor::reloadScenes()
    {
        std::string error;
        for (auto &parked : m_parked)
        {
            SceneDocument document;
            if (document.load(parked.first, &error))
            {
                bind(document);
                parked.second.m_document = std::move(document);
                parked.second.m_commands = std::make_unique<CommandStack>(m_document);
            }
        }
        if (m_document.load(m_scenePath, &error))
        {
            bind(m_document);
            m_commands = std::make_unique<CommandStack>(m_document);
            m_commands->m_onChange = [this] { changed(); };
            loadLocked();
            m_selected.reset();
            m_needsLoad = true;
        }
    }

    std::string Editor::assetFile(const std::string &root, const std::string &path) const
    {
        return (root == "engine" ? m_enginePath + "/assets/" : root == "src" ? m_project.directory() + "/src/" : m_project.assetsDirectory() + "/") + path;
    }

    void Editor::dropAsset(const std::string &root, const std::string &path, double frameX, double frameY)
    {
        if (path.size() > 11 && path.compare(path.size() - 11, 11, ".scene.json") == 0)
        {
            std::string error;
            if (!addInstance(assetFile(root, path), std::nullopt, &error))
            {
                m_message = error;
                changed();
            }
            return;
        }
        AssetKind kind = assetKindOf(path);
        if (kind != AssetKind::Image && kind != AssetKind::Font)
        {
            return;
        }

        Json reference = {{"root", root}, {"path", path}};
        if (kind == AssetKind::Image)
        {
            if (auto target = Viewport::hit(m_viewport, m_frame.m_bounds, frameX, frameY))
            {
                bool hasTexture = false;
                for (const ClassSchema *schema : m_session.classChain(m_document.typeOf(*target)))
                {
                    for (const FieldSchema &field : schema->m_fields)
                    {
                        hasTexture = hasTexture || field.m_name == "texture";
                    }
                }
                if (hasTexture)
                {
                    select(target);
                    m_commands->execute(std::make_unique<SetNodeField>(*target, "texture", reference));
                    return;
                }
            }
        }
        else
        {
            reference["size"] = 16;
        }

        std::string stem = fs::path(path).stem().stem().string();
        std::string id = m_document.uniqueId(stem.empty() ? "asset" : stem);
        double worldX = std::round(m_viewport.frameToWorldX(frameX));
        double worldY = std::round(m_viewport.frameToWorldY(frameY));
        Json node = newNode(kind == AssetKind::Image ? "Sprite" : "Label");
        node["id"] = id;
        node["name"] = stem;
        node[kind == AssetKind::Image ? "texture" : "font"] = reference;
        if (kind == AssetKind::Image)
        {
            node["x"] = worldX;
            node["y"] = worldY;
        }
        else
        {
            // A label sits at its offset from the top left of its parent, which for a node at the top of the scene is the world's origin.
            node["offsetX"] = worldX;
            node["offsetY"] = worldY;
            node["text"] = "Text";
            node["color"] = {{"r", 255}, {"g", 255}, {"b", 255}, {"a", 255}};
        }
        std::string top = m_document.rootId();
        if (m_commands->execute(std::make_unique<AddNode>(top, m_document.childCount(top), node)))
        {
            select(id);
        }
    }

    void Editor::addAssetNode(const std::string &root, const std::string &path, const std::string &parentId)
    {
        if (path.size() > 11 && path.compare(path.size() - 11, 11, ".scene.json") == 0)
        {
            std::string error;
            if (!addInstance(assetFile(root, path), parentId.empty() ? std::nullopt : std::optional<std::string>(parentId), &error))
            {
                m_message = error;
                changed();
            }
            return;
        }
        AssetKind kind = assetKindOf(path);
        if (kind != AssetKind::Image && kind != AssetKind::Font)
        {
            return;
        }

        Json reference = {{"root", root}, {"path", path}};
        std::string stem = fs::path(path).stem().stem().string();
        std::string id = m_document.uniqueId(stem.empty() ? "asset" : stem);
        Json node = newNode(kind == AssetKind::Image ? "Sprite" : "Label");
        node["id"] = id;
        node["name"] = stem;
        if (kind == AssetKind::Image)
        {
            node["texture"] = reference;
        }
        else
        {
            reference["size"] = 16;
            node["font"] = reference;
            node["text"] = "Text";
            node["color"] = {{"r", 255}, {"g", 255}, {"b", 255}, {"a", 255}};
        }
        std::string parent = parentId.empty() ? m_document.rootId() : parentId;
        if (m_commands->execute(std::make_unique<AddNode>(parent, m_document.childCount(parent), node)))
        {
            select(id);
        }
    }

    std::vector<AssetIssue> Editor::issues() const
    {
        return validateAssets(m_platform, m_project.directory());
    }

    bool Editor::fixIssue(const AssetIssue &issue, std::string *error)
    {
        if (!applyFix(issue, m_project.directory(), m_project.directory() + "/.fried-editor/originals", error))
        {
            return false;
        }
        m_message = "Fixed " + issue.m_asset + ", the original is kept in .fried-editor/originals";
        m_needsLoad = true;
        changed();
        return true;
    }

    void Editor::setGizmoMode(GizmoMode mode)
    {
        m_gizmoMode = mode;
        changed();
    }

    TransformValues Editor::transformOf(const std::string &id) const
    {
        auto number = [&](const char *field, double fallback) {
            std::optional<Json> value = m_document.nodeField(id, field);
            return value && value->is_number() ? value->get<double>() : fallback;
        };
        TransformValues values = {number("x", 0), number("y", 0), number("rotation", 0), number("scaleX", 1), number("scaleY", 1)};
        if (isControl(id))
        {
            // Where the layout put it, which the last frame shows.
            if (const ObjectBounds *box = boundsOf(id))
            {
                values.m_x = box->m_x;
                values.m_y = box->m_y;
            }
        }
        return values;
    }

    // The anchors of the selected Control, as fractions of its parent's rectangle: a marker at each corner of the area they span, and that area itself when the control stretches.
    DrawList Editor::anchorOverlay() const
    {
        DrawList list;
        std::optional<RectPreview> preview = rectPreview();
        if (!preview)
        {
            return list;
        }
        constexpr unsigned int anchorColor = 0xff40c0ff;
        double left = m_frameView.worldToFrameX(preview->m_originX + preview->m_viewWidth * preview->m_anchorMinX);
        double right = m_frameView.worldToFrameX(preview->m_originX + preview->m_viewWidth * preview->m_anchorMaxX);
        double top = m_frameView.worldToFrameY(preview->m_originY + preview->m_viewHeight * preview->m_anchorMinY);
        double bottom = m_frameView.worldToFrameY(preview->m_originY + preview->m_viewHeight * preview->m_anchorMaxY);
        if (left != right || top != bottom)
        {
            list.push_back({DrawCommand::Kind::Rect, left, top, right - left, bottom - top, anchorColor});
        }
        constexpr double marker = 5;
        for (double x : {left, right})
        {
            for (double y : {top, bottom})
            {
                list.push_back({DrawCommand::Kind::FilledRect, x - marker, y - marker, marker * 2, marker * 2, anchorColor});
            }
        }
        return list;
    }

    std::optional<RectPreview> Editor::rectPreview() const
    {
        if (!m_selected || !isControl(*m_selected))
        {
            return std::nullopt;
        }
        const ObjectBounds *box = boundsOf(*m_selected);
        if (!box)
        {
            return std::nullopt;
        }
        auto number = [&](const char *field) {
            std::optional<Json> value = m_document.nodeField(*m_selected, field);
            return value && value->is_number() ? value->get<double>() : 0.0;
        };
        auto axisOf = [](double minimum, double maximum) {
            if (minimum != maximum)
            {
                return minimum == 0 && maximum == 1 ? AnchorAxis::Stretch : AnchorAxis::Custom;
            }
            return minimum == 0 ? AnchorAxis::Start : minimum == 0.5 ? AnchorAxis::Center : minimum == 1 ? AnchorAxis::End : AnchorAxis::Custom;
        };
        RectPreview preview;
        // The rectangle the anchors refer to is the parent's: a control's, a point for any other node, or the screen for a node at the top.
        if (box->m_parent)
        {
            preview.m_originX = box->m_parent->m_x;
            preview.m_originY = box->m_parent->m_y;
            preview.m_viewWidth = box->m_parent->m_width;
            preview.m_viewHeight = box->m_parent->m_height;
        }
        else
        {
            std::pair<int, int> logical = m_project.logicalSize(m_platform);
            preview.m_viewWidth = logical.first;
            preview.m_viewHeight = logical.second;
        }
        preview.m_x = box->m_x;
        preview.m_y = box->m_y;
        preview.m_width = box->m_width;
        preview.m_height = box->m_height;
        preview.m_anchorMinX = number("anchorMinX");
        preview.m_anchorMinY = number("anchorMinY");
        preview.m_anchorMaxX = number("anchorMaxX");
        preview.m_anchorMaxY = number("anchorMaxY");
        preview.m_pivotX = number("pivotX");
        preview.m_pivotY = number("pivotY");
        preview.m_horizontal = axisOf(preview.m_anchorMinX, preview.m_anchorMaxX);
        preview.m_vertical = axisOf(preview.m_anchorMinY, preview.m_anchorMaxY);
        std::optional<Json> below = m_document.nodeField(*m_selected, "below");
        preview.m_isBelow = below && below->is_string() && !below->get<std::string>().empty();
        return preview;
    }

    namespace
    {
        // The fields of one axis that keep a box of the given start and size where it is, for the anchors and pivot the axis is given.
        void keepAxis(std::map<std::string, double> &fields, bool isHorizontal, double anchorMin, double anchorMax, double pivot, double start, double size,
                      double origin, double view)
        {
            start -= origin;
            const char *minimumName = isHorizontal ? "anchorMinX" : "anchorMinY";
            const char *maximumName = isHorizontal ? "anchorMaxX" : "anchorMaxY";
            fields[minimumName] = anchorMin;
            fields[maximumName] = anchorMax;
            fields[isHorizontal ? "pivotX" : "pivotY"] = pivot;
            if (anchorMin != anchorMax)
            {
                fields[isHorizontal ? "insetLeft" : "insetTop"] = std::round(start - view * anchorMin);
                fields[isHorizontal ? "insetRight" : "insetBottom"] = std::round(view * anchorMax - (start + size));
            }
            else
            {
                fields[isHorizontal ? "offsetX" : "offsetY"] = std::round(start - view * anchorMin + size * pivot);
            }
        }

        double anchorOf(AnchorAxis axis, bool isMaximum)
        {
            switch (axis)
            {
            case AnchorAxis::Start:
                return 0;
            case AnchorAxis::Center:
                return 0.5;
            case AnchorAxis::End:
                return 1;
            default:
                return isMaximum ? 1 : 0;
            }
        }
    }

    void Editor::setAnchorPreset(AnchorAxis horizontal, AnchorAxis vertical)
    {
        std::optional<RectPreview> preview = rectPreview();
        if (!preview)
        {
            return;
        }
        std::map<std::string, double> fields;
        if (horizontal != AnchorAxis::Custom)
        {
            double pivot = horizontal == AnchorAxis::Stretch ? 0.5 : anchorOf(horizontal, false);
            keepAxis(fields, true, anchorOf(horizontal, false), anchorOf(horizontal, true), pivot, preview->m_x, preview->m_width, preview->m_originX,
                     preview->m_viewWidth);
        }
        // An object under another is placed by the gap to it, which is not a place on the screen.
        if (vertical != AnchorAxis::Custom && !preview->m_isBelow)
        {
            double pivot = vertical == AnchorAxis::Stretch ? 0.5 : anchorOf(vertical, false);
            keepAxis(fields, false, anchorOf(vertical, false), anchorOf(vertical, true), pivot, preview->m_y, preview->m_height, preview->m_originY,
                     preview->m_viewHeight);
        }
        if (!fields.empty())
        {
            setTransformFields(*m_selected, fields, false);
        }
    }

    void Editor::setPivot(double x, double y)
    {
        std::optional<RectPreview> preview = rectPreview();
        if (!preview)
        {
            return;
        }
        std::map<std::string, double> fields;
        if (preview->m_horizontal != AnchorAxis::Stretch)
        {
            keepAxis(fields, true, preview->m_anchorMinX, preview->m_anchorMaxX, x, preview->m_x, preview->m_width, preview->m_originX,
                     preview->m_viewWidth);
        }
        else
        {
            fields["pivotX"] = x;
        }
        if (preview->m_vertical != AnchorAxis::Stretch && !preview->m_isBelow)
        {
            keepAxis(fields, false, preview->m_anchorMinY, preview->m_anchorMaxY, y, preview->m_y, preview->m_height, preview->m_originY,
                     preview->m_viewHeight);
        }
        else
        {
            fields["pivotY"] = y;
        }
        setTransformFields(*m_selected, fields, false);
    }

    void Editor::setGrid(const GridSettings &grid)
    {
        m_grid = grid;
        m_grid.m_step = std::max(1.0, m_grid.m_step);
        m_grid.m_rotationStep = std::max(0.1, m_grid.m_rotationStep);
        m_grid.m_scaleStep = std::max(0.001, m_grid.m_scaleStep);
        EditorState::setProjectValue(m_project.directory(), "grid",
                                     {{"shown", m_grid.m_isShown}, {"snap", m_grid.m_isSnapping}, {"step", m_grid.m_step}, {"rotation", m_grid.m_rotationStep}, {"scale", m_grid.m_scaleStep}});
        m_needsRender = true;
        changed();
    }

    std::map<std::string, double> Editor::snapped(std::map<std::string, double> fields) const
    {
        if (m_grid.m_isSnapping == m_isSnapHeld)
        {
            return fields;
        }
        auto toStep = [](double value, double step) { return std::round(value / step) * step; };
        for (auto &[name, value] : fields)
        {
            if (name == "x" || name == "y")
            {
                value = toStep(value, m_grid.m_step);
            }
            else if (name == "rotation")
            {
                value = toStep(value, m_grid.m_rotationStep);
            }
            else if (name == "scaleX" || name == "scaleY")
            {
                value = std::round(toStep(value, m_grid.m_scaleStep) * 1000) / 1000;
            }
        }
        return fields;
    }

    std::map<std::string, double> Editor::dragFields(const std::string &id, std::map<std::string, double> fields)
    {
        if (!isControl(id))
        {
            // The pointer moves the node in the world, and the node's own x and y are in its parent's space.
            const ObjectBounds *box = boundsOf(id);
            if (!box || !box->m_parent)
            {
                return fields;
            }
            if (!m_drag.m_hasMoved)
            {
                TransformValues start = transformOf(id);
                m_drag.m_originX = start.m_x;
                m_drag.m_originY = start.m_y;
            }
            auto x = fields.find("x");
            auto y = fields.find("y");
            if (x == fields.end() || y == fields.end())
            {
                return fields;
            }
            double worldDx = x->second - m_drag.m_originX;
            double worldDy = y->second - m_drag.m_originY;
            double radians = -box->m_parent->m_rotation * 3.14159265358979323846 / 180.0;
            double cosine = std::cos(radians);
            double sine = std::sin(radians);
            double scaleX = box->m_parent->m_scaleX != 0 ? box->m_parent->m_scaleX : 1;
            double scaleY = box->m_parent->m_scaleY != 0 ? box->m_parent->m_scaleY : 1;
            x->second = std::round(m_drag.m_originX + (worldDx * cosine - worldDy * sine) / scaleX);
            y->second = std::round(m_drag.m_originY + (worldDx * sine + worldDy * cosine) / scaleY);
            return fields;
        }
        if (!m_drag.m_hasMoved)
        {
            TransformValues start = transformOf(id);
            m_drag.m_appliedX = start.m_x;
            m_drag.m_appliedY = start.m_y;
        }
        auto number = [&](const char *field) {
            std::optional<Json> value = m_document.nodeField(id, field);
            return value && value->is_number() ? value->get<double>() : 0.0;
        };
        auto moveAxis = [&](const char *position, double &applied, const char *anchorMin, const char *anchorMax, const char *insetStart, const char *insetEnd,
                            const char *offset) {
            auto found = fields.find(position);
            if (found == fields.end())
            {
                return;
            }
            double delta = found->second - applied;
            applied = found->second;
            fields.erase(found);
            if (number(anchorMin) != number(anchorMax))
            {
                fields[insetStart] = number(insetStart) + delta;
                fields[insetEnd] = number(insetEnd) - delta;
            }
            else
            {
                fields[offset] = number(offset) + delta;
            }
        };
        moveAxis("x", m_drag.m_appliedX, "anchorMinX", "anchorMaxX", "insetLeft", "insetRight", "offsetX");
        moveAxis("y", m_drag.m_appliedY, "anchorMinY", "anchorMaxY", "insetTop", "insetBottom", "offsetY");
        return fields;
    }

    std::vector<GizmoHandle> Editor::currentHandles() const
    {
        return handlesFor(m_viewport);
    }

    std::vector<GizmoHandle> Editor::handlesFor(const Viewport &view) const
    {
        if (m_selected && !isLocked(*m_selected))
        {
            for (const ObjectBounds &box : m_frame.m_bounds)
            {
                if (box.m_id == *m_selected)
                {
                    // A handle is drawn where the node is in the world, and its fields are in its parent's space, so a child is placed by the origin the player reports.
                    TransformValues transform = transformOf(box.m_id);
                    if (box.m_origin)
                    {
                        transform.m_x = box.m_origin->first;
                        transform.m_y = box.m_origin->second;
                    }
                    return Gizmo::handles(m_gizmoMode, view, box, transform);
                }
            }
        }
        return {};
    }

    double Editor::gameCameraZoom() const
    {
        auto id = m_document.cameraId();
        if (!id)
        {
            return 1.0;
        }
        std::optional<Json> zoom = m_document.nodeField(*id, "zoom");
        return zoom && zoom->is_number() && zoom->get<double>() > 0 ? zoom->get<double>() : 1.0;
    }

    DrawList Editor::overlay() const
    {
        if (m_viewMode == ViewMode::ThreeD)
        {
            return overlay3D();
        }
        std::optional<std::pair<double, double>> gameCamera;
        if (auto id = m_document.cameraId())
        {
            for (const ObjectBounds &box : m_frame.m_bounds)
            {
                if (box.m_id == *id)
                {
                    gameCamera = std::make_pair(box.m_x + box.m_width / 2, box.m_y + box.m_height / 2);
                }
            }
        }
        // The game shows its display's size divided by the camera's zoom of the platform being previewed.
        std::pair<int, int> logical = m_project.logicalSize(m_platform);
        if (!gameCamera)
        {
            // Without a camera the game's view starts at the world's origin.
            gameCamera = std::make_pair(logical.first / 2.0, logical.second / 2.0);
        }
        double zoom = gameCameraZoom();
        DrawList list = m_grid.m_isShown ? m_frameView.grid(m_grid.m_step) : DrawList();
        DrawList scene = m_frameView.overlay(m_frame.m_bounds, m_selected, gameCamera, (int)std::lround(logical.first / zoom), (int)std::lround(logical.second / zoom));
        list.insert(list.end(), scene.begin(), scene.end());
        DrawList handles = Gizmo::draw(handlesFor(m_frameView));
        list.insert(list.end(), handles.begin(), handles.end());
        DrawList anchors = anchorOverlay();
        list.insert(list.end(), anchors.begin(), anchors.end());
        if (m_modal && m_modal->m_axis != 0 && m_modal->m_kind != Modal::Rotate)
        {
            // The line through the node that the change is held to, in the color of the axis.
            double reach = 4000;
            double originX = m_frameView.worldToFrameX(m_viewport.frameToWorldX(m_modal->m_originFrameX));
            double originY = m_frameView.worldToFrameY(m_viewport.frameToWorldY(m_modal->m_originFrameY));
            bool isX = m_modal->m_axis == 'x';
            unsigned int color = isX ? 0xffe05050 : 0xff50d060;
            list.push_back({DrawCommand::Kind::Line, isX ? originX - reach : originX, isX ? originY : originY - reach, isX ? reach * 2 : 0, isX ? 0 : reach * 2, color});
        }
        return list;
    }

    void Editor::documentChanged(const DocumentChange &change)
    {
        using Kind = DocumentChange::Kind;
        // The player is told of an edit as it is made only while it is ready and no whole scene is waiting to be sent, which would include the edit.
        bool isLive = m_status == Status::Ready && !m_needsLoad;
        // A game that is running is told of the same edits, which it applies to the scene it has on screen. A change of class cannot be, and waits for the next run.
        if (m_isPlaying)
        {
            switch (change.m_kind)
            {
            case Kind::Field:
                if (!change.m_value.is_null())
                {
                    m_play.setField(change.m_nodeId, change.m_field, change.m_value);
                }
                break;
            case Kind::Added:
                m_play.addNode(change.m_parentId, change.m_index, change.m_value);
                break;
            case Kind::Removed:
                m_play.removeNode(change.m_nodeId);
                break;
            case Kind::Moved:
                m_play.moveNode(change.m_nodeId, change.m_parentId, change.m_index);
                break;
            default:
                break;
            }
        }
        switch (change.m_kind)
        {
        case Kind::Field:
            if (change.m_value.is_null())
            {
                m_needsLoad = true;
            }
            else if (m_status == Status::Ready)
            {
                m_session.setField(change.m_nodeId, change.m_field, change.m_value);
            }
            break;
        case Kind::Added:
            if (isLive)
            {
                m_mayResync = true;
                m_session.addNode(change.m_parentId, change.m_index, change.m_value);
            }
            else
            {
                m_needsLoad = true;
            }
            break;
        case Kind::Removed:
            if (isLive)
            {
                m_mayResync = true;
                m_session.removeNode(change.m_nodeId);
            }
            else
            {
                m_needsLoad = true;
            }
            break;
        case Kind::Moved:
            if (isLive)
            {
                m_mayResync = true;
                m_session.moveNode(change.m_nodeId, change.m_parentId, change.m_index);
            }
            else
            {
                m_needsLoad = true;
            }
            break;
        case Kind::Camera:
            break;
        case Kind::Structure:
            m_needsLoad = true;
            break;
        }
        m_needsRender = true;
    }

    void Editor::cameraChanged()
    {
        m_isViewDirty = true;
        m_viewChangedAt = std::chrono::steady_clock::now();
        m_needsCamera = true;
        changed();
    }

    void Editor::changed()
    {
        if (m_onChanged)
        {
            m_onChanged();
        }
    }

    void Editor::pointerDown(double frameX, double frameY, Pointer pointer)
    {
        m_drag = Drag();
        m_drag.m_lastFrameX = frameX;
        m_drag.m_lastFrameY = frameY;
        m_drag.m_startFrameX = frameX;
        m_drag.m_startFrameY = frameY;
        if (m_viewMode == ViewMode::TwoD && pointer == Pointer::Look)
        {
            pointer = Pointer::Pan;
        }
        if (pointer == Pointer::Pan)
        {
            m_drag.m_kind = Drag::Kind::Pan;
            return;
        }
        if (pointer == Pointer::Look)
        {
            m_drag.m_kind = Drag::Kind::Look;
            return;
        }
        if (m_viewMode == ViewMode::ThreeD)
        {
            if (auto handle = Gizmo3D::hit(handles3D(), frameX, frameY))
            {
                std::optional<Json> value = m_document.nodeField(*m_selected, Gizmo3D::fieldOf(handle->m_axis));
                m_drag.m_kind = Drag::Kind::Handle3D;
                m_drag.m_id = *m_selected;
                m_drag.m_axisHandle = *handle;
                m_drag.m_startValue = value && value->is_number() ? value->get<double>() : 0.0;
                return;
            }
            select(hit3D(frameX, frameY));
            return;
        }

        if (auto handle = Gizmo::hit(currentHandles(), frameX, frameY))
        {
            for (const ObjectBounds &box : m_frame.m_bounds)
            {
                if (box.m_id == *m_selected)
                {
                    m_drag.m_kind = Drag::Kind::Handle;
                    m_drag.m_id = box.m_id;
                    m_drag.m_handle.emplace(*handle, m_viewport, box, transformOf(box.m_id), frameX, frameY);
                    return;
                }
            }
        }

        // A press on what is not selected only selects it. What is selected is moved by pressing anywhere inside its box, even where a child sits on top of it, so a node is never dragged out from under the pointer by what covers it.
        if (isOverSelected(frameX, frameY))
        {
            m_drag.m_kind = Drag::Kind::Object;
            m_drag.m_id = *m_selected;
            m_drag.m_startWorldX = m_viewport.frameToWorldX(frameX);
            m_drag.m_startWorldY = m_viewport.frameToWorldY(frameY);
            TransformValues values = transformOf(*m_selected);
            m_drag.m_startX = values.m_x;
            m_drag.m_startY = values.m_y;
            return;
        }
        if (m_locked.empty())
        {
            select(Viewport::hit(m_viewport, m_frame.m_bounds, frameX, frameY));
            return;
        }
        std::vector<ObjectBounds> pickable;
        for (const ObjectBounds &box : m_frame.m_bounds)
        {
            if (!isLocked(box.m_id))
            {
                pickable.push_back(box);
            }
        }
        select(Viewport::hit(m_viewport, pickable, frameX, frameY));
    }

    bool Editor::isOverSelected(double frameX, double frameY) const
    {
        if (!m_selected || isLocked(*m_selected))
        {
            return false;
        }
        for (const ObjectBounds &box : m_frame.m_bounds)
        {
            if (box.m_id == *m_selected)
            {
                return Viewport::hit(m_viewport, {box}, frameX, frameY).has_value();
            }
        }
        return false;
    }

    Editor::PointerTarget Editor::pointerTarget(double frameX, double frameY) const
    {
        PointerTarget target;
        switch (m_drag.m_kind)
        {
        case Drag::Kind::Pan:
        case Drag::Kind::Look:
            target.m_kind = PointerTarget::Kind::Pan;
            return target;
        case Drag::Kind::Object:
            target.m_kind = PointerTarget::Kind::Object;
            return target;
        case Drag::Kind::Handle:
            target.m_kind = PointerTarget::Kind::Handle;
            target.m_handle = m_drag.m_handle->kind();
            return target;
        case Drag::Kind::Handle3D:
            target.m_kind = PointerTarget::Kind::Handle;
            target.m_handle = GizmoHandle::Kind::MoveXY;
            return target;
        case Drag::Kind::None:
            break;
        }
        if (m_viewMode == ViewMode::ThreeD)
        {
            if (Gizmo3D::hit(handles3D(), frameX, frameY))
            {
                target.m_kind = PointerTarget::Kind::Handle;
                target.m_handle = GizmoHandle::Kind::MoveXY;
            }
        }
        else if (auto handle = Gizmo::hit(currentHandles(), frameX, frameY))
        {
            target.m_kind = PointerTarget::Kind::Handle;
            target.m_handle = handle->m_kind;
        }
        else if (isOverSelected(frameX, frameY))
        {
            target.m_kind = PointerTarget::Kind::Object;
        }
        return target;
    }

    bool Editor::beginModal(Modal kind, double frameX, double frameY)
    {
        if (m_viewMode != ViewMode::TwoD || !m_selected || isLocked(*m_selected) || m_modal)
        {
            return false;
        }
        const ObjectBounds *box = boundsOf(*m_selected);
        if (!box)
        {
            return false;
        }
        m_drag = Drag();
        ModalState state;
        state.m_kind = kind;
        state.m_id = *m_selected;
        state.m_start = transformOf(*m_selected);
        state.m_startFrameX = state.m_frameX = frameX;
        state.m_startFrameY = state.m_frameY = frameY;
        std::pair<double, double> origin = box->m_origin ? *box->m_origin : std::make_pair(box->m_x, box->m_y);
        state.m_originFrameX = m_viewport.worldToFrameX(origin.first);
        state.m_originFrameY = m_viewport.worldToFrameY(origin.second);
        m_modal = state;
        changed();
        return true;
    }

    void Editor::modalAxis(char axis)
    {
        if (m_modal)
        {
            m_modal->m_axis = m_modal->m_axis == axis ? 0 : axis;
            applyModal();
        }
    }

    void Editor::modalType(char character)
    {
        if (!m_modal)
        {
            return;
        }
        std::string &typed = m_modal->m_typed;
        bool isDigit = character >= '0' && character <= '9';
        bool isPoint = character == '.' && typed.find('.') == std::string::npos;
        bool isSign = character == '-' && typed.empty();
        if (isDigit || isPoint || isSign)
        {
            typed += character;
            applyModal();
        }
    }

    void Editor::modalBackspace()
    {
        if (m_modal && !m_modal->m_typed.empty())
        {
            m_modal->m_typed.pop_back();
            applyModal();
        }
    }

    void Editor::modalMove(double frameX, double frameY)
    {
        if (m_modal && (m_modal->m_frameX != frameX || m_modal->m_frameY != frameY))
        {
            m_modal->m_frameX = frameX;
            m_modal->m_frameY = frameY;
            applyModal();
        }
    }

    void Editor::applyModal()
    {
        const ModalState &modal = *m_modal;
        bool isTyped = !modal.m_typed.empty() && modal.m_typed != "-" && modal.m_typed != ".";
        double typed = isTyped ? std::atof(modal.m_typed.c_str()) : 0.0;
        std::map<std::string, double> fields;
        if (modal.m_kind == Modal::Move)
        {
            double deltaX = (modal.m_frameX - modal.m_startFrameX) / m_viewport.zoom();
            double deltaY = (modal.m_frameY - modal.m_startFrameY) / m_viewport.zoom();
            if (isTyped)
            {
                deltaX = modal.m_axis == 'y' ? 0 : typed;
                deltaY = modal.m_axis == 'y' ? typed : 0;
            }
            else if (modal.m_axis == 'x')
            {
                deltaY = 0;
            }
            else if (modal.m_axis == 'y')
            {
                deltaX = 0;
            }
            fields = {{"x", std::round(modal.m_start.m_x + deltaX)}, {"y", std::round(modal.m_start.m_y + deltaY)}};
            if (!isTyped)
            {
                fields = snapped(fields);
            }
            fields = dragFields(modal.m_id, fields);
        }
        else if (modal.m_kind == Modal::Rotate)
        {
            double degrees = typed;
            if (!isTyped)
            {
                double start = std::atan2(modal.m_startFrameY - modal.m_originFrameY, modal.m_startFrameX - modal.m_originFrameX);
                double now = std::atan2(modal.m_frameY - modal.m_originFrameY, modal.m_frameX - modal.m_originFrameX);
                degrees = (now - start) * 180 / 3.14159265358979323846;
            }
            fields["rotation"] = std::round((modal.m_start.m_rotation + degrees) * 10) / 10;
            if (!isTyped)
            {
                fields = snapped(fields);
            }
        }
        else
        {
            double factor = typed;
            if (!isTyped)
            {
                double startDistance = std::max(1.0, std::hypot(modal.m_startFrameX - modal.m_originFrameX, modal.m_startFrameY - modal.m_originFrameY));
                factor = std::hypot(modal.m_frameX - modal.m_originFrameX, modal.m_frameY - modal.m_originFrameY) / startDistance;
            }
            if (modal.m_axis != 'y')
            {
                fields["scaleX"] = std::round(modal.m_start.m_scaleX * factor * 100) / 100;
            }
            if (modal.m_axis != 'x')
            {
                fields["scaleY"] = std::round(modal.m_start.m_scaleY * factor * 100) / 100;
            }
            if (!isTyped)
            {
                fields = snapped(fields);
            }
        }
        setTransformFields(modal.m_id, fields, m_drag.m_hasMoved);
        m_drag.m_hasMoved = true;
        changed();
    }

    void Editor::confirmModal()
    {
        m_modal.reset();
        m_drag = Drag();
        changed();
    }

    void Editor::cancelModal()
    {
        if (!m_modal)
        {
            return;
        }
        if (m_drag.m_hasMoved)
        {
            m_commands->undoAndForget();
        }
        confirmModal();
    }

    std::string Editor::modalText() const
    {
        if (!m_modal)
        {
            return std::string();
        }
        const char *names[] = {"Move", "Rotate", "Scale"};
        std::string text = names[(int)m_modal->m_kind];
        if (m_modal->m_axis != 0)
        {
            text += std::string(" ") + (char)std::toupper(m_modal->m_axis);
        }
        if (!m_modal->m_typed.empty())
        {
            text += ": " + m_modal->m_typed;
        }
        return text;
    }

    void Editor::pointerMove(double frameX, double frameY)
    {
        if (m_drag.m_kind == Drag::Kind::Pan || m_drag.m_kind == Drag::Kind::Look)
        {
            double deltaX = frameX - m_drag.m_lastFrameX;
            double deltaY = frameY - m_drag.m_lastFrameY;
            if (m_viewMode == ViewMode::ThreeD)
            {
                m_drag.m_kind == Drag::Kind::Look ? m_viewport3D.look(deltaX, deltaY) : m_viewport3D.pan(deltaX, deltaY);
            }
            else
            {
                m_viewport.panByFrame(deltaX, deltaY);
            }
            m_drag.m_lastFrameX = frameX;
            m_drag.m_lastFrameY = frameY;
            cameraChanged();
        }
        else if (m_drag.m_kind == Drag::Kind::Handle3D)
        {
            double moved = Gizmo3D::delta(*m_drag.m_axisHandle, m_drag.m_startFrameX, m_drag.m_startFrameY, frameX, frameY);
            double value = std::round((m_drag.m_startValue + moved) * 100) / 100;
            setTransformFields(m_drag.m_id, {{Gizmo3D::fieldOf(m_drag.m_axisHandle->m_axis), value}}, m_drag.m_hasMoved);
            m_drag.m_hasMoved = true;
        }
        else if (m_drag.m_kind == Drag::Kind::Handle)
        {
            setTransformFields(m_drag.m_id, dragFields(m_drag.m_id, snapped(m_drag.m_handle->update(m_viewport, frameX, frameY))), m_drag.m_hasMoved);
            m_drag.m_hasMoved = true;
        }
        else if (m_drag.m_kind == Drag::Kind::Object)
        {
            double x = std::round(m_drag.m_startX + m_viewport.frameToWorldX(frameX) - m_drag.m_startWorldX);
            double y = std::round(m_drag.m_startY + m_viewport.frameToWorldY(frameY) - m_drag.m_startWorldY);
            setTransformFields(m_drag.m_id, dragFields(m_drag.m_id, snapped({{"x", x}, {"y", y}})), m_drag.m_hasMoved);
            m_drag.m_hasMoved = true;
        }
    }

    void Editor::pointerUp()
    {
        m_drag = Drag();
    }

    void Editor::wheel(double frameX, double frameY, double steps)
    {
        if (m_viewMode == ViewMode::ThreeD)
        {
            m_viewport3D.dolly(steps);
        }
        else
        {
            m_viewport.zoomAt(frameX, frameY, std::pow(1.1, steps));
        }
        cameraChanged();
    }
}
