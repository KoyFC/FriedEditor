# Interface conventions

Rules the editor's interface follows, so that a new panel or dialog behaves like the rest. When Godot and Unity agree, the editor does what they do; when they differ, it follows Godot.

- Labels: menu items, buttons, window, dialog and tab titles are in Title Case (short words such as a, the, in, of and to stay lowercase); tooltips, hints and messages are sentences. An item ends in "..." only when it opens something that asks for more before acting.
- Words: one word per action across panels. Delete destroys a node or a file, Remove takes an entry out of a list (an autoload, an instance), Check Again repeats a check, Show in Folder opens the system's file manager, Move Up and Move Down reorder.
- Shortcuts: every shortcut is listed in [Shortcuts](../using/shortcuts.md). Delete, Duplicate, Copy, Paste and Rename act on what the panel with the keyboard shows (nodes in the Hierarchy and the Scene view, files in Assets), so the same key never deletes something in another panel. The other keys work anywhere except while text is typed or a dialog is open, and the play keys stay the editor's while the game has the keyboard.
- Menus and tooltips show the shortcut: a menu item puts it in the shortcut column, a toolbar button ends its tooltip with it in brackets, as in "Move (W)".
- Dialogs: Escape always cancels. Enter confirms when the dialog has a text field and its action can be undone or is harmless; a destructive confirmation needs a click. Buttons go action first, then Cancel, with Discard between them when there is something to lose.
- Destructive actions: what can be undone is not confirmed (nodes); what cannot is confirmed and says where it goes (files go to the system's trash).
- Unsaved changes: a `*` after the name, in tabs, the hierarchy header and the window title.
- New user-facing text is in English, with no em dashes.
