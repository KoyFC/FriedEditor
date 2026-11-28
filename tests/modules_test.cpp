#include "doctest/doctest.h"

#include "fried/editor/modules.h"

using namespace fried::editor;
using fried::core::Platform;

namespace
{
    std::vector<ModuleSchema> sample()
    {
        return modulesFromManifest(nlohmann::json::parse(R"({
            "physics2d": {"default": "kinematic", "backends": {
                "kinematic": {"platforms": "all", "unsupportedFields": {"Body2D": ["bullet"]}},
                "box2d": {"platforms": ["pc", "vita"], "native": "box2d"}}},
            "audio": {"default": "native", "backends": {
                "native": {"platforms": "all", "degraded": {"cg50": "No audio output."}}}},
            "physics3d": {"default": "none", "backends": {
                "none": {"platforms": "all"},
                "jolt": {"platforms": ["pc"], "planned": true}}}
        })"));
    }

    fried::core::Project project()
    {
        fried::core::Project value;
        std::string error;
        REQUIRE(value.parse("/games/sample", "{\"name\": \"Sample\"}", &error));
        return value;
    }
}

TEST_CASE("the manifest gives modules with the platforms each backend runs on")
{
    std::vector<ModuleSchema> modules = sample();
    REQUIRE(modules.size() == 3);
    const ModuleSchema *physics = nullptr;
    for (const ModuleSchema &module : modules)
    {
        if (module.m_name == "physics2d")
        {
            physics = &module;
        }
    }
    REQUIRE(physics != nullptr);
    CHECK(physics->m_default == "kinematic");
    CHECK(physics->backendNamed("kinematic")->runsOn(Platform::NintendoDs));
    CHECK(physics->backendNamed("box2d")->runsOn(Platform::Pc));
    CHECK_FALSE(physics->backendNamed("box2d")->runsOn(Platform::NintendoDs));
    CHECK(physics->refusal("box2d", Platform::NintendoDs) == "box2d does not run on nds.");
    CHECK(physics->refusal("box2d", Platform::Pc).empty());
}

TEST_CASE("a planned backend cannot be chosen")
{
    for (const ModuleSchema &module : sample())
    {
        if (module.m_name == "physics3d")
        {
            CHECK(module.refusal("jolt", Platform::Pc) == "jolt is planned and cannot be chosen yet.");
        }
    }
}

TEST_CASE("a platform's choice beats the project's, which beats the default")
{
    std::vector<ModuleSchema> modules = sample();
    fried::core::Project value = project();
    const ModuleSchema &physics = modules[1].m_name == "physics2d" ? modules[1] : modules[0];
    CHECK(physics.backendFor(value, Platform::Pc) == "kinematic");

    value.setModuleBackend("physics2d", "box2d");
    CHECK(physics.backendFor(value, Platform::Vita) == "box2d");

    value.setModuleBackend(Platform::Vita, "physics2d", "kinematic");
    CHECK(physics.backendFor(value, Platform::Vita) == "kinematic");
    CHECK(physics.backendFor(value, Platform::Pc) == "box2d");
}

TEST_CASE("a component is left out where its backend is not the one in use or only works in part")
{
    std::vector<ModuleSchema> modules = sample();
    fried::core::Project value = project();
    value.setModuleBackend(Platform::Pc, "physics2d", "box2d");

    CHECK(isLeftOut(modules, "physics2d", "kinematic", value, Platform::Pc));
    CHECK_FALSE(isLeftOut(modules, "physics2d", "box2d", value, Platform::Pc));
    CHECK(isLeftOut(modules, "physics2d", "box2d", value, Platform::NintendoDs));
    CHECK_FALSE(isLeftOut(modules, "audio", "native", value, Platform::Pc));
    CHECK(isLeftOut(modules, "audio", "native", value, Platform::Cg50));
    CHECK_FALSE(isLeftOut(modules, "unknown", "x", value, Platform::Pc));
}

TEST_CASE("a setting is ignored on the platforms whose backend does not use it")
{
    std::vector<ModuleSchema> modules = sample();
    fried::core::Project value = project();
    value.setModuleBackend(Platform::Pc, "physics2d", "box2d");
    std::vector<Platform> considered = {Platform::Pc, Platform::NintendoDs, Platform::Vita};

    IgnoredSetting ignored = settingIgnoredOn(modules, value, "Body2D", "bullet", considered);
    CHECK(ignored.m_platforms == std::vector<Platform>{Platform::NintendoDs, Platform::Vita});
    CHECK(ignored.m_module == "physics2d");
    CHECK(ignored.m_backend == "kinematic");
    CHECK(settingIgnoredOn(modules, value, "Body2D", "velocityX", considered).m_platforms.empty());
    CHECK(settingIgnoredOn(modules, value, "Collider2D", "bullet", considered).m_platforms.empty());
}
