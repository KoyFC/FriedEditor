#pragma once

#include "fried/editor/core_types.h"
#include "fried/editor/core_types.h"

#include "fried/core/build.h"
#include "fried/core/step_runner.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace fried::editor
{
    // Edits a project's project.fried. Every change that passes its check is written to the file at once, so the project is never ahead of what is on disk and there is nothing to apply.
    class ProjectSettings
    {
    public:
        explicit ProjectSettings(Project &project);

        // Each returns what is wrong with the value, in a sentence, or an empty string once it is saved. A refused value leaves the project and its file as they were.
        std::string setName(const std::string &value);
        std::string setOrganization(const std::string &value);
        std::string setVersion(const std::string &value);
        std::string setWindowTitle(const std::string &value);
        std::string setStartScene(const std::string &value);
        std::string setVitaTitleId(const std::string &value);
        std::string setDisplay(const DisplaySettings &value);

        // The 3DS's description and the DS's two subtitles, which are the only lines of packaging text there are.
        std::string setPackagingText(const std::string &section, const std::string &key, const std::string &value);
        static int packagingTextLimit(const std::string &section);

        // The project's scenes by the name the engine gives them, which is what "startScene" holds.
        std::vector<std::string> startSceneChoices() const;

        // Makes the platform's icon files from an image. False with the reason in error, and otherwise notes says what the image had to become.
        bool setIcon(Platform platform, const std::string &sourceFile, std::vector<std::string> *notes, std::string *error);

        // The platform's icon files as they are on disk now, whether or not they exist.
        std::vector<std::string> iconFiles(Platform platform) const;

        // Called after every save, so whatever shows the project follows it.
        std::function<void()> m_onSaved;

    private:
        std::string apply(const std::string &problem, const std::function<void(Project &)> &change);

        Project &m_project;
    };

    // How one target of a project is built, which is remembered for each project.
    struct BuildSettings
    {
        fried::core::BuildType m_type = fried::core::BuildType::Debug;

        // Relative to the project's folder.
        std::string m_directory;
        bool m_logToFile = false;
    };

    // Builds a project for a platform, with the steps the Project Manager's build runs.
    class ProjectBuild
    {
    public:
        using State = fried::core::StepRunner::State;

        explicit ProjectBuild(const Project &project);

        // The targets whose game runs on a platform: one, except the PC, which is also built for Windows from another system.
        static std::vector<fried::core::BuildTarget> targetsOf(Platform platform);

        BuildSettings settings(fried::core::BuildTarget target) const;
        void setSettings(fried::core::BuildTarget target, const BuildSettings &settings);

        // Looks the machine over again, for a toolchain installed while the editor was open.
        void refreshToolchains();

        // "Ready", or why the target cannot be built on this machine.
        std::string status(fried::core::BuildTarget target) const;
        bool isReady(fried::core::BuildTarget target) const;

        // The commands a build of the target runs, one a line, the way a shell takes them.
        std::string commands(fried::core::BuildTarget target) const;

        // What is wrong with the project's assets for the platform the target runs on.
        std::vector<AssetIssue> issues(fried::core::BuildTarget target) const;

        // False when the target cannot be built, with the reason in error. Does nothing while a build is running.
        bool start(fried::core::BuildTarget target, std::string *error);
        State pump() { return m_runner.pump(); }
        void cancel() { m_runner.cancel(); }

        // Forgets a build that has ended, so it is no longer shown.
        void dismiss();

        State state() const { return m_runner.state(); }
        bool isRunning() const { return m_runner.state() == State::Running; }
        fried::core::BuildTarget target() const { return m_target; }
        const fried::core::StepRunner &runner() const { return m_runner; }

    private:
        std::vector<fried::core::BuildStep> steps(fried::core::BuildTarget target, std::string *error) const;

        const Project &m_project;
        fried::core::Toolchains m_toolchains;
        fried::core::StepRunner m_runner;
        fried::core::BuildTarget m_target = fried::core::BuildTarget::Pc;

        // What was read or set this run, so the remembered file is read once a target, and which also serves where nothing is remembered between runs.
        mutable std::map<fried::core::BuildTarget, BuildSettings> m_settings;
    };
}
