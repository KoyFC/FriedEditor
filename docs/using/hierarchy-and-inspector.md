# Hierarchy and inspector

The hierarchy is the scene's tree of nodes: it adds a node of any class under the selected one, deletes it, reorders it, drags it under another node or out of one, changes its type and picks the scene's camera, with a right click menu for each node.

Each row shows an icon for the node's type. A class the engine does not know the icon of, such as one of your own, takes the icon of the nearest class it extends, so a `Sprite` subclass looks like a sprite. Rows alternate in shade, a line marks each level of nesting, the selected row has a bar on its left edge and the scene's camera carries a camera icon on its right. The plus button above the tree adds a node under the selected one, and the scene's file name heads the tree, with a star while it has unsaved changes. Delete removes the selected node, and the node's right click menu has Move Up, Move Down and Use as the Camera.

Ctrl+D duplicates the selected node with everything under it, right after it, with new ids and a name that none of its siblings has (`Enemy` gives `Enemy2`). Ctrl+C and Ctrl+V copy and paste: the copy goes to the system clipboard as text, so it can be pasted into another scene, even in another editor, and a paste lands next to the selected node, or in the scene when none is selected. All three are in the Edit menu and the node's right click menu, and each is one undo step.

The field beside the plus button filters the tree: it keeps the nodes whose name or type contains the text, in blue, and the nodes above them so each is seen in its place. Folds are ignored while there is text in it, and clearing it brings the tree back as it was.

A padlock at the end of a row locks the node: it shows when the pointer is over the row, and stays when the node is locked. A locked node is not picked by a click in the view, has no handles and is not moved there, so a background or a floor can be left alone while the nodes over it are worked on; it is still chosen in the hierarchy and edited in the inspector. The locks belong to the editor and are remembered for each scene, and the scene file does not have them, so the game is not changed by them.

The inspector has one section for each class of the selected node's chain, its own class first and `Node` last, and a node dragged under a rotated or scaled parent moves by the amount in its own coordinates that follows the pointer, and Ctrl+Z and Ctrl+Y undo and redo.

The layout of the panels is docked and is remembered in `layout.ini` in the editor's settings folder (`~/.config/fried-editor/`, or `%APPDATA%\fried-editor` on Windows).

In the inspector each class of the selected node has a title with its icon, and the fields under it are a name on the left and an editor on the right.

A field that holds a file, such as the texture of a sprite or the font of a label, can be filled three ways: by typing the reference (`game/sprite.png`, and `engine/NunitoSans.ttf@16` for a font with its size), by dragging the file from the Assets tab onto it, or with the magnifier button at its end. The button opens a list of the project's files of the kind the field takes, images for a texture and fonts for a font, with a thumbnail for each, a search field and a `None` entry that clears the field. The one in use is marked, and choosing a font keeps its size.
