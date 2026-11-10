#pragma once

#include "fried/editor/platform.h"

#include <string>
#include <vector>

namespace fried::editor
{
    // A way a project's asset does not suit a platform, and how it can be put right.
    struct AssetIssue
    {
        enum class Severity
        {
            Warning,
            Error
        };

        enum class Fix
        {
            None,
            ShrinkImage,
            DownmixWav
        };

        Severity m_severity = Severity::Warning;

        // "game/sprite.png", or empty for something about the project as a whole.
        std::string m_asset;
        std::string m_message;
        Fix m_fix = Fix::None;
        std::string m_fixLabel;
    };

    // Checks the project's own assets (the engine's are the engine's) against the limits of a platform.
    std::vector<AssetIssue> validateAssets(Platform platform, const std::string &assetsDirectory);

    // Puts an issue right. The file as it was is kept under the backup directory, at the same relative path, so nothing is lost.
    bool applyFix(const AssetIssue &issue, const std::string &assetsDirectory, const std::string &backupDirectory, std::string *error);
}
