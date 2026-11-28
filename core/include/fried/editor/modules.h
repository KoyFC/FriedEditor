#pragma once

#include "fried/core/platform.h"
#include "fried/core/project.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace fried::editor
{
    // One way of doing what a module does, and the platforms it runs on.
    struct ModuleBackend
    {
        std::string m_name;
        bool m_isOnEveryPlatform = false;
        std::vector<std::string> m_platforms;
        // Planned backends are listed and cannot be chosen yet.
        bool m_isPlanned = false;
        // The settings of a component the backend does not use, as component and field.
        std::vector<std::pair<std::string, std::string>> m_unsupported;
        // Where the backend only works in part, the platform and what is missing.
        std::vector<std::pair<std::string, std::string>> m_degraded;

        bool runsOn(fried::core::Platform platform) const;
        bool ignores(const std::string &component, const std::string &field) const;
        // The text saying what a platform lacks, empty when it lacks nothing.
        std::string degradedOn(fried::core::Platform platform) const;
    };

    struct ModuleSchema
    {
        std::string m_name;
        std::string m_default;
        std::vector<ModuleBackend> m_backends;

        const ModuleBackend *backendNamed(const std::string &name) const;
        // What a project makes of the module on a platform: the platform's choice, else the project's, else the default.
        std::string backendFor(const fried::core::Project &project, fried::core::Platform platform) const;
        // The backend a platform gets from the project-wide choice: that choice where the platform runs it, else the default.
        std::string inheritedBackend(const fried::core::Project &project, fried::core::Platform platform) const;
        // Why a backend cannot be chosen for a platform, empty when it can.
        std::string refusal(const std::string &backend, fried::core::Platform platform) const;
    };

    // The platforms, among those given, where the backend the project uses does not use a setting of a component, and the module and backend that say so.
    struct IgnoredSetting
    {
        std::vector<fried::core::Platform> m_platforms;
        std::string m_module;
        std::string m_backend;
    };
    IgnoredSetting settingIgnoredOn(const std::vector<ModuleSchema> &modules, const fried::core::Project &project, const std::string &component,
                                    const std::string &field, const std::vector<fried::core::Platform> &considered);

    // The manifest of modules the player sends with its schema, which is the manifest of the engine it was built from.
    // What is wrong with the modules a platform gets: a project-wide choice it cannot run, which it replaces with the module's default, or a choice of its own it cannot run.
    std::vector<std::string> moduleProblems(const std::vector<ModuleSchema> &modules, const fried::core::Project &project, fried::core::Platform platform);

    std::vector<ModuleSchema> modulesFromManifest(const nlohmann::json &manifest);

    // Whether a component of a backend is left out of the platform's build: the platform uses another backend of the module, or one that only works in part there.
    bool isLeftOut(const std::vector<ModuleSchema> &modules, const std::string &module, const std::string &backend, const fried::core::Project &project,
                   fried::core::Platform platform);
}
