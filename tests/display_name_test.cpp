#include "doctest/doctest.h"

#include "fried/editor/display_name.h"

using namespace fried::editor;

TEST_CASE("a camel case name becomes words with a capital first one")
{
    CHECK(displayName("speed") == "Speed");
    CHECK(displayName("scaleX") == "Scale X");
    CHECK(displayName("flipH") == "Flip H");
    CHECK(displayName("pivotOffsetY") == "Pivot Offset Y");
    CHECK(displayName("is_visible") == "Is Visible");
    CHECK(displayName("FOVAngle") == "FOV Angle");
    CHECK(displayName("") == "");
}
