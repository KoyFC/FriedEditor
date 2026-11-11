#include "fried/editor/editor.h"

#include "fried/editor/commands.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace fried::editor
{
    using Json = nlohmann::json;
    namespace fs = std::filesystem;

    std::string Editor::findEngine(const std::string &projectDirectory)
    {
        for (const fs::path &candidate : {fs::path(projectDirectory) / "engine", fs::path(projectDirectory) / ".."})
        {
            std::error_code ignored;
            fs::path normal = fs::weakly_canonical(candidate, ignored);
            if (fs::exists(normal / "editor" / "player" / "build.hxml", ignored))
            {
                return normal.string();
            }
        }
        return std::string();
    }

    bool Editor::open(const std::string &projectDirectory, const std::string &enginePath, std::string *error)
    {
        if (!m_project.load(projectDirectory, error))
        {
            return false;
        }
        m_enginePath = enginePath;

        std::vector<std::string> scenes = scenePaths();
        if (scenes.empty())
        {
            m_scenePath = m_project.scenesDirectory() + "/main.scene.json";
            m_document = SceneDocument::empty("Main");
        }
        else
        {
            // The scene that was open last time, when it is still there.
            std::string chosen = scenes.front();
            std::string remembered = EditorState::lastScene(m_project.directory());
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
            EditorState::setLastScene(m_project.directory(), fs::path(chosen).filename().string());
        }

        m_commands = std::make_unique<CommandStack>(m_document);
        m_commands->m_onChange = [this] { changed(); };
        m_document.m_onChange = [this](const DocumentChange &change) { documentChanged(change); };
        m_viewport.setFrameSize(s_frameWidth, s_frameHeight);
        m_viewport.setCamera(s_frameWidth / 2.0, s_frameHeight / 2.0, 1.0);

        if (isPlayerStale(m_enginePath, m_project.directory()))
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
            progress.m_log = tailOfFile(playerLogFile(), 40);
        }
        else if (m_game.state() == GameRunner::State::Building)
        {
            progress.m_isVisible = true;
            progress.m_canStop = true;
            progress.m_title = "Building the game";
            progress.m_step = (int)m_game.step();
            progress.m_steps = (int)m_game.stepCount();
            progress.m_stepLabel = m_game.stepLabel();
            progress.m_log = tailOfFile(m_game.logFile(), 40);
        }
        else if (m_isFailureShown)
        {
            progress.m_isVisible = true;
            progress.m_isFailed = true;
            progress.m_title = m_failedTitle;
            progress.m_message = m_failedMessage;
            progress.m_log = tailOfFile(m_failedLog, 40);
        }
        return progress;
    }

    void Editor::dismissBuild()
    {
        m_isFailureShown = false;
        changed();
    }

    void Editor::rebuildPlayer()
    {
        m_isFailureShown = false;
        m_builder = std::make_unique<PlayerBuilder>(hasProjectPlayer(m_project.directory())
            ? PlayerBuilder::projectSteps(m_enginePath, m_project.directory(), playerLogFile())
            : PlayerBuilder::engineSteps(m_enginePath, playerLogFile()));
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
            m_needsLoad = true;
            m_needsCamera = true;
            m_needsRender = true;
            changed();
        };
        m_session.m_onFrame = [this](const Frame &frame) {
            m_frame = frame;
            m_frameView = m_inFlightView;
            m_frameView.setFrameSize(frame.m_width, frame.m_height);
            ++m_frameSerial;
            m_viewport.setFrameSize(frame.m_width, frame.m_height);
            m_isRenderInFlight = false;
            changed();
        };
        m_session.m_onError = [this](const std::string &message) {
            m_message = message;
            m_isRenderInFlight = false;
            if (m_session.state() == PlayerSession::State::Closed)
            {
                m_status = Status::Failed;
            }
            changed();
        };

        std::string error;
        if (!m_session.launch(playerExecutablePath(m_enginePath, m_project.directory()), m_enginePath, m_project.directory(), s_frameWidth, s_frameHeight, &error))
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

    void Editor::play()
    {
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
        if (m_game.start(m_project.directory(), log, &error))
        {
            m_message = "Building the game, the log is " + log;
        }
        else
        {
            m_message = error;
            m_isFailureShown = true;
            m_failedTitle = "The game cannot be built";
            m_failedLog.clear();
            m_failedMessage = error;
        }
        changed();
    }

    void Editor::stopGame()
    {
        m_game.stop();
        m_message = "Stopped the game";
        changed();
    }

    void Editor::setViewSize(int width, int height)
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
        GameRunner::State before = m_game.state();
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
                m_isFailureShown = true;
                m_failedTitle = "Building the game failed";
                m_failedLog = m_game.logFile();
                m_failedMessage = m_game.failure();
            }
            else if (after == GameRunner::State::Idle)
            {
                m_message = "The game closed";
            }
            changed();
        }

        if (m_builder)
        {
            PlayerBuilder::State state = m_builder->pump();
            if (state == PlayerBuilder::State::Failed)
            {
                m_status = Status::Failed;
                m_message = m_builder->failure() + " The log is " + playerLogFile();
                m_isFailureShown = true;
                m_failedTitle = "Building the editor player failed";
                m_failedLog = playerLogFile();
                m_failedMessage = m_builder->failure();
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
            m_needsRender = true;
        }
        if (m_needsCamera)
        {
            m_needsCamera = false;
            m_session.setEditorCamera(m_viewport.cameraX(), m_viewport.cameraY(), m_viewport.zoom());
            m_sentView = m_viewport;
            m_needsRender = true;
        }
        if (m_needsRender && !m_isRenderInFlight)
        {
            m_needsRender = false;
            m_isRenderInFlight = true;
            m_inFlightView = m_sentView;
            m_session.render();
        }
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

    bool Editor::openScene(const std::string &path, std::string *error)
    {
        SceneDocument loaded;
        if (!loaded.load(path, error))
        {
            return false;
        }
        m_document.m_onChange = nullptr;
        m_document = std::move(loaded);
        m_document.m_onChange = [this](const DocumentChange &change) { documentChanged(change); };
        m_commands = std::make_unique<CommandStack>(m_document);
        m_commands->m_onChange = [this] { changed(); };
        m_scenePath = path;
        EditorState::setLastScene(m_project.directory(), fs::path(path).filename().string());
        m_selected.reset();
        m_needsLoad = true;
        changed();
        return true;
    }

    bool Editor::save(std::string *error)
    {
        std::error_code ignored;
        fs::create_directories(fs::path(m_scenePath).parent_path(), ignored);
        if (!m_document.save(m_scenePath, error))
        {
            return false;
        }
        m_commands->markClean();
        changed();
        return true;
    }

    void Editor::setPlatform(Platform platform)
    {
        m_platform = platform;
        m_needsLoad = true;
        if (m_status == Status::Ready)
        {
            m_message = "Playing on " + platformLabel(m_platform);
        }
        changed();
    }

    void Editor::select(const std::optional<std::string> &id)
    {
        m_selected = id;
        m_needsRender = true;
        changed();
    }

    std::vector<HierarchyRow> Editor::hierarchy() const
    {
        std::vector<HierarchyRow> rows;
        for (const Json &object : m_document.data()["objects"])
        {
            std::string id = object.value("id", "");
            rows.push_back({id, object.value("name", id), m_document.cameraId() == id});
        }
        return rows;
    }

    const ComponentSchema *Editor::schemaOf(const std::string &type) const
    {
        for (const ComponentSchema &schema : m_session.schema())
        {
            if (schema.m_name == type)
            {
                return &schema;
            }
        }
        return nullptr;
    }

    Json Editor::newComponent(const std::string &type) const
    {
        Json component = {{"type", type}};
        if (const ComponentSchema *schema = schemaOf(type))
        {
            for (const FieldSchema &field : schema->m_fields)
            {
                if (field.m_name != "enabled" && !field.m_default.is_null())
                {
                    component[field.m_name] = field.m_default;
                }
            }
        }
        return component;
    }

    void Editor::addObject(const std::string &componentType)
    {
        std::string base = componentType.empty() ? "object" : componentType;
        std::transform(base.begin(), base.end(), base.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        std::string id = m_document.uniqueId(base);

        Json transform = newComponent("Transform");
        transform["x"] = std::round(m_viewport.cameraX());
        transform["y"] = std::round(m_viewport.cameraY());
        Json components = Json::array({transform});
        if (!componentType.empty() && componentType != "Transform")
        {
            components.push_back(newComponent(componentType));
        }
        std::string name = componentType.empty() ? "Object" : componentType;
        m_commands->execute(std::make_unique<AddObject>((int)m_document.objectIds().size(), Json{{"id", id}, {"name", name}, {"components", components}}));
        select(id);
    }

    void Editor::removeSelected()
    {
        if (m_selected && m_commands->execute(std::make_unique<RemoveObject>(*m_selected)))
        {
            select(std::nullopt);
        }
    }

    void Editor::moveSelected(int delta)
    {
        if (m_selected)
        {
            int target = m_document.indexOf(*m_selected) + delta;
            if (target >= 0 && target < (int)m_document.objectIds().size())
            {
                m_commands->execute(std::make_unique<MoveObject>(*m_selected, target));
            }
        }
    }

    void Editor::setCameraToSelected()
    {
        if (m_selected)
        {
            m_commands->execute(std::make_unique<SetCamera>(m_selected));
        }
    }

    void Editor::renameSelected(const std::string &name)
    {
        if (m_selected)
        {
            m_commands->execute(std::make_unique<SetObjectField>(*m_selected, "name", Json(name)));
        }
    }

    std::vector<ComponentView> Editor::inspector() const
    {
        std::vector<ComponentView> views;
        const Json *object = m_selected ? m_document.object(*m_selected) : nullptr;
        if (!object)
        {
            return views;
        }
        for (const Json &component : (*object)["components"])
        {
            std::string type = component.value("type", "");
            const ComponentSchema *schema = schemaOf(type);
            if (!schema)
            {
                continue;
            }
            ComponentView view;
            view.m_type = type;
            view.m_isRemovable = type != "Transform";
            bool isRect = type == "RectTransform";
            for (const FieldSchema &field : schema->m_fields)
            {
                // A RectTransform lays out the position, so the offsets are what is edited.
                if (isRect && (field.m_name == "x" || field.m_name == "y"))
                {
                    continue;
                }
                FieldView fieldView;
                fieldView.m_schema = field;
                fieldView.m_isSet = component.contains(field.m_name);
                fieldView.m_value = fieldView.m_isSet ? component[field.m_name] : field.m_default;
                view.m_fields.push_back(std::move(fieldView));
            }
            views.push_back(std::move(view));
        }
        return views;
    }

    std::vector<std::string> Editor::addableComponents() const
    {
        std::vector<std::string> types;
        const Json *object = m_selected ? m_document.object(*m_selected) : nullptr;
        if (!object)
        {
            return types;
        }
        for (const ComponentSchema &schema : m_session.schema())
        {
            bool has = false;
            for (const Json &component : (*object)["components"])
            {
                has = has || component.value("type", "") == schema.m_name;
            }
            // A transform is replaced, never added next to another.
            if (!has && schema.m_name != "Transform")
            {
                types.push_back(schema.m_name);
            }
        }
        return types;
    }

    std::vector<std::string> Editor::componentTypes() const
    {
        std::vector<std::string> types;
        for (const ComponentSchema &schema : m_session.schema())
        {
            types.push_back(schema.m_name);
        }
        return types;
    }

    std::optional<Json> Editor::selectedObjectField(const std::string &field) const
    {
        return m_selected ? m_document.objectField(*m_selected, field) : std::nullopt;
    }

    void Editor::setObjectField(const std::string &field, const std::optional<Json> &value)
    {
        if (m_selected)
        {
            m_commands->execute(std::make_unique<SetObjectField>(*m_selected, field, value), true);
        }
    }

    void Editor::setField(const std::string &type, const std::string &field, const std::optional<Json> &value)
    {
        if (!m_selected)
        {
            return;
        }
        m_commands->execute(std::make_unique<SetComponentField>(*m_selected, type, field, value), true);
    }

    void Editor::setTransformFields(const std::string &id, const std::map<std::string, double> &fields, bool merge)
    {
        m_commands->execute(std::make_unique<SetTransformFields>(id, fields), merge);
    }

    void Editor::addComponent(const std::string &type)
    {
        if (m_selected)
        {
            const Json *object = m_document.object(*m_selected);
            if (type == "RectTransform")
            {
                m_commands->execute(std::make_unique<ChangeTransformType>(*m_selected, "RectTransform"));
                // As in Unity's interface, it starts anchored to the center of the screen, where the object is.
                setAnchorPreset(AnchorAxis::Center, AnchorAxis::Center);
                return;
            }
            m_commands->execute(std::make_unique<AddComponent>(*m_selected, (int)(*object)["components"].size(), newComponent(type)));
        }
    }

    void Editor::removeComponent(const std::string &type)
    {
        if (m_selected)
        {
            if (type == "RectTransform")
            {
                m_commands->execute(std::make_unique<ChangeTransformType>(*m_selected, "Transform"));
                return;
            }
            m_commands->execute(std::make_unique<RemoveComponent>(*m_selected, type));
        }
    }

    std::vector<AssetEntry> Editor::assets() const
    {
        std::vector<AssetEntry> entries = listAssets("game", m_project.assetsDirectory());
        std::vector<AssetEntry> engine = listAssets("engine", m_enginePath + "/assets");
        entries.insert(entries.end(), engine.begin(), engine.end());
        return entries;
    }

    std::string Editor::assetFile(const std::string &root, const std::string &path) const
    {
        return (root == "engine" ? m_enginePath + "/assets/" : m_project.assetsDirectory() + "/") + path;
    }

    void Editor::dropAsset(const std::string &root, const std::string &path, double frameX, double frameY)
    {
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
                bool hasSprite = false;
                for (const Json &component : (*m_document.object(*target))["components"])
                {
                    hasSprite = hasSprite || component.value("type", "") == "Sprite";
                }
                if (hasSprite)
                {
                    select(target);
                    m_commands->execute(std::make_unique<SetComponentField>(*target, "Sprite", "texture", reference));
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
        Json transform = newComponent("Transform");
        transform["x"] = std::round(m_viewport.frameToWorldX(frameX));
        transform["y"] = std::round(m_viewport.frameToWorldY(frameY));
        Json component = newComponent(kind == AssetKind::Image ? "Sprite" : "Text");
        component[kind == AssetKind::Image ? "texture" : "font"] = reference;
        if (kind == AssetKind::Font)
        {
            component["text"] = "Text";
            component["color"] = {{"r", 255}, {"g", 255}, {"b", 255}, {"a", 255}};
        }
        m_commands->execute(std::make_unique<AddObject>((int)m_document.objectIds().size(),
                                                        Json{{"id", id}, {"name", stem}, {"components", Json::array({transform, component})}}));
        select(id);
    }

    std::vector<AssetIssue> Editor::issues() const
    {
        return validateAssets(m_platform, m_project.assetsDirectory());
    }

    bool Editor::fixIssue(const AssetIssue &issue, std::string *error)
    {
        if (!applyFix(issue, m_project.assetsDirectory(), m_project.directory() + "/.fried-editor/originals", error))
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
            std::optional<Json> value = m_document.componentField(id, m_document.transformType(id), field);
            return value && value->is_number() ? value->get<double>() : fallback;
        };
        TransformValues values = {number("x", 0), number("y", 0), number("rotation", 0), number("scaleX", 1), number("scaleY", 1)};
        if (m_document.transformType(id) == "RectTransform")
        {
            // Where the layout put it, which the last frame shows.
            for (const ObjectBounds &box : m_frame.m_bounds)
            {
                if (box.m_id == id)
                {
                    values.m_x = box.m_x;
                    values.m_y = box.m_y;
                }
            }
        }
        return values;
    }

    // The anchors of the selected object's RectTransform, as fractions of the screen the layout is made for: a marker at each corner of the area they span, and that area itself when the object stretches.
    DrawList Editor::anchorOverlay(int viewWidth, int viewHeight) const
    {
        DrawList list;
        if (!m_selected || m_document.transformType(*m_selected) != "RectTransform")
        {
            return list;
        }
        auto number = [&](const char *field) {
            std::optional<Json> value = m_document.componentField(*m_selected, "RectTransform", field);
            return value && value->is_number() ? value->get<double>() : 0.0;
        };
        constexpr unsigned int anchorColor = 0xff40c0ff;
        double left = m_frameView.worldToFrameX(viewWidth * number("anchorMinX"));
        double right = m_frameView.worldToFrameX(viewWidth * number("anchorMaxX"));
        double top = m_frameView.worldToFrameY(viewHeight * number("anchorMinY"));
        double bottom = m_frameView.worldToFrameY(viewHeight * number("anchorMaxY"));
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
        if (!m_selected || m_document.transformType(*m_selected) != "RectTransform")
        {
            return std::nullopt;
        }
        const ObjectBounds *box = nullptr;
        for (const ObjectBounds &candidate : m_frame.m_bounds)
        {
            if (candidate.m_id == *m_selected)
            {
                box = &candidate;
            }
        }
        if (!box)
        {
            return std::nullopt;
        }
        auto number = [&](const char *field) {
            std::optional<Json> value = m_document.componentField(*m_selected, "RectTransform", field);
            return value && value->is_number() ? value->get<double>() : 0.0;
        };
        auto axisOf = [](double minimum, double maximum) {
            if (minimum != maximum)
            {
                return minimum == 0 && maximum == 1 ? AnchorAxis::Stretch : AnchorAxis::Custom;
            }
            return minimum == 0 ? AnchorAxis::Start : minimum == 0.5 ? AnchorAxis::Center : minimum == 1 ? AnchorAxis::End : AnchorAxis::Custom;
        };
        std::pair<int, int> logical = m_project.logicalSize(m_platform);
        RectPreview preview;
        preview.m_viewWidth = logical.first;
        preview.m_viewHeight = logical.second;
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
        std::optional<Json> below = m_document.componentField(*m_selected, "RectTransform", "below");
        preview.m_isBelow = below && below->is_string() && !below->get<std::string>().empty();
        return preview;
    }

    namespace
    {
        // The fields of one axis that keep a box of the given start and size where it is, for the anchors and pivot the axis is given.
        void keepAxis(std::map<std::string, double> &fields, bool isHorizontal, double anchorMin, double anchorMax, double pivot, double start, double size,
                      double view)
        {
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
            keepAxis(fields, true, anchorOf(horizontal, false), anchorOf(horizontal, true), pivot, preview->m_x, preview->m_width, preview->m_viewWidth);
        }
        // An object under another is placed by the gap to it, which is not a place on the screen.
        if (vertical != AnchorAxis::Custom && !preview->m_isBelow)
        {
            double pivot = vertical == AnchorAxis::Stretch ? 0.5 : anchorOf(vertical, false);
            keepAxis(fields, false, anchorOf(vertical, false), anchorOf(vertical, true), pivot, preview->m_y, preview->m_height, preview->m_viewHeight);
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
            keepAxis(fields, true, preview->m_anchorMinX, preview->m_anchorMaxX, x, preview->m_x, preview->m_width, preview->m_viewWidth);
        }
        else
        {
            fields["pivotX"] = x;
        }
        if (preview->m_vertical != AnchorAxis::Stretch && !preview->m_isBelow)
        {
            keepAxis(fields, false, preview->m_anchorMinY, preview->m_anchorMaxY, y, preview->m_y, preview->m_height, preview->m_viewHeight);
        }
        else
        {
            fields["pivotY"] = y;
        }
        setTransformFields(*m_selected, fields, false);
    }

    std::map<std::string, double> Editor::dragFields(const std::string &id, std::map<std::string, double> fields)
    {
        if (m_document.transformType(id) != "RectTransform")
        {
            return fields;
        }
        if (!m_drag.m_hasMoved)
        {
            TransformValues start = transformOf(id);
            m_drag.m_appliedX = start.m_x;
            m_drag.m_appliedY = start.m_y;
        }
        auto number = [&](const char *field) {
            std::optional<Json> value = m_document.componentField(id, "RectTransform", field);
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
        if (m_selected)
        {
            for (const ObjectBounds &box : m_frame.m_bounds)
            {
                if (box.m_id == *m_selected)
                {
                    return Gizmo::handles(m_gizmoMode, view, box, transformOf(box.m_id));
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
        for (const Json &object : m_document.data()["objects"])
        {
            if (object.value("id", "") != *id)
            {
                continue;
            }
            for (const Json &component : object["components"])
            {
                if (component.value("type", "") == "Camera" && component.contains("zoom") && component["zoom"].is_number() && component["zoom"].get<double>() > 0)
                {
                    return component["zoom"].get<double>();
                }
            }
        }
        return 1.0;
    }

    DrawList Editor::overlay() const
    {
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
        DrawList list = m_frameView.overlay(m_frame.m_bounds, m_selected, gameCamera, (int)std::lround(logical.first / zoom),
                                            (int)std::lround(logical.second / zoom));
        DrawList handles = Gizmo::draw(handlesFor(m_frameView));
        list.insert(list.end(), handles.begin(), handles.end());
        DrawList anchors = anchorOverlay(logical.first, logical.second);
        list.insert(list.end(), anchors.begin(), anchors.end());
        return list;
    }

    void Editor::documentChanged(const DocumentChange &change)
    {
        if (change.m_kind == DocumentChange::Kind::Field && !change.m_value.is_null() && change.m_field != "enabled")
        {
            if (m_status == Status::Ready)
            {
                m_session.setField(change.m_objectId, change.m_component, change.m_field, change.m_value);
            }
        }
        else
        {
            m_needsLoad = true;
        }
        m_needsRender = true;
    }

    void Editor::cameraChanged()
    {
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
        if (pointer == Pointer::Pan)
        {
            m_drag.m_kind = Drag::Kind::Pan;
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

        std::optional<std::string> hit = Viewport::hit(m_viewport, m_frame.m_bounds, frameX, frameY);
        select(hit);
        if (hit)
        {
            m_drag.m_kind = Drag::Kind::Object;
            m_drag.m_id = *hit;
            m_drag.m_startWorldX = m_viewport.frameToWorldX(frameX);
            m_drag.m_startWorldY = m_viewport.frameToWorldY(frameY);
            TransformValues values = transformOf(*hit);
            m_drag.m_startX = values.m_x;
            m_drag.m_startY = values.m_y;
        }
    }

    Editor::PointerTarget Editor::pointerTarget(double frameX, double frameY) const
    {
        PointerTarget target;
        switch (m_drag.m_kind)
        {
        case Drag::Kind::Pan:
            target.m_kind = PointerTarget::Kind::Pan;
            return target;
        case Drag::Kind::Object:
            target.m_kind = PointerTarget::Kind::Object;
            return target;
        case Drag::Kind::Handle:
            target.m_kind = PointerTarget::Kind::Handle;
            target.m_handle = m_drag.m_handle->kind();
            return target;
        case Drag::Kind::None:
            break;
        }
        if (auto handle = Gizmo::hit(currentHandles(), frameX, frameY))
        {
            target.m_kind = PointerTarget::Kind::Handle;
            target.m_handle = handle->m_kind;
        }
        else if (Viewport::hit(m_viewport, m_frame.m_bounds, frameX, frameY))
        {
            target.m_kind = PointerTarget::Kind::Object;
        }
        return target;
    }

    void Editor::pointerMove(double frameX, double frameY)
    {
        if (m_drag.m_kind == Drag::Kind::Pan)
        {
            m_viewport.panByFrame(frameX - m_drag.m_lastFrameX, frameY - m_drag.m_lastFrameY);
            m_drag.m_lastFrameX = frameX;
            m_drag.m_lastFrameY = frameY;
            cameraChanged();
        }
        else if (m_drag.m_kind == Drag::Kind::Handle)
        {
            setTransformFields(m_drag.m_id, dragFields(m_drag.m_id, m_drag.m_handle->update(m_viewport, frameX, frameY)), m_drag.m_hasMoved);
            m_drag.m_hasMoved = true;
        }
        else if (m_drag.m_kind == Drag::Kind::Object)
        {
            double x = std::round(m_drag.m_startX + m_viewport.frameToWorldX(frameX) - m_drag.m_startWorldX);
            double y = std::round(m_drag.m_startY + m_viewport.frameToWorldY(frameY) - m_drag.m_startWorldY);
            setTransformFields(m_drag.m_id, dragFields(m_drag.m_id, {{"x", x}, {"y", y}}), m_drag.m_hasMoved);
            m_drag.m_hasMoved = true;
        }
    }

    void Editor::pointerUp()
    {
        m_drag = Drag();
    }

    void Editor::wheel(double frameX, double frameY, double steps)
    {
        m_viewport.zoomAt(frameX, frameY, std::pow(1.1, steps));
        cameraChanged();
    }
}
