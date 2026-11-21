#pragma once

#include <string>

namespace fried::editor
{
    // A field's name as a label for people: the words of a camel case name apart and the first one capitalized, so `scaleX` reads Scale X and `flipH` reads Flip H. An underscore also separates words, each with a capital.
    std::string displayName(const std::string &name);
}
