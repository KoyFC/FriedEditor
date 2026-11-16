#pragma once

#include "fried/core/platform.h"
#include "fried/core/process.h"
#include "fried/core/project.h"

// What the editor takes from FriedCore, the library it shares with the Project Manager, under its own names.
namespace fried::editor
{
    using fried::core::DisplaySettings;
    using fried::core::Platform;
    using fried::core::platformKey;
    using fried::core::platformLabel;
    using fried::core::Project;
    using fried::core::s_platforms;
    using fried::core::startSubprocess;
    using fried::core::Subprocess;
    using fried::core::SubprocessOptions;
}
