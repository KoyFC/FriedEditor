#pragma once

#include "fried/editor/viewport.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fried::editor
{
    enum class GizmoMode
    {
        Move,
        Rotate,
        Scale
    };

    // What an object's Transform holds, with the engine's defaults where the file says nothing.
    struct TransformValues
    {
        double m_x = 0;
        double m_y = 0;
        double m_rotation = 0;
        double m_scaleX = 1;
        double m_scaleY = 1;
    };

    struct GizmoHandle
    {
        enum class Kind
        {
            MoveX,
            MoveY,
            Rotate,
            ScaleX,
            ScaleY,
            ScaleBoth
        };

        Kind m_kind = Kind::MoveX;

        // In frame pixels. A ring is centered here and has the radius, anything else is a square of that half size.
        double m_x = 0;
        double m_y = 0;
        double m_size = 0;
        double m_originX = 0;
        double m_originY = 0;
    };

    // The handles a mode puts on the selected object, and what dragging one does to its Transform.
    class Gizmo
    {
    public:
        static constexpr double s_arrowLength = 70;
        static constexpr double s_handleHalfSize = 6;
        static constexpr double s_ringTolerance = 7;

        static std::vector<GizmoHandle> handles(GizmoMode mode, const Viewport &viewport, const ObjectBounds &bounds, const TransformValues &transform);
        static std::optional<GizmoHandle> hit(const std::vector<GizmoHandle> &handles, double frameX, double frameY);
        static DrawList draw(const std::vector<GizmoHandle> &handles);
    };

    // One drag of a handle, from where the pointer went down.
    class GizmoDrag
    {
    public:
        GizmoDrag(const GizmoHandle &handle, const Viewport &viewport, const ObjectBounds &bounds, const TransformValues &start, double frameX,
                  double frameY);

        // The Transform fields that change, for the pointer where it is now.
        std::map<std::string, double> update(const Viewport &viewport, double frameX, double frameY) const;

    private:
        GizmoHandle m_handle;
        ObjectBounds m_bounds;
        TransformValues m_start;
        double m_startWorldX;
        double m_startWorldY;
        double m_startAngle;
    };
}
