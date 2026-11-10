#pragma once

#include "fried/editor/player_session.h"

#include <optional>
#include <string>
#include <vector>

namespace fried::editor
{
    // What a backend draws over the player's frame, in the frame's own pixels.
    struct DrawCommand
    {
        enum class Kind
        {
            Rect,
            FilledRect,
            Line
        };

        Kind m_kind = Kind::Rect;
        double m_x = 0;
        double m_y = 0;
        double m_width = 0;
        double m_height = 0;
        unsigned int m_rgba = 0xffffffff;
    };

    using DrawList = std::vector<DrawCommand>;

    // The editor's view of the world, with the same maths as the engine's Camera: the camera's position is the middle of the frame.
    class Viewport
    {
    public:
        static constexpr double s_minZoom = 0.05;
        static constexpr double s_maxZoom = 32.0;

        void setFrameSize(int width, int height);
        int frameWidth() const { return m_frameWidth; }
        int frameHeight() const { return m_frameHeight; }

        double cameraX() const { return m_x; }
        double cameraY() const { return m_y; }
        double zoom() const { return m_zoom; }
        void setCamera(double x, double y, double zoom);

        double worldToFrameX(double worldX) const { return (worldX - m_x) * m_zoom + m_frameWidth / 2.0; }
        double worldToFrameY(double worldY) const { return (worldY - m_y) * m_zoom + m_frameHeight / 2.0; }
        double frameToWorldX(double frameX) const { return (frameX - m_frameWidth / 2.0) / m_zoom + m_x; }
        double frameToWorldY(double frameY) const { return (frameY - m_frameHeight / 2.0) / m_zoom + m_y; }

        // Zooms by a factor while the world point under the pointer stays under it.
        void zoomAt(double frameX, double frameY, double factor);
        void panByFrame(double deltaX, double deltaY);

        // The topmost object under a point of the frame, which is the last one drawn.
        static std::optional<std::string> hit(const Viewport &viewport, const std::vector<ObjectBounds> &bounds, double frameX, double frameY);

        // The selection's outline and the frame the game shows, around the scene's camera.
        DrawList overlay(const std::vector<ObjectBounds> &bounds, const std::optional<std::string> &selected,
                         const std::optional<std::pair<double, double>> &gameCamera, int gameWidth, int gameHeight) const;

    private:
        int m_frameWidth = 1;
        int m_frameHeight = 1;
        double m_x = 0;
        double m_y = 0;
        double m_zoom = 1;
    };
}
