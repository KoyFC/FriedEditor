#pragma once

#include <string>

namespace imgui_backend
{
    // A dark theme with a warm accent, and the engine's own font when it can be found.
    void applyStyle(const std::string &fontPath);
}
