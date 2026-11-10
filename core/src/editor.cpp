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
        else if (!m_document.load(scenes.front(), error))
        {
            return false;
        }
        else
        {
            m_scenePath = scenes.front();
        }

        m_commands = std::make_unique<CommandStack>(m_document);
        m_commands->m_onChange = [this] { changed(); };
        m_document.m_onChange = [this](const DocumentChange &change) { documentChanged(change); };
        m_viewport.setFrameSize(s_frameWidth, s_frameHeight);
        m_viewport.setCamera(s_frameWidth / 2.0, s_frameHeight / 2.0, 1.0);

        if (!fs::exists(playerExecutablePath(m_enginePath)))
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

    void Editor::rebuildPlayer()
    {
        m_builder = std::make_unique<PlayerBuilder>(PlayerBuilder::engineSteps(m_enginePath, playerLogFile()));
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
        if (!m_session.launch(playerExecutablePath(m_enginePath), m_enginePath, m_project.directory(), s_frameWidth, s_frameHeight, &error))
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
        }
        changed();
    }

    void Editor::stopGame()
    {
        m_game.stop();
        m_message = "Stopped the game";
        changed();
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
        if (m_needsLoad)
        {
            m_needsLoad = false;
            m_session.load(m_document.data(), m_platform == Platform::Pc ? "" : platformKey(m_platform));
            m_needsRender = true;
        }
        if (m_needsCamera)
        {
            m_needsCamera = false;
            m_session.setEditorCamera(m_viewport.cameraX(), m_viewport.cameraY(), m_viewport.zoom());
            m_needsRender = true;
        }
        if (m_needsRender && !m_isRenderInFlight)
        {
            m_needsRender = false;
            m_isRenderInFlight = true;
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
            for (const FieldSchema &field : schema->m_fields)
            {
                FieldView fieldView;
                fieldView.m_schema = field;
                fieldView.m_isSet = component.contains(field.m_name);
                fieldView.m_value = fieldView.m_isSet ? component[field.m_name] : field.m_default;
                if (m_platform != Platform::Pc)
                {
                    if (auto overridden = m_document.overrideField(platformKey(m_platform), *m_selected, type, field.m_name))
                    {
                        fieldView.m_value = *overridden;
                        fieldView.m_isOverridden = true;
                    }
                }
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
            if (!has)
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
        if (m_platform == Platform::Pc || field == "enabled")
        {
            m_commands->execute(std::make_unique<SetComponentField>(*m_selected, type, field, value), true);
        }
        else
        {
            m_commands->execute(std::make_unique<SetOverrideField>(platformKey(m_platform), *m_selected, type, field, value), true);
        }
    }

    void Editor::clearOverride(const std::string &type, const std::string &field)
    {
        if (m_selected && m_platform != Platform::Pc)
        {
            m_commands->execute(std::make_unique<SetOverrideField>(platformKey(m_platform), *m_selected, type, field, std::nullopt));
        }
    }

    void Editor::setTransformFields(const std::string &id, const std::map<std::string, double> &fields, bool merge)
    {
        if (m_platform == Platform::Pc)
        {
            m_commands->execute(std::make_unique<SetTransformFields>(id, fields), merge);
        }
        else
        {
            m_commands->execute(std::make_unique<SetOverrideTransformFields>(platformKey(m_platform), id, fields), merge);
        }
    }

    void Editor::addComponent(const std::string &type)
    {
        if (m_selected)
        {
            const Json *object = m_document.object(*m_selected);
            m_commands->execute(std::make_unique<AddComponent>(*m_selected, (int)(*object)["components"].size(), newComponent(type)));
        }
    }

    void Editor::removeComponent(const std::string &type)
    {
        if (m_selected)
        {
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
            std::optional<Json> value = m_platform != Platform::Pc ? m_document.overrideField(platformKey(m_platform), id, "Transform", field) : std::nullopt;
            if (!value)
            {
                value = m_document.componentField(id, "Transform", field);
            }
            return value && value->is_number() ? value->get<double>() : fallback;
        };
        return {number("x", 0), number("y", 0), number("rotation", 0), number("scaleX", 1), number("scaleY", 1)};
    }

    std::vector<GizmoHandle> Editor::currentHandles() const
    {
        if (m_selected)
        {
            for (const ObjectBounds &box : m_frame.m_bounds)
            {
                if (box.m_id == *m_selected)
                {
                    return Gizmo::handles(m_gizmoMode, m_viewport, box, transformOf(box.m_id));
                }
            }
        }
        return {};
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
        DisplaySettings display = m_project.display(m_platform);
        DrawList list = m_viewport.overlay(m_frame.m_bounds, m_selected, gameCamera, display.m_width, display.m_height);
        DrawList handles = Gizmo::draw(currentHandles());
        list.insert(list.end(), handles.begin(), handles.end());
        return list;
    }

    void Editor::documentChanged(const DocumentChange &change)
    {
        if (change.m_kind == DocumentChange::Kind::Field && !change.m_value.is_null() && m_platform == Platform::Pc && change.m_field != "enabled")
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
            setTransformFields(m_drag.m_id, m_drag.m_handle->update(m_viewport, frameX, frameY), m_drag.m_hasMoved);
            m_drag.m_hasMoved = true;
        }
        else if (m_drag.m_kind == Drag::Kind::Object)
        {
            double x = std::round(m_drag.m_startX + m_viewport.frameToWorldX(frameX) - m_drag.m_startWorldX);
            double y = std::round(m_drag.m_startY + m_viewport.frameToWorldY(frameY) - m_drag.m_startWorldY);
            setTransformFields(m_drag.m_id, {{"x", x}, {"y", y}}, m_drag.m_hasMoved);
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
