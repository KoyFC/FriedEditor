#pragma once

#include <string>

namespace fried::editor
{
    // Moves a file or a folder to the system's trash, where a file manager can restore it from. False with the reason in error when it cannot, and then nothing is removed.
    bool moveToTrash(const std::string &path, std::string *error);
}
