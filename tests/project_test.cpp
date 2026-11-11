#include "doctest/doctest.h"

#include <vector>

#include "fried/editor/project.h"

using namespace fried::editor;

TEST_CASE("a project file gives its identity and one display for every platform")
{
    Project project;
    std::string error;
    REQUIRE(project.parse("/games/sample", R"({
        "name": "Sample",
        "organization": "Org",
        "version": "01.00",
        "display": {"mode": "pixel", "width": 640, "height": 480, "filter": "linear"},
        "nds": {"display": {"mode": "default"}}
    })", &error));

    CHECK(project.name() == "Sample");
    CHECK(project.scenesDirectory() == "/games/sample/assets/scenes");
    CHECK(project.display().m_mode == "pixel");
    CHECK(project.display().m_width == 640);
    CHECK(project.display().m_height == 480);
    CHECK(project.display().m_filter == "linear");
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
    auto sizes = [](const char *mode) {
        Project project;
        std::string error;
        REQUIRE(project.parse("/p", std::string(R"({"display": {"mode": ")") + mode + R"(", "width": 640, "height": 480}})", &error));
        return std::vector<std::pair<int, int>>{project.logicalSize(Platform::Pc), project.logicalSize(Platform::NintendoDs),
                                                project.logicalSize(Platform::Nintendo3ds), project.logicalSize(Platform::Vita),
                                                project.logicalSize(Platform::Switch), project.logicalSize(Platform::Cg50)};
    };
    using Size = std::pair<int, int>;
    CHECK(sizes("default") == std::vector<Size>{{640, 480}, {256, 192}, {400, 240}, {960, 544}, {1280, 720}, {396, 224}});
    CHECK(sizes("fit") == std::vector<Size>(6, Size{640, 480}));
    CHECK(sizes("expand") == std::vector<Size>{{640, 480}, {640, 480}, {800, 480}, {847, 480}, {853, 480}, {849, 480}});
    CHECK(sizes("height") == std::vector<Size>{{640, 480}, {640, 480}, {800, 480}, {847, 480}, {853, 480}, {849, 480}});
    CHECK(sizes("pixel") == std::vector<Size>{{640, 480}, {256, 192}, {400, 240}, {960, 544}, {1280, 720}, {396, 224}});
}
