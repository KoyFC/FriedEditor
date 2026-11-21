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

        // Icons rasterized side by side in one texture, each in a square cell of its own.
        struct Atlas
        {
            Texture m_texture;
            int m_cell = 0;
        };

        Atlas s_small;
        Atlas s_big;
        float s_size = 16.0f;

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

        const char *s_bigNames[] = {
#define FRIED_ICON_NAME(name) #name,
            FRIED_BIG_ICONS(FRIED_ICON_NAME)
#undef FRIED_ICON_NAME
        };

        void build(Atlas &atlas, const char *const *names, int count, int cell)
        {
            atlas.m_cell = std::max(1, cell);
            int rows = (count + s_columns - 1) / s_columns;
            int width = s_columns * atlas.m_cell;
            int height = rows * atlas.m_cell;
            std::vector<unsigned char> pixels((size_t)width * height * 4, 0);
            std::vector<unsigned char> one((size_t)atlas.m_cell * atlas.m_cell * 4);

            NSVGrasterizer *rasterizer = nsvgCreateRasterizer();
            for (int index = 0; index < count; ++index)
            {
                const char *svg = svgOf(names[index]);
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
                std::fill(one.begin(), one.end(), 0);
                nsvgRasterize(rasterizer, image, 0, 0, atlas.m_cell / std::max(image->width, image->height), one.data(), atlas.m_cell, atlas.m_cell, atlas.m_cell * 4);
                nsvgDelete(image);
                int originX = (index % s_columns) * atlas.m_cell;
                int originY = (index / s_columns) * atlas.m_cell;
                for (int row = 0; row < atlas.m_cell; ++row)
                {
                    std::memcpy(&pixels[((size_t)(originY + row) * width + originX) * 4], &one[(size_t)row * atlas.m_cell * 4], (size_t)atlas.m_cell * 4);
                }
            }
            nsvgDeleteRasterizer(rasterizer);
            atlas.m_texture.upload(pixels.data(), width, height);
        }

        void drawFrom(const Atlas &atlas, ImDrawList *list, int index, ImVec2 position, float side, ImU32 tint)
        {
            if (atlas.m_texture.isEmpty())
            {
                return;
            }
            float width = (float)atlas.m_texture.width();
            float height = (float)atlas.m_texture.height();
            float x = (float)((index % s_columns) * atlas.m_cell);
            float y = (float)((index / s_columns) * atlas.m_cell);
            list->AddImage(atlas.m_texture.id(), position, ImVec2(position.x + side, position.y + side), ImVec2(x / width, y / height),
                           ImVec2((x + atlas.m_cell) / width, (y + atlas.m_cell) / height), tint);
        }
    }

    void Icons::rebuild(float scale)
    {
        s_size = std::round(16.0f * scale);
        build(s_small, s_names, (int)Icon::Count, (int)s_size);
        build(s_big, s_bigNames, (int)BigIcon::Count, (int)std::round(64.0f * scale));
    }

    float Icons::size()
    {
        return s_size;
    }

    void Icons::draw(ImDrawList *list, Icon icon, ImVec2 position, ImU32 tint)
    {
        drawFrom(s_small, list, (int)icon, position, s_size, tint);
    }

    void Icons::drawBig(ImDrawList *list, BigIcon icon, ImVec2 position, float side, ImU32 tint)
    {
        drawFrom(s_big, list, (int)icon, position, side, tint);
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
