#pragma once

#include "fried/editor/editor.h"
#include "icons.h"
#include "textures.h"

#include <imgui.h>

#include <functional>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace editor_ui
{
    // What a file dragged out of the Assets panel carries: its reference, as "root/path".
    inline constexpr const char *s_assetPayload = "FRIED_ASSET";

    // The panels of the editor. Each draws one window from the editor's models and calls the editor for what the user does.
    class Panels
    {
    public:
        explicit Panels(fried::editor::Editor &editor);

        void toolbar();
        void viewport();

        // Panning holds the pointer still, so the viewport pans by the movement it is given instead of by where the pointer is.
        bool isPanning() const { return m_dragKind >= 2; }
        void setHeldMotion(ImVec2 motion) { m_heldMotion = motion; }

        // Whether a value was being dragged this frame. It is reset by asking.
        bool takeValueDrag();
        void hierarchy();
        void inspector();
        void assets();
        void issues();

        // How many problems the assets have for the platform being previewed, for the tab and the status bar.
        // Looks for the problems now if the list is old, whether or not the tab that shows them is the one open.
        void refreshIssues(bool isForced = false);
        int issueCount() const { return (int)m_issues.size(); }
        void statusBar();

        // The window that shows a build going on, or one that has ended, with what it printed.
        void buildWindow();

        // The project's own settings and, for each platform, its icon, its packaging text and its build.
        void projectSettings(bool *isOpen);

        // A file dropped on the editor from outside it, which the settings window takes as an icon when it is open on a platform.
        void fileDropped(const std::string &file);

    private:
        // The icon of a node type: its own, or the nearest class it extends that has one.
        Icon iconOf(const std::string &type);

        // The title of a class in the inspector, with its icon. True while it is open.
        bool classHeader(const std::string &name);
        void openSceneFile(const std::string &path);
        void sceneChangeDialog();
        void controlEditor(const fried::editor::ClassView &view);
        void fieldEditor(const fried::editor::FieldView &field);
        bool textField(const char *label, std::string *value);
        void projectPage();
        void platformPage(fried::editor::Platform platform);
        void iconSection(fried::editor::Platform platform);
        void packagingSection(fried::editor::Platform platform);
        void buildSection(fried::editor::Platform platform);
        void buildSettingsStrip(fried::editor::Platform platform);
        fried::core::BuildTarget selectedTarget(fried::editor::Platform platform);
        void settingText(const char *label, const std::string &value, int limit, const std::function<std::string(const std::string &)> &set);
        void applyIcon(fried::editor::Platform platform, const std::string &file, bool isForEveryPlatform);
        bool textArea(const char *label, std::string *value);
        bool referenceField(const fried::editor::FieldView &field, bool isFont);

        fried::editor::Editor &m_editor;
        Texture m_frame;
        unsigned long m_uploadedSerial = 0;
        ImageCache m_images;
        int m_dragKind = 0;
        ImVec2 m_heldMotion;
        ImVec2 m_panPosition;
        int m_assetFilter = 0;
        std::string m_assetSearch;
        std::string m_pendingScene;
        bool m_isSceneChangeAsked = false;
        std::vector<fried::editor::AssetIssue> m_issues;
        double m_issuesCheckedAt = -100;
        fried::editor::Platform m_issuesPlatform = fried::editor::Platform::Pc;
        std::string m_issuesMessage;
        std::map<ImGuiID, std::string> m_textEdits;
        std::unordered_set<std::string> m_collapsed;
        std::string m_collapsedScene;
        std::vector<std::size_t> m_visibleRows;
        std::string m_hierarchyFilter;
        std::vector<char> m_filterKeep;
        std::string m_filterKeyText;
        unsigned long m_filterKeyVersion = 0;
        std::map<std::string, Icon> m_typeIcons;
        std::size_t m_typeIconsSchema = 0;
        std::string m_renameId;
        std::string m_renameText;
        bool m_isRenameAsked = false;

        // The settings window: its page (0 is the project, then one per platform), what was wrong with the last value typed into each field, and what the last icon set had to become.
        int m_settingsPage = 0;
        bool m_isSettingsShown = false;
        std::map<std::string, std::string> m_settingErrors;
        std::string m_iconSource;
        std::vector<std::string> m_iconNotes;
        std::string m_iconError;
        std::map<fried::editor::Platform, int> m_buildTargetChoice;
        bool m_isFramerateCustom = false;
        bool m_isPhysicsRateCustom = false;
        std::vector<fried::editor::AssetIssue> m_buildIssues;
        double m_buildIssuesCheckedAt = -100;
        int m_buildIssuesTarget = -1;
        std::string m_buildMessage;
    };
}
