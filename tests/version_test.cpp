#include "doctest/doctest.h"

#include "fried/editor/version.h"

TEST_CASE("the version is the one the project declares")
{
    CHECK(fried::editor::version() == "0.1.0");
}
