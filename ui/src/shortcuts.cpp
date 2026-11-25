#include "shortcuts.h"

#include <imgui.h>

namespace editor_ui
{
    namespace
    {
        struct Chord
        {
            ImGuiKey m_key;
            bool m_isCtrl;
            bool m_isShift;
            bool m_isAlt;
        };

        // In the order of the Shortcut enum.
        const Chord s_chords[] = {
            {ImGuiKey_N, true, false, false},          // NewScene
            {ImGuiKey_S, true, false, false},          // Save
            {ImGuiKey_S, true, true, false},           // SaveSceneAs
            {ImGuiKey_S, true, false, true},           // SaveAll
            {ImGuiKey_P, true, false, false},          // QuickOpen
            {ImGuiKey_Q, true, false, false},          // Quit
            {ImGuiKey_B, true, false, false},          // BuildAndRun
            {ImGuiKey_Z, true, false, false},          // Undo
            {ImGuiKey_Z, true, true, false},           // Redo
            {ImGuiKey_Y, true, false, false},          // RedoAlternate
            {ImGuiKey_A, true, false, false},          // CreateNode
            {ImGuiKey_D, true, false, false},          // Duplicate
            {ImGuiKey_C, true, false, false},          // Copy
            {ImGuiKey_V, true, false, false},          // Paste
            {ImGuiKey_Delete, false, false, false},    // Delete
            {ImGuiKey_F2, false, false, false},        // Rename
            {ImGuiKey_W, false, false, false},         // MoveTool
            {ImGuiKey_E, false, false, false},         // RotateTool
            {ImGuiKey_T, false, false, false},         // ScaleTool
            {ImGuiKey_F, false, false, false},         // FrameSelected
            {ImGuiKey_F5, false, false, false},        // PlayProject
            {ImGuiKey_F6, false, false, false},        // PlayScene
            {ImGuiKey_F7, false, false, false},        // Pause
            {ImGuiKey_F8, false, false, false},        // Stop
            {ImGuiKey_F10, false, false, false},       // NextFrame
        };
        constexpr int s_count = sizeof(s_chords) / sizeof(s_chords[0]);
        static_assert(s_count == (int)Shortcut::NextFrame + 1, "every shortcut has a chord, in the order of the enum");
    }

    const char *shortcutText(Shortcut shortcut)
    {
        static std::string s_texts[s_count];
        std::string &text = s_texts[(int)shortcut];
        if (text.empty())
        {
            const Chord &chord = s_chords[(int)shortcut];
            text = std::string(chord.m_isCtrl ? "Ctrl+" : "") + (chord.m_isShift ? "Shift+" : "") + (chord.m_isAlt ? "Alt+" : "") + ImGui::GetKeyName(chord.m_key);
        }
        return text.c_str();
    }

    std::string withShortcut(const char *text, Shortcut shortcut)
    {
        return std::string(text) + " (" + shortcutText(shortcut) + ")";
    }

    bool isShortcutPressed(Shortcut shortcut)
    {
        const Chord &chord = s_chords[(int)shortcut];
        const ImGuiIO &io = ImGui::GetIO();
        return io.KeyCtrl == chord.m_isCtrl && io.KeyShift == chord.m_isShift && io.KeyAlt == chord.m_isAlt && ImGui::IsKeyPressed(chord.m_key, false);
    }
}
