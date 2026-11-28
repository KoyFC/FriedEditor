#include "fried/editor/modules.h"

#include <algorithm>

namespace fried::editor
{
    bool ModuleBackend::runsOn(fried::core::Platform platform) const
    {
        return m_isOnEveryPlatform || std::find(m_platforms.begin(), m_platforms.end(), fried::core::platformKey(platform)) != m_platforms.end();
    }

    std::string ModuleBackend::degradedOn(fried::core::Platform platform) const
    {
        for (const auto &entry : m_degraded)
        {
            if (entry.first == fried::core::platformKey(platform))
            {
                return entry.second;
            }
        }
        return std::string();
    }

    const ModuleBackend *ModuleSchema::backendNamed(const std::string &name) const
    {
        for (const ModuleBackend &backend : m_backends)
        {
            if (backend.m_name == name)
            {
                return &backend;
            }
        }
        return nullptr;
    }

    std::string ModuleSchema::backendFor(const fried::core::Project &project, fried::core::Platform platform) const
    {
        std::string chosen = project.moduleBackend(platform, m_name);
        if (chosen.empty())
        {
            chosen = project.moduleBackend(m_name);
        }
        return chosen.empty() ? m_default : chosen;
    }

    std::string ModuleSchema::refusal(const std::string &backend, fried::core::Platform platform) const
    {
        const ModuleBackend *found = backendNamed(backend);
        if (found == nullptr)
        {
            return m_name + " has no backend named " + backend + ".";
        }
        if (found->m_isPlanned)
        {
            return backend + " is planned and cannot be chosen yet.";
        }
        if (!found->runsOn(platform))
        {
            return backend + " does not run on " + fried::core::platformKey(platform) + ".";
        }
        return std::string();
    }

    std::vector<ModuleSchema> modulesFromManifest(const nlohmann::json &manifest)
    {
        std::vector<ModuleSchema> modules;
        if (!manifest.is_object())
        {
            return modules;
        }
        for (auto module = manifest.begin(); module != manifest.end(); ++module)
        {
            ModuleSchema schema;
            schema.m_name = module.key();
            schema.m_default = module.value().value("default", std::string());
            auto backends = module.value().find("backends");
            if (backends != module.value().end() && backends->is_object())
            {
                for (auto entry = backends->begin(); entry != backends->end(); ++entry)
                {
                    ModuleBackend backend;
                    backend.m_name = entry.key();
                    const nlohmann::json &platforms = entry.value().contains("platforms") ? entry.value()["platforms"] : nlohmann::json();
                    backend.m_isOnEveryPlatform = platforms.is_string() && platforms.get<std::string>() == "all";
                    if (platforms.is_array())
                    {
                        for (const auto &platform : platforms)
                        {
                            if (platform.is_string())
                            {
                                backend.m_platforms.push_back(platform.get<std::string>());
                            }
                        }
                    }
                    backend.m_isPlanned = entry.value().value("planned", false);
                    auto degraded = entry.value().find("degraded");
                    if (degraded != entry.value().end() && degraded->is_object())
                    {
                        for (auto lack = degraded->begin(); lack != degraded->end(); ++lack)
                        {
                            backend.m_degraded.emplace_back(lack.key(), lack.value().is_string() ? lack.value().get<std::string>() : std::string());
                        }
                    }
                    schema.m_backends.push_back(std::move(backend));
                }
            }
            modules.push_back(std::move(schema));
        }
        return modules;
    }

    bool isLeftOut(const std::vector<ModuleSchema> &modules, const std::string &module, const std::string &backend, const fried::core::Project &project,
                   fried::core::Platform platform)
    {
        for (const ModuleSchema &schema : modules)
        {
            if (schema.m_name != module)
            {
                continue;
            }
            if (schema.backendFor(project, platform) != backend)
            {
                return true;
            }
            const ModuleBackend *found = schema.backendNamed(backend);
            return found != nullptr && !found->degradedOn(platform).empty();
        }
        return false;
    }
}
