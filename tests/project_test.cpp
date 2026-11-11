#include "doctest/doctest.h"

#include "fried/editor/project.h"

using namespace fried::editor;

TEST_CASE("a project file gives its identity and a display per platform")
{
    Project project;
    std::string error;
    REQUIRE(project.parse("/games/sample", R"({
        "name": "Sample",
        "organization": "Org",
        "version": "01.00",
        "display": {"mode": "expand", "width": 640, "height": 480},
        "nds": {"display": {"mode": "default"}},
        "vita": {"display": {"filter": "linear"}}
    })", &error));

    CHECK(project.name() == "Sample");
    CHECK(project.scenesDirectory() == "/games/sample/assets/scenes");

    DisplaySettings pc = project.display(Platform::Pc);
    CHECK(pc.m_mode == "expand");
    CHECK(pc.m_width == 640);
    CHECK(pc.m_height == 480);

    CHECK(project.display(Platform::NintendoDs).m_mode == "default");

    DisplaySettings vita = project.display(Platform::Vita);
    CHECK(vita.m_mode == "expand");
    CHECK(vita.m_filter == "linear");
    CHECK(vita.m_width == 640);
}

TEST_CASE("a file that is not a project is refused with a reason")
{
    Project project;
    std::string error;
    CHECK_FALSE(project.parse("/games/sample", "[1, 2]", &error));
    CHECK_FALSE(error.empty());
    CHECK_FALSE(project.load("/nonexistent-directory", &error));
}

TEST_CASE("the camera area follows the platform's screen and the display mode")
{
    Project project;
    std::string error;
    REQUIRE(project.parse("/p", R"({"display": {"mode": "expand", "width": 640, "height": 480}, "nds": {"display": {"mode": "default"}}, "3ds": {"display": {"mode": "fit"}}})", &error));
    CHECK(project.logicalSize(Platform::Pc) == std::make_pair(640, 480));
    CHECK(project.logicalSize(Platform::NintendoDs) == std::make_pair(256, 192));
    CHECK(project.logicalSize(Platform::Nintendo3ds) == std::make_pair(640, 480));
    CHECK(project.logicalSize(Platform::Vita) == std::make_pair(847, 480));
}
