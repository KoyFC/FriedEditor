#pragma once

#include "fried/editor/editor.h"

namespace editor_ui
{
    // Opens the window and runs the editor until it is closed. Returns the process's exit code.
    int run(fried::editor::Editor &editor);
}
