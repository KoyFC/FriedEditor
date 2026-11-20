# Hierarchy and inspector

The hierarchy is the scene's tree of nodes: it adds a node of any class under the selected one, deletes it, reorders it, drags it under another node or out of one, changes its type and picks the scene's camera, with a right click menu for each node.

The inspector has one section for each class of the selected node's chain, its own class first and `Node` last, and a node dragged under a rotated or scaled parent moves by the amount in its own coordinates that follows the pointer, and Ctrl+Z and Ctrl+Y undo and redo.

The layout of the panels is docked and is remembered in `layout.ini` in the editor's settings folder (`~/.config/fried-editor/`, or `%APPDATA%\fried-editor` on Windows).
