#include "fried/editor/viewport.h"

#include <cmath>

#include <algorithm>

namespace fried::editor
{
    namespace
    {
        constexpr unsigned int s_selection = 0xffffc800;
        constexpr unsigned int s_gameFrame = 0xa0ffffff;
        constexpr unsigned int s_grid = 0x30ffffff;
        constexpr unsigned int s_gridMinor = 0x10ffffff;
        constexpr unsigned int s_gridMajor = 0x22ffffff;
    }

    void Viewport::setFrameSize(int width, int height)
    {
        m_frameWidth = std::max(1, width);
        m_frameHeight = std::max(1, height);
    }

    void Viewport::setCamera(double x, double y, double zoom)
    {
        m_x = x;
        m_y = y;
        m_zoom = std::clamp(zoom, s_minZoom, s_maxZoom);
    }

    void Viewport::zoomAt(double frameX, double frameY, double factor)
    {
        double worldX = frameToWorldX(frameX);
        double worldY = frameToWorldY(frameY);
        m_zoom = std::clamp(m_zoom * factor, s_minZoom, s_maxZoom);
        m_x = worldX - (frameX - m_frameWidth / 2.0) / m_zoom;
        m_y = worldY - (frameY - m_frameHeight / 2.0) / m_zoom;
    }

    void Viewport::panByFrame(double deltaX, double deltaY)
    {
        m_x -= deltaX / m_zoom;
        m_y -= deltaY / m_zoom;
    }

    std::optional<std::string> Viewport::hit(const Viewport &viewport, const std::vector<ObjectBounds> &bounds, double frameX, double frameY)
    {
        double worldX = viewport.frameToWorldX(frameX);
        double worldY = viewport.frameToWorldY(frameY);
        for (auto candidate = bounds.rbegin(); candidate != bounds.rend(); ++candidate)
        {
            if (worldX >= candidate->m_x && worldX < candidate->m_x + candidate->m_width && worldY >= candidate->m_y
                && worldY < candidate->m_y + candidate->m_height)
            {
                return candidate->m_id;
            }
        }
        return std::nullopt;
    }

    DrawList Viewport::grid(double step) const
    {
        DrawList list;
        if (step <= 0 || m_zoom <= 0)
        {
            return list;
        }
        constexpr double s_closest = 8;
        while (step * m_zoom < s_closest)
        {
            step *= 2;
        }
        for (int vertical = 0; vertical < 2; ++vertical)
        {
            double low = vertical ? frameToWorldY(0) : frameToWorldX(0);
            double high = vertical ? frameToWorldY(m_frameHeight) : frameToWorldX(m_frameWidth);
            for (long line = (long)std::ceil(low / step); line * step <= high; ++line)
            {
                unsigned int color = line % 4 == 0 ? s_gridMajor : s_gridMinor;
                if (vertical)
                {
                    list.push_back({DrawCommand::Kind::Line, 0, worldToFrameY(line * step), (double)m_frameWidth, 0, color});
                }
                else
                {
                    list.push_back({DrawCommand::Kind::Line, worldToFrameX(line * step), 0, 0, (double)m_frameHeight, color});
                }
            }
        }
        return list;
    }

    DrawList Viewport::overlay(const std::vector<ObjectBounds> &bounds, const std::optional<std::string> &selected,
                               const std::optional<std::pair<double, double>> &gameCamera, int gameWidth, int gameHeight) const
    {
        DrawList list;

        // The world's axes, where the origin is.
        list.push_back({DrawCommand::Kind::Line, worldToFrameX(0), 0, 0, (double)m_frameHeight, s_grid});
        list.push_back({DrawCommand::Kind::Line, 0, worldToFrameY(0), (double)m_frameWidth, 0, s_grid});

        if (gameCamera && gameWidth > 0 && gameHeight > 0)
        {
            list.push_back({DrawCommand::Kind::Rect, worldToFrameX(gameCamera->first - gameWidth / 2.0),
                            worldToFrameY(gameCamera->second - gameHeight / 2.0), gameWidth * m_zoom, gameHeight * m_zoom, s_gameFrame});
        }

        if (selected)
        {
            for (const ObjectBounds &box : bounds)
            {
                if (box.m_id == *selected)
                {
                    list.push_back({DrawCommand::Kind::Rect, worldToFrameX(box.m_x), worldToFrameY(box.m_y), box.m_width * m_zoom, box.m_height * m_zoom,
                                    s_selection});
                }
            }
        }
        return list;
    }
}
