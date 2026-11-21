#include "icons.h"

#include "textures.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The copy bundled with SDL_image qualifies its sort callback with SDL's calling convention macro, which is nothing here.
#define SDLCALL
#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace editor_ui
{
    namespace
    {
        constexpr int s_columns = 8;
        constexpr float s_designSize = 16.0f;

        Texture s_atlas;
        int s_cell = 0;
        float s_size = s_designSize;

        const char *svgOf(const char *name)
        {
            for (int index = 0; index < s_iconSourceCount; ++index)
            {
                if (std::strcmp(s_iconSources[index].m_name, name) == 0)
                {
                    return s_iconSources[index].m_svg;
                }
            }
            return nullptr;
        }

        const char *s_names[] = {
#define FRIED_ICON_NAME(name) #name,
            FRIED_ICONS(FRIED_ICON_NAME)
#undef FRIED_ICON_NAME
        };
    }

    void Icons::rebuild(float scale)
    {
        s_size = std::round(s_designSize * scale);
        s_cell = std::max(1, (int)s_size);
        int count = (int)Icon::Count;
        int rows = (count + s_columns - 1) / s_columns;
        int width = s_columns * s_cell;
        int height = rows * s_cell;
        std::vector<unsigned char> pixels((size_t)width * height * 4, 0);
        std::vector<unsigned char> cell((size_t)s_cell * s_cell * 4);

        NSVGrasterizer *rasterizer = nsvgCreateRasterizer();
        for (int index = 0; index < count; ++index)
        {
            const char *svg = svgOf(s_names[index]);
            if (!svg)
            {
                continue;
            }
            std::string copy(svg);
            NSVGimage *image = nsvgParse(copy.data(), "px", 96.0f);
            if (!image)
            {
                continue;
            }
            std::fill(cell.begin(), cell.end(), 0);
            nsvgRasterize(rasterizer, image, 0, 0, s_cell / s_designSize, cell.data(), s_cell, s_cell, s_cell * 4);
            nsvgDelete(image);
            int originX = (index % s_columns) * s_cell;
            int originY = (index / s_columns) * s_cell;
            for (int row = 0; row < s_cell; ++row)
            {
                std::memcpy(&pixels[((size_t)(originY + row) * width + originX) * 4], &cell[(size_t)row * s_cell * 4], (size_t)s_cell * 4);
            }
        }
        nsvgDeleteRasterizer(rasterizer);
        s_atlas.upload(pixels.data(), width, height);
    }

    float Icons::size()
    {
        return s_size;
    }

    void Icons::draw(ImDrawList *list, Icon icon, ImVec2 position, ImU32 tint)
    {
        if (s_atlas.isEmpty())
        {
            return;
        }
        int index = (int)icon;
        float width = (float)s_atlas.width();
        float height = (float)s_atlas.height();
        float x = (float)((index % s_columns) * s_cell);
        float y = (float)((index / s_columns) * s_cell);
        list->AddImage(s_atlas.id(), position, ImVec2(position.x + s_size, position.y + s_size), ImVec2(x / width, y / height), ImVec2((x + s_cell) / width, (y + s_cell) / height), tint);
    }

    bool Icons::button(const char *id, Icon icon, const char *tooltip, bool isActive)
    {
        ImGuiStyle &style = ImGui::GetStyle();
        float side = s_size + style.FramePadding.y * 2;
        ImGui::PushID(id);
        ImVec2 position = ImGui::GetCursorScreenPos();
        bool isPressed = ImGui::InvisibleButton("##icon", ImVec2(side, side));
        bool isHovered = ImGui::IsItemHovered();
        ImDrawList *list = ImGui::GetWindowDrawList();
        ImU32 fill = ImGui::GetColorU32(ImGui::IsItemActive() ? ImGuiCol_ButtonActive : isActive ? ImGuiCol_ButtonActive : isHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
        list->AddRectFilled(position, ImVec2(position.x + side, position.y + side), fill, style.FrameRounding);
        ImVec4 color = isActive ? ImVec4(1.00f, 0.78f, 0.24f, 1.00f) : ImVec4(1, 1, 1, 1);
        color.w *= style.Alpha;
        draw(list, icon, ImVec2(position.x + (side - s_size) / 2, position.y + (side - s_size) / 2), ImGui::GetColorU32(color));
        if (isHovered && tooltip && *tooltip)
        {
            ImGui::SetTooltip("%s", tooltip);
        }
        ImGui::PopID();
        return isPressed;
    }
}
