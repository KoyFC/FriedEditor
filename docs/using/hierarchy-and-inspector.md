# Hierarchy and inspector

The hierarchy is the scene's tree of nodes: it adds a node of any class under the selected one, deletes it, reorders it, drags it under another node or out of one, changes its type and picks the scene's camera, with a right click menu for each node.

Each row shows an icon for the node's type. A class the engine does not know the icon of, such as one of your own, takes the icon of the nearest class it extends, so a `Sprite` subclass looks like a sprite. Rows alternate in shade, a line marks each level of nesting, the selected row has a bar on its left edge and the scene's camera carries a camera icon on its right. The buttons above the tree add, delete, move up, move down and set the camera, and say so when the pointer rests on them.

Ctrl+D duplicates the selected node with everything under it, right after it, with new ids and a name that none of its siblings has (`Enemy` gives `Enemy2`). Ctrl+C and Ctrl+V copy and paste: the copy goes to the system clipboard as text, so it can be pasted into another scene, even in another editor, and a paste lands next to the selected node, or in the scene when none is selected. All three are in the Edit menu and the node's right click menu, and each is one undo step.

The inspector has one section for each class of the selected node's chain, its own class first and `Node` last, and a node dragged under a rotated or scaled parent moves by the amount in its own coordinates that follows the pointer, and Ctrl+Z and Ctrl+Y undo and redo.

The layout of the panels is docked and is remembered in `layout.ini` in the editor's settings folder (`~/.config/fried-editor/`, or `%APPDATA%\fried-editor` on Windows).

In the inspector each class of the selected node has a title with its icon, and the fields under it are a name on the left and an editor on the right.
