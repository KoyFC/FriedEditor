#pragma once

#include "fried/editor/viewport.h"

#include <optional>
#include <utility>

namespace fried::editor
{
    // The editor's camera for a 3D scene, which looks about itself and flies, and is kept as a target a distance in front of it so that it can also be dollied and focused, with the engine's maths: left handed, y up, turned about x and then y, so a positive pitch looks down.
    class Viewport3D
    {
    public:
        void setFrameSize(int width, int height);
        int frameWidth() const { return m_frameWidth; }
        int frameHeight() const { return m_frameHeight; }

        void set(double targetX, double targetY, double targetZ, double distance, double yaw, double pitch);

        // The camera turns about its own position, as a free camera does: a step of a pixel is a third of a degree, and the view cannot go past straight up or down.
        void look(double deltaX, double deltaY);

        // Moves the camera along where it looks, across it and straight up, by distances in the world's units.
        void fly(double forward, double right, double up);

        // Moves the target across the view by the pixels the pointer moved, so the world follows the pointer.
        void pan(double deltaX, double deltaY);

        // Closer by a tenth for each step of the wheel.
        void dolly(double steps);

        void focus(double x, double y, double z);

        double x() const;
        double y() const;
        double z() const;
        double rotationX() const { return m_pitch; }
        double rotationY() const { return m_yaw; }
        double fieldOfView() const { return m_fieldOfView; }
        double distance() const { return m_distance; }
        double targetX() const { return m_targetX; }
        double targetY() const { return m_targetY; }
        double targetZ() const { return m_targetZ; }

        // Where a point of the world lands on the frame, or nothing when it is nearer than the camera's near plane.
        std::optional<std::pair<double, double>> project(double x, double y, double z) const;

        // The ground's lines from -10 to 10 on x and z, the x axis red and the z axis blue, cut where they pass behind the camera.
        DrawList grid() const;

        static constexpr double s_near = 0.1;

    private:
        void axes(double *forward, double *right, double *up) const;

        int m_frameWidth = 1;
        int m_frameHeight = 1;
        double m_targetX = 0;
        double m_targetY = 0;
        double m_targetZ = 0;
        double m_distance = 10;
        double m_yaw = 0;
        double m_pitch = 20;
        double m_fieldOfView = 60;
    };
}
