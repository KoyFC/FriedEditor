#include "fried/editor/viewport3d.h"

#include <algorithm>
#include <cmath>

namespace fried::editor
{
    namespace
    {
        constexpr double s_pi = 3.14159265358979323846;
        constexpr double s_degrees = s_pi / 180.0;
        constexpr double s_minDistance = 0.1;
        constexpr double s_maxDistance = 10000.0;
        constexpr double s_maxPitch = 89.0;
        constexpr double s_degreesPerPixel = 0.3;
        constexpr int s_gridExtent = 10;
        constexpr unsigned int s_gridColor = 0x30ffffff;
        constexpr unsigned int s_xAxisColor = 0xc04040ff;
        constexpr unsigned int s_zAxisColor = 0xc0ff6040;
    }

    void Viewport3D::setFrameSize(int width, int height)
    {
        m_frameWidth = std::max(1, width);
        m_frameHeight = std::max(1, height);
    }

    void Viewport3D::set(double targetX, double targetY, double targetZ, double distance, double yaw, double pitch)
    {
        m_targetX = targetX;
        m_targetY = targetY;
        m_targetZ = targetZ;
        m_distance = std::clamp(distance, s_minDistance, s_maxDistance);
        m_yaw = yaw;
        m_pitch = std::clamp(pitch, -s_maxPitch, s_maxPitch);
    }


    void Viewport3D::axes(double *forward, double *right, double *up) const
    {
        double yaw = m_yaw * s_degrees;
        double pitch = m_pitch * s_degrees;
        forward[0] = std::sin(yaw) * std::cos(pitch);
        forward[1] = -std::sin(pitch);
        forward[2] = std::cos(yaw) * std::cos(pitch);
        right[0] = std::cos(yaw);
        right[1] = 0;
        right[2] = -std::sin(yaw);
        up[0] = std::sin(yaw) * std::sin(pitch);
        up[1] = std::cos(pitch);
        up[2] = std::cos(yaw) * std::sin(pitch);
    }

    void Viewport3D::look(double deltaX, double deltaY)
    {
        double eyeX = x();
        double eyeY = y();
        double eyeZ = z();
        m_yaw += deltaX * s_degreesPerPixel;
        m_pitch = std::clamp(m_pitch + deltaY * s_degreesPerPixel, -s_maxPitch, s_maxPitch);
        double forward[3], right[3], up[3];
        axes(forward, right, up);
        m_targetX = eyeX + forward[0] * m_distance;
        m_targetY = eyeY + forward[1] * m_distance;
        m_targetZ = eyeZ + forward[2] * m_distance;
    }

    void Viewport3D::fly(double forwardDistance, double rightDistance, double upDistance)
    {
        double forward[3], right[3], up[3];
        axes(forward, right, up);
        m_targetX += forward[0] * forwardDistance + right[0] * rightDistance;
        m_targetY += forward[1] * forwardDistance + right[1] * rightDistance + upDistance;
        m_targetZ += forward[2] * forwardDistance + right[2] * rightDistance;
    }

    void Viewport3D::pan(double deltaX, double deltaY)
    {
        double forward[3];
        double right[3];
        double up[3];
        axes(forward, right, up);
        double unit = 2.0 * m_distance * std::tan(m_fieldOfView * s_degrees / 2.0) / m_frameHeight;
        m_targetX += (-right[0] * deltaX + up[0] * deltaY) * unit;
        m_targetY += (-right[1] * deltaX + up[1] * deltaY) * unit;
        m_targetZ += (-right[2] * deltaX + up[2] * deltaY) * unit;
    }

    void Viewport3D::dolly(double steps)
    {
        m_distance = std::clamp(m_distance * std::pow(1.1, -steps), s_minDistance, s_maxDistance);
    }

    void Viewport3D::focus(double x, double y, double z)
    {
        m_targetX = x;
        m_targetY = y;
        m_targetZ = z;
    }

    double Viewport3D::x() const
    {
        double forward[3], right[3], up[3];
        axes(forward, right, up);
        return m_targetX - forward[0] * m_distance;
    }

    double Viewport3D::y() const
    {
        double forward[3], right[3], up[3];
        axes(forward, right, up);
        return m_targetY - forward[1] * m_distance;
    }

    double Viewport3D::z() const
    {
        double forward[3], right[3], up[3];
        axes(forward, right, up);
        return m_targetZ - forward[2] * m_distance;
    }

    std::optional<std::pair<double, double>> Viewport3D::project(double px, double py, double pz) const
    {
        double forward[3], right[3], up[3];
        axes(forward, right, up);
        double dx = px - x();
        double dy = py - y();
        double dz = pz - z();
        double depth = forward[0] * dx + forward[1] * dy + forward[2] * dz;
        if (depth < s_near)
        {
            return std::nullopt;
        }
        double viewX = right[0] * dx + right[1] * dy + right[2] * dz;
        double viewY = up[0] * dx + up[1] * dy + up[2] * dz;
        double focalLength = (m_frameHeight / 2.0) / std::tan(m_fieldOfView * s_degrees / 2.0);
        return std::make_pair(m_frameWidth / 2.0 + viewX * focalLength / depth, m_frameHeight / 2.0 - viewY * focalLength / depth);
    }

    DrawList Viewport3D::grid() const
    {
        DrawList list;
        double forward[3], right[3], up[3];
        axes(forward, right, up);
        double cameraX = x();
        double cameraY = y();
        double cameraZ = z();
        double focalLength = (m_frameHeight / 2.0) / std::tan(m_fieldOfView * s_degrees / 2.0);

        auto line = [&](double ax, double ay, double az, double bx, double by, double bz, unsigned int color) {
            double points[2][3] = {{ax, ay, az}, {bx, by, bz}};
            double view[2][3];
            for (int end = 0; end < 2; ++end)
            {
                double dx = points[end][0] - cameraX;
                double dy = points[end][1] - cameraY;
                double dz = points[end][2] - cameraZ;
                view[end][0] = right[0] * dx + right[1] * dy + right[2] * dz;
                view[end][1] = up[0] * dx + up[1] * dy + up[2] * dz;
                view[end][2] = forward[0] * dx + forward[1] * dy + forward[2] * dz;
            }
            if (view[0][2] < s_near && view[1][2] < s_near)
            {
                return;
            }
            for (int end = 0; end < 2; ++end)
            {
                if (view[end][2] < s_near)
                {
                    int other = 1 - end;
                    double along = (s_near - view[end][2]) / (view[other][2] - view[end][2]);
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        view[end][axis] += (view[other][axis] - view[end][axis]) * along;
                    }
                }
            }
            double fromX = m_frameWidth / 2.0 + view[0][0] * focalLength / view[0][2];
            double fromY = m_frameHeight / 2.0 - view[0][1] * focalLength / view[0][2];
            double toX = m_frameWidth / 2.0 + view[1][0] * focalLength / view[1][2];
            double toY = m_frameHeight / 2.0 - view[1][1] * focalLength / view[1][2];
            list.push_back({DrawCommand::Kind::Line, fromX, fromY, toX - fromX, toY - fromY, color});
        };

        for (int offset = -s_gridExtent; offset <= s_gridExtent; ++offset)
        {
            if (offset != 0)
            {
                line(offset, 0, -s_gridExtent, offset, 0, s_gridExtent, s_gridColor);
                line(-s_gridExtent, 0, offset, s_gridExtent, 0, offset, s_gridColor);
            }
        }
        line(-s_gridExtent, 0, 0, s_gridExtent, 0, 0, s_xAxisColor);
        line(0, 0, -s_gridExtent, 0, 0, s_gridExtent, s_zAxisColor);
        return list;
    }
}
