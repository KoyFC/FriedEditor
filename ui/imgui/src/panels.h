#pragma once

#include "fried/editor/editor.h"
#include "textures.h"

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
        void hierarchy();
        void inspector();
        void assets();
        void statusBar();

    private:
        void objectFields();
        void fieldEditor(const std::string &type, const fried::editor::FieldView &field);
        bool textField(const char *label, std::string *value);
        bool referenceField(const std::string &type, const fried::editor::FieldView &field, bool isFont);

        fried::editor::Editor &m_editor;
        Texture m_frame;
        unsigned long m_uploadedSerial = 0;
        ImageCache m_images;
        int m_dragKind = 0;
        int m_assetFilter = 0;
        std::map<ImGuiID, std::string> m_textEdits;
    };
}
