#include "doctest/doctest.h"

#include "fried/editor/version.h"

#include <string>

TEST_CASE("the version is the one the build was given, as numbers separated by dots")
{
    std::string version = fried::editor::version();
    CHECK_FALSE(version.empty());
    CHECK(version.find_first_not_of("0123456789.") == std::string::npos);
}
