#include "fried/editor/gizmo.h"

#include <algorithm>
#include <cmath>

namespace fried::editor
{
    namespace
    {
        constexpr double s_pi = 3.14159265358979323846;
        constexpr unsigned int s_red = 0xffe05050;
        constexpr unsigned int s_green = 0xff50d060;
        constexpr unsigned int s_blue = 0xff5090ff;
        constexpr unsigned int s_ring = 0xffffd24a;

        double roundTo(double value, double step)
        {
            return std::round(value / step) * step;
        }
    }

    std::vector<GizmoHandle> Gizmo::handles(GizmoMode mode, const Viewport &viewport, const ObjectBounds &bounds, const TransformValues &transform)
    {
        using Kind = GizmoHandle::Kind;
        std::vector<GizmoHandle> list;

        if (mode == GizmoMode::Move)
        {
            double originX = viewport.worldToFrameX(transform.m_x);
            double originY = viewport.worldToFrameY(transform.m_y);
            list.push_back({Kind::MoveX, originX + s_arrowLength, originY, s_handleHalfSize, originX, originY});
            list.push_back({Kind::MoveY, originX, originY + s_arrowLength, s_handleHalfSize, originX, originY});
        }
        else if (mode == GizmoMode::Scale)
        {
            double left = viewport.worldToFrameX(bounds.m_x);
            double top = viewport.worldToFrameY(bounds.m_y);
            double right = viewport.worldToFrameX(bounds.m_x + bounds.m_width);
            double bottom = viewport.worldToFrameY(bounds.m_y + bounds.m_height);
            list.push_back({Kind::ScaleX, right, (top + bottom) / 2, s_handleHalfSize, left, top});
            list.push_back({Kind::ScaleY, (left + right) / 2, bottom, s_handleHalfSize, left, top});
            list.push_back({Kind::ScaleBoth, right, bottom, s_handleHalfSize, left, top});
        }
        else
        {
            double centerX = viewport.worldToFrameX(bounds.m_x + bounds.m_width / 2);
            double centerY = viewport.worldToFrameY(bounds.m_y + bounds.m_height / 2);
            double radius = std::max(40.0, 0.5 * std::hypot(bounds.m_width, bounds.m_height) * viewport.zoom() + 16);
            list.push_back({Kind::Rotate, centerX, centerY, radius, centerX, centerY});
        }
        return list;
    }

    std::optional<GizmoHandle> Gizmo::hit(const std::vector<GizmoHandle> &handles, double frameX, double frameY)
    {
        for (const GizmoHandle &handle : handles)
        {
            if (handle.m_kind == GizmoHandle::Kind::Rotate)
            {
                if (std::abs(std::hypot(frameX - handle.m_x, frameY - handle.m_y) - handle.m_size) <= s_ringTolerance)
                {
                    return handle;
                }
            }
            else if (std::abs(frameX - handle.m_x) <= handle.m_size + 2 && std::abs(frameY - handle.m_y) <= handle.m_size + 2)
            {
                return handle;
            }
        }
        return std::nullopt;
    }

    DrawList Gizmo::draw(const std::vector<GizmoHandle> &handles)
    {
        using Kind = GizmoHandle::Kind;
        DrawList list;
        for (const GizmoHandle &handle : handles)
        {
            switch (handle.m_kind)
            {
            case Kind::MoveX:
            case Kind::MoveY:
            {
                unsigned int color = handle.m_kind == Kind::MoveX ? s_red : s_green;
                list.push_back({DrawCommand::Kind::Line, handle.m_originX, handle.m_originY, handle.m_x - handle.m_originX, handle.m_y - handle.m_originY,
                                color});
                list.push_back({DrawCommand::Kind::FilledRect, handle.m_x - handle.m_size, handle.m_y - handle.m_size, handle.m_size * 2,
                                handle.m_size * 2, color});
                break;
            }
            case Kind::Rotate:
            {
                const int segments = 48;
                for (int index = 0; index < segments; ++index)
                {
                    double from = 2 * s_pi * index / segments;
                    double to = 2 * s_pi * (index + 1) / segments;
                    double x = handle.m_x + std::cos(from) * handle.m_size;
                    double y = handle.m_y + std::sin(from) * handle.m_size;
                    list.push_back({DrawCommand::Kind::Line, x, y, handle.m_x + std::cos(to) * handle.m_size - x,
                                    handle.m_y + std::sin(to) * handle.m_size - y, s_ring});
                }
                break;
            }
            default:
                list.push_back({DrawCommand::Kind::FilledRect, handle.m_x - handle.m_size, handle.m_y - handle.m_size, handle.m_size * 2,
                                handle.m_size * 2, handle.m_kind == Kind::ScaleBoth ? s_ring : s_blue});
                break;
            }
        }
        return list;
    }

    GizmoDrag::GizmoDrag(const GizmoHandle &handle, const Viewport &viewport, const ObjectBounds &bounds, const TransformValues &start, double frameX,
                         double frameY)
        : m_handle(handle)
        , m_bounds(bounds)
        , m_start(start)
        , m_startWorldX(viewport.frameToWorldX(frameX))
        , m_startWorldY(viewport.frameToWorldY(frameY))
        , m_startAngle(std::atan2(frameY - handle.m_originY, frameX - handle.m_originX))
    {
    }

    std::map<std::string, double> GizmoDrag::update(const Viewport &viewport, double frameX, double frameY) const
    {
        using Kind = GizmoHandle::Kind;
        double worldX = viewport.frameToWorldX(frameX);
        double worldY = viewport.frameToWorldY(frameY);
        std::map<std::string, double> fields;

        switch (m_handle.m_kind)
        {
        case Kind::MoveX:
            fields["x"] = std::round(m_start.m_x + worldX - m_startWorldX);
            break;
        case Kind::MoveY:
            fields["y"] = std::round(m_start.m_y + worldY - m_startWorldY);
            break;
        case Kind::Rotate:
        {
            double angle = std::atan2(frameY - m_handle.m_originY, frameX - m_handle.m_originX);
            fields["rotation"] = roundTo(m_start.m_rotation + (angle - m_startAngle) * 180 / s_pi, 1);
            break;
        }
        default:
        {
            // The handle's origin is the box's top left, so the pointer's distance from it is the new size.
            double left = viewport.frameToWorldX(m_handle.m_originX);
            double top = viewport.frameToWorldY(m_handle.m_originY);
            double ratioX = m_bounds.m_width > 0 ? (worldX - left) / m_bounds.m_width : 1;
            double ratioY = m_bounds.m_height > 0 ? (worldY - top) / m_bounds.m_height : 1;
            double uniform = (ratioX + ratioY) / 2;
            if (m_handle.m_kind == Kind::ScaleX || m_handle.m_kind == Kind::ScaleBoth)
            {
                fields["scaleX"] = roundTo(m_start.m_scaleX * (m_handle.m_kind == Kind::ScaleBoth ? uniform : ratioX), 0.01);
            }
            if (m_handle.m_kind == Kind::ScaleY || m_handle.m_kind == Kind::ScaleBoth)
            {
                fields["scaleY"] = roundTo(m_start.m_scaleY * (m_handle.m_kind == Kind::ScaleBoth ? uniform : ratioY), 0.01);
            }
            break;
        }
        }
        return fields;
    }
}
