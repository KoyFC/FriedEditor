#pragma once

#include <imgui.h>

namespace editor_ui
{
    // The SVG of an icon, by the name of its file, as the build compiles them in.
    struct IconSource
    {
        const char *m_name;
        const char *m_svg;
    };
    extern const IconSource s_iconSources[];
    extern const int s_iconSourceCount;

    // Every icon the editor draws, named after the file in ui/icons.
#define FRIED_ICONS(X) \
    X(Node) X(Node2D) X(Node3D) X(Sprite2D) X(Camera2D) X(Camera3D) X(CollisionShape2D) X(CharacterBody2D) X(MeshInstance3D) X(DirectionalLight3D) \
    X(Control) X(Label) X(Button) X(TextureRect) X(Play) X(Stop) X(ToolMove) X(ToolRotate) X(ToolScale) X(Add) X(Remove) X(MoveUp) X(MoveDown) X(Search) X(Grid) X(Snap) X(Folder) X(File) X(Image) X(Font) X(AudioStreamWAV) X(PackedScene) X(FileList) X(CenterView) X(ZoomReset) X(Lock) X(Unlock)

    // The icons that are drawn large, as the thumbnails of a folder and a file, and so are rasterized at their own size.
#define FRIED_BIG_ICONS(X) X(FolderBigThumb) X(FileBigThumb)

    enum class Icon
    {
#define FRIED_ICON_ENTRY(name) name,
        FRIED_ICONS(FRIED_ICON_ENTRY)
#undef FRIED_ICON_ENTRY
        Count
    };

    enum class BigIcon
    {
#define FRIED_BIG_ICON_ENTRY(name) name,
        FRIED_BIG_ICONS(FRIED_BIG_ICON_ENTRY)
#undef FRIED_BIG_ICON_ENTRY
        Count
    };

    // The icons rasterized once into a single texture at the size the interface is shown at, so drawing one costs a textured quad.
    namespace Icons
    {
        // Draws every icon again for a scale. Called when the scale changes, never per frame.
        void rebuild(float scale);

        // The side of an icon on screen.
        float size();

        void draw(ImDrawList *list, Icon icon, ImVec2 position, ImU32 tint = IM_COL32_WHITE);

        // A large icon, drawn in a square of the given side.
        void drawBig(ImDrawList *list, BigIcon icon, ImVec2 position, float side, ImU32 tint = IM_COL32_WHITE);

        // A square button with the icon, which is lit while it is active.
        bool button(const char *id, Icon icon, const char *tooltip, bool isActive = false);
    }
}
