#pragma once

#include "fried/editor/editor.h"
#include "textures.h"

#include <imgui.h>

#include <string>

namespace imgui_backend
{
    // The panels of the editor. Each draws one window from the editor's models and calls the editor for what the user does.
    class Panels
    {
    public:
        explicit Panels(fried::editor::Editor &editor);

        void toolbar();
        void viewport();

        // Panning holds the pointer still, so the viewport pans by the movement it is given instead of by where the pointer is.
        bool isPanning() const { return m_dragKind == 2; }
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

        // The window that shows a build going on, or one that failed, with the end of its log.
        void buildWindow();

    private:
        void openSceneFile(const std::string &path);
        void sceneChangeDialog();
        void rectTransformEditor(const fried::editor::ComponentView &view);
        void objectFields();
        void fieldEditor(const std::string &type, const fried::editor::FieldView &field);
        bool textField(const char *label, std::string *value);
        bool textArea(const char *label, std::string *value);
        bool referenceField(const std::string &type, const fried::editor::FieldView &field, bool isFont);

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
    };
}
