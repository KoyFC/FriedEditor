#include "fried/editor/project_settings.h"

#include "fried/editor/editor_state.h"

#include "fried/core/icon.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace fried::editor
{
    namespace
    {
        using Json = nlohmann::json;
        using fried::core::BuildTarget;

        const std::string s_sceneSuffix = ".scene.json";

        // Letters as a reader counts them, not bytes, which is how the engine's build measures a line.
        int lengthOf(const std::string &text)
        {
            return (int)std::count_if(text.begin(), text.end(), [](unsigned char byte) { return (byte & 0xc0) != 0x80; });
        }

        bool contains(const std::vector<std::string> &values, const std::string &value)
        {
            return std::find(values.begin(), values.end(), value) != values.end();
        }

        std::string named(const std::string &what, const std::string &problem)
        {
            return problem.empty() ? problem : what + " " + problem;
        }

        std::string settingsKey(BuildTarget target)
        {
            return "build." + fried::core::buildTargetKey(target);
        }
    }

    ProjectSettings::ProjectSettings(Project &project)
        : m_project(project)
    {
    }

    // The change is made on a copy, so a file that cannot be written leaves the project as it was too.
    std::string ProjectSettings::apply(const std::string &problem, const std::function<void(Project &)> &change)
    {
        if (!problem.empty())
        {
            return problem;
        }

        Project changed = m_project;
        change(changed);
        if (changed.text() == m_project.text())
        {
            return std::string();
        }

        std::string error;
        if (!changed.save(&error))
        {
            return error;
        }
        m_project = changed;
        if (m_onSaved)
        {
            m_onSaved();
        }
        return std::string();
    }

    std::string ProjectSettings::setName(const std::string &value)
    {
        return apply(named("The name", Project::checkIdentity(value)), [&](Project &project) { project.setName(value); });
    }

    std::string ProjectSettings::setOrganization(const std::string &value)
    {
        return apply(named("The organization", Project::checkIdentity(value)), [&](Project &project) { project.setOrganization(value); });
    }

    std::string ProjectSettings::setVersion(const std::string &value)
    {
        std::string problem = value.empty() ? "must not be empty." : Project::checkVersion(value);
        return apply(named("The version", problem), [&](Project &project) { project.setVersion(value); });
    }

    std::string ProjectSettings::setWindowTitle(const std::string &value)
    {
        return apply(std::string(), [&](Project &project) { project.setWindowTitle(value); });
    }

    std::string ProjectSettings::setStartScene(const std::string &value)
    {
        bool isKnown = value.empty() || contains(startSceneChoices(), value);
        return apply(isKnown ? std::string() : "The project has no scene named " + value + ".", [&](Project &project) { project.setStartScene(value); });
    }

    std::string ProjectSettings::setVitaTitleId(const std::string &value)
    {
        return apply(named("The title ID", Project::checkVitaTitleId(value)), [&](Project &project) { project.setVitaTitleId(value); });
    }

    std::string ProjectSettings::setVsync(bool value)
    {
        return apply(std::string(), [&](Project &project) { project.setVsync(value); });
    }

    std::string ProjectSettings::setDefaultTargetFramerate(int value)
    {
        return apply(value < 0 ? "The target framerate cannot be negative." : std::string(), [&](Project &project) { project.setDefaultTargetFramerate(value); });
    }

    std::string ProjectSettings::setDisplay(const DisplaySettings &value)
    {
        std::string problem;
        if (!contains(fried::core::displayModes(), value.m_mode))
        {
            problem = "The engine has no display mode named " + value.m_mode + ".";
        }
        else if (!contains(fried::core::displayFilters(), value.m_filter))
        {
            problem = "The engine has no display filter named " + value.m_filter + ".";
        }
        else if (value.m_mode != "default" && (value.m_width <= 0 || value.m_height <= 0))
        {
            problem = "The display's width and height must be above zero in the " + value.m_mode + " mode.";
        }
        return apply(problem, [&](Project &project) { project.setDisplay(value); });
    }

    int ProjectSettings::packagingTextLimit(const std::string &section)
    {
        return section == "3ds" ? Project::s_3dsDescriptionLimit : Project::s_ndsSubtitleLimit;
    }

    std::string ProjectSettings::setPackagingText(const std::string &section, const std::string &key, const std::string &value)
    {
        int limit = packagingTextLimit(section);
        std::string problem = lengthOf(value) > limit ? "This line holds " + std::to_string(limit) + " characters at most." : std::string();
        return apply(problem, [&](Project &project) { project.setPackagingText(section, key, value); });
    }

    std::vector<std::string> ProjectSettings::startSceneChoices() const
    {
        std::vector<std::string> names;
        std::error_code ignored;
        for (const auto &entry : fs::directory_iterator(m_project.scenesDirectory(), ignored))
        {
            std::string name = entry.path().filename().string();
            if (name.size() > s_sceneSuffix.size() && name.compare(name.size() - s_sceneSuffix.size(), s_sceneSuffix.size(), s_sceneSuffix) == 0)
            {
                names.push_back(name.substr(0, name.size() - s_sceneSuffix.size()));
            }
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    bool ProjectSettings::setIcon(Platform platform, const std::string &sourceFile, std::vector<std::string> *notes, std::string *error)
    {
        fried::core::Image source = fried::core::readImage(sourceFile, error);
        if (source.isNull())
        {
            return false;
        }

        fried::core::RenderedIcon icon = fried::core::Icon::render(platform, source, notes);
        if (icon.isNull())
        {
            *error = "The " + platformLabel(platform) + " has no icon.";
            return false;
        }
        return fried::core::Icon::write(m_project.directory(), {icon}, error);
    }

    std::vector<std::string> ProjectSettings::iconFiles(Platform platform) const
    {
        std::vector<std::string> files;
        for (const std::string &path : fried::core::Icon::paths(platform))
        {
            files.push_back(m_project.directory() + "/" + path);
        }
        return files;
    }

    ProjectBuild::ProjectBuild(const Project &project)
        : m_project(project)
    {
        refreshToolchains();
    }

    std::vector<BuildTarget> ProjectBuild::targetsOf(Platform platform)
    {
        std::vector<BuildTarget> targets;
        for (BuildTarget target : fried::core::buildTargets())
        {
            if (fried::core::platformOf(target) == platform)
            {
                targets.push_back(target);
            }
        }
        return targets;
    }

    BuildSettings ProjectBuild::settings(BuildTarget target) const
    {
        auto set = m_settings.find(target);
        if (set != m_settings.end())
        {
            return set->second;
        }

        BuildSettings settings;
        settings.m_directory = fried::core::Build::defaultDirectory(target);
        Json remembered = EditorState::projectValue(m_project.directory(), settingsKey(target));
        if (remembered.is_object())
        {
            settings.m_type = fried::core::buildTypeNamed(remembered.value("type", std::string()));
            settings.m_directory = remembered.value("directory", settings.m_directory);
            settings.m_logToFile = remembered.value("logToFile", false);
        }
        m_settings[target] = settings;
        return settings;
    }

    void ProjectBuild::setSettings(BuildTarget target, const BuildSettings &settings)
    {
        m_settings[target] = settings;
        EditorState::setProjectValue(m_project.directory(), settingsKey(target),
                                     Json{{"type", fried::core::buildTypeName(settings.m_type)}, {"directory", settings.m_directory}, {"logToFile", settings.m_logToFile}});
    }

    // The Emscripten SDK the Project Manager downloads is found where the manager keeps it.
    void ProjectBuild::refreshToolchains()
    {
        fried::core::ManagedTools managed;
        managed.m_emscriptenSdk = fried::core::managerToolsDirectory() + "/emsdk";
        m_toolchains = fried::core::detectToolchains(managed);
    }

    std::string ProjectBuild::status(BuildTarget target) const
    {
        std::string status = fried::core::Build::status(target, m_project.directory(), m_toolchains);
        if (fried::core::Build::needsEmscriptenDownload(target, m_toolchains))
        {
            return "Not ready: there is no Emscripten SDK. The Project Manager downloads one from this platform's page.";
        }
        return status;
    }

    bool ProjectBuild::isReady(BuildTarget target) const
    {
        std::string error;
        return !steps(target, &error).empty();
    }

    std::vector<fried::core::BuildStep> ProjectBuild::steps(BuildTarget target, std::string *error) const
    {
        BuildSettings chosen = settings(target);
        return fried::core::Build::steps(target, chosen.m_type, chosen.m_logToFile, m_project.directory(), chosen.m_directory, m_toolchains, error);
    }

    std::string ProjectBuild::commands(BuildTarget target) const
    {
        std::string error;
        std::string text;
        for (const fried::core::BuildStep &step : steps(target, &error))
        {
            text += (text.empty() ? "" : "\n") + fried::core::Build::commandLine(step);
        }
        return text.empty() ? error : text;
    }

    std::vector<AssetIssue> ProjectBuild::issues(BuildTarget target) const
    {
        return validateAssets(fried::core::platformOf(target), m_project.assetsDirectory());
    }

    bool ProjectBuild::start(BuildTarget target, std::string *error)
    {
        if (isRunning())
        {
            return true;
        }
        std::vector<fried::core::BuildStep> build = steps(target, error);
        if (build.empty())
        {
            return false;
        }
        m_target = target;
        m_startedAt = fs::file_time_type::clock::now();
        m_runner.start(fried::core::subprocessSteps(build, m_project.directory()));
        return true;
    }

    std::string ProjectBuild::builtFile() const
    {
        static const std::vector<std::string> s_packageExtensions = {".exe", ".vpk", ".nro", ".3dsx", ".nds", ".g3a", ".html"};
        fs::path directory = fs::path(m_project.directory()) / settings(m_target).m_directory;
        std::error_code error;
        fs::path newest;
        fs::file_time_type newestTime;
        for (fs::recursive_directory_iterator entry(directory, fs::directory_options::skip_permission_denied, error), end; !error && entry != end; entry.increment(error))
        {
            std::string name = entry->path().filename().string();
            if (entry->is_directory(error))
            {
                if (name == "CMakeFiles" || name == "cpp" || name == "_deps" || entry.depth() >= 2)
                {
                    entry.disable_recursion_pending();
                }
                continue;
            }
            if (!entry->is_regular_file(error))
            {
                continue;
            }
            std::string extension = entry->path().extension().string();
            bool isPackage = contains(s_packageExtensions, extension);
            // A desktop executable on Linux has no extension and sits at the top of the build folder.
            bool isExecutable = extension.empty() && entry.depth() == 0 && (fs::status(entry->path(), error).permissions() & fs::perms::owner_exec) != fs::perms::none;
            if (!isPackage && !isExecutable)
            {
                continue;
            }
            fs::file_time_type time = entry->last_write_time(error);
            if (!error && time >= m_startedAt && (newest.empty() || time > newestTime))
            {
                newest = entry->path();
                newestTime = time;
            }
        }
        return newest.string();
    }

    void ProjectBuild::dismiss()
    {
        if (!isRunning())
        {
            m_runner = fried::core::StepRunner();
        }
    }
}
