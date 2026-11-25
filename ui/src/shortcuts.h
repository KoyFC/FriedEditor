#pragma once

#include <string>

namespace editor_ui
{
    // Every shortcut the editor has that is not local to one widget, defined once in shortcuts.cpp. The key handling asks whether one was pressed, and menus and tooltips ask for its text, so a key can only be changed in one place.
    enum class Shortcut
    {
        NewScene,
        Save,
        SaveSceneAs,
        SaveAll,
        QuickOpen,
        Quit,
        BuildAndRun,
        Undo,
        Redo,
        RedoAlternate,
        CreateNode,
        Duplicate,
        Copy,
        Paste,
        Delete,
        Rename,
        MoveTool,
        RotateTool,
        ScaleTool,
        FrameSelected,
        PlayProject,
        PlayScene,
        Pause,
        Stop,
        NextFrame,
    };

    // The chord as a menu shows it, such as "Ctrl+Shift+S".
    const char *shortcutText(Shortcut shortcut);
    // A tooltip with its shortcut after it, such as "Move (W)".
    std::string withShortcut(const char *text, Shortcut shortcut);
    // True on the frame the chord goes down, with exactly its modifiers held.
    bool isShortcutPressed(Shortcut shortcut);
}
