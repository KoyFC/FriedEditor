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

        // The game running inside the editor, which the keyboard and the mouse go to while it is the window with the focus.
        void gameView();
        // Whether the game's window had the focus when it was last drawn, which is when the keyboard is the game's.
        bool isGameFocused() const { return m_isGameFocused; }
        // Whether the game's window was drawn since this was last asked, which is whether it is on screen for the game to send frames to.
        bool takeGameDrawn();
        // The same for the scene view, which the game's preview and the scene take turns with.
        bool takeSceneDrawn();
        // Gives the focus back to the scene, which is how the keyboard is taken back from the game.
        void releaseGame();

        // Panning holds the pointer still, so the viewport pans by the movement it is given instead of by where the pointer is.
        bool isPanning() const { return m_dragKind >= 2; }
        void setHeldMotion(ImVec2 motion) { m_heldMotion = motion; }

        // Whether a value was being dragged this frame. It is reset by asking.
        bool takeValueDrag();
        void hierarchy();
        void inspector();
        void assets();
        void issues();

        // What the game printed, and what its build did, as it is written.
        void console();
        int consoleErrorCount() const { return m_editor.gameLog().errorCount(); }

        // How many problems the assets have for the platform being previewed, for the tab and the status bar.
        // Looks for the problems now if the list is old, whether or not the tab that shows them is the one open.
        void refreshIssues(bool isForced = false);
        int issueCount() const { return (int)(m_issues.size() + m_sceneIssues.size()); }
        void statusBar();

        // Opens a scene in a tab, or goes to its tab.
        void openSceneFile(const std::string &path);

        // Closes the scene's tab, asking first about its changes. The question is drawn by sceneChangeDialog, wherever the panels are.
        void closeSceneTab(const std::string &path);
        void sceneChangeDialog();

        // Ctrl+P: a field that finds a scene or an asset by a part of its name, and opens it or shows it in the assets.
        void showQuickOpen() { m_isQuickOpenAsked = true; }

        // Asks for a name and the class of the root node of a new scene. The question is drawn by newSceneDialog.
        void showNewScene() { m_isNewSceneAsked = true; }
        void newSceneDialog();
        void quickOpen();

        // The window that shows a build going on, or one that has ended, with what it printed.
        void buildWindow();

        // The project's own settings and, for each platform, its icon, its packaging text and its build.
        void projectSettings(bool *isOpen);
        void playerSettings(bool *isOpen);

        // A file dropped on the editor from outside it, which the settings window takes as an icon when it is open on a platform.
        void fileDropped(const std::string &file);

    private:
        // The icon of a node type: its own, or the nearest class it extends that has one.
        Icon iconOf(const std::string &type);

        // The title of a class in the inspector, with its icon. True while it is open.
        bool classHeader(const std::string &name);
        void enterAssetFolder(const std::string &folder);
        void controlEditor(const fried::editor::ClassView &view);
        void fieldEditor(const fried::editor::FieldView &field);
        // Fields that are the x, y and z of one thing, as one row of small drag fields, each with its axis letter.
        void vectorEditor(const std::string &label, const std::vector<const fried::editor::FieldView *> &axes);
        bool textField(const char *label, std::string *value);
        void projectPage();
        void playerPage();
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
        Texture m_gameFrame;
        unsigned long long m_uploadedGameSerial = 0;
        std::vector<float> m_frameTimes;
        double m_frameTimeSampledAt = 0.0;
        int m_gameScale = 0;
        int m_gameLimit = 0;
        bool m_isStatsShown = false;
        bool m_isGameDrawn = false;
        bool m_isSceneDrawn = false;
        bool m_isTextureThePreview = false;
        unsigned long long m_uploadedPreviewSerial = 0;
        bool m_isGameFocused = false;
        bool m_isGameButtonDown[3] = {false, false, false};
        int m_gameMouseX = -1;
        int m_gameMouseY = -1;
        ImageCache m_images;
        int m_dragKind = 0;
        ImVec2 m_heldMotion;
        ImVec2 m_panPosition;
        int m_assetFilter = 0;
        std::string m_assetFolder;
        bool m_isAssetFolderLoaded = false;
        bool m_isAssetList = false;
        float m_assetCell = 84;
        float m_assetTreeWidth = 190;
        std::vector<std::string> m_folders;
        std::size_t m_foldersKey = 0;
        std::string m_assetSearch;
        std::string m_autoloadFilter;
        std::string m_pendingScene;
        bool m_isSceneChangeAsked = false;
        std::string m_lastActiveScene;
        std::vector<fried::editor::SceneIssue> m_sceneIssues;
        std::string m_issueNode;
        void sceneTabs();
        void instanceMenu(const std::optional<std::string> &parent);
        std::vector<fried::editor::AssetIssue> m_issues;
        double m_issuesCheckedAt = -100;
        fried::editor::Platform m_issuesPlatform = fried::editor::Platform::Pc;
        std::string m_issuesMessage;
        std::map<ImGuiID, std::string> m_textEdits;
        std::unordered_set<std::string> m_collapsed;
        std::string m_collapsedScene;
        std::vector<std::size_t> m_visibleRows;
        bool m_consoleShow[3] = {true, true, true};
        std::string m_consoleSearch;
        std::vector<std::size_t> m_consoleRows;
        unsigned long m_consoleKeyRevision = 0;
        std::string m_consoleKeyText;
        int m_consoleKeyMask = 0;
        bool m_isQuickOpenAsked = false;
        bool m_isNewSceneAsked = false;
        std::string m_newSceneName;
        std::string m_newSceneType = "Node2D";
        std::string m_newSceneError;
        std::string m_quickText;
        int m_quickIndex = 0;
        std::string m_pickSearch;
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
