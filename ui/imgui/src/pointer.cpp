#include "pointer.h"

#include <cmath>

namespace imgui_backend
{
    namespace
    {
        constexpr float s_wrapMargin = 2;
        constexpr float s_wrapLanding = 6;
        constexpr int s_warpWaitFrames = 10;

        SDL_Cursor *systemCursor(SDL_SystemCursor kind)
        {
            return SDL_CreateSystemCursor(kind);
        }
    }

    Pointer::Pointer(SDL_Window *window)
        : m_window(window)
    {
        m_cursors[ImGuiMouseCursor_Arrow] = systemCursor(SDL_SYSTEM_CURSOR_ARROW);
        m_cursors[ImGuiMouseCursor_TextInput] = systemCursor(SDL_SYSTEM_CURSOR_IBEAM);
        m_cursors[ImGuiMouseCursor_ResizeAll] = systemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
        m_cursors[ImGuiMouseCursor_ResizeNS] = systemCursor(SDL_SYSTEM_CURSOR_SIZENS);
        m_cursors[ImGuiMouseCursor_ResizeEW] = systemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
        m_cursors[ImGuiMouseCursor_ResizeNESW] = systemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
        m_cursors[ImGuiMouseCursor_ResizeNWSE] = systemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
        m_cursors[ImGuiMouseCursor_Hand] = systemCursor(SDL_SYSTEM_CURSOR_HAND);
        m_cursors[ImGuiMouseCursor_NotAllowed] = systemCursor(SDL_SYSTEM_CURSOR_NO);
        m_cursors[ImGuiMouseCursor_Wait] = systemCursor(SDL_SYSTEM_CURSOR_WAIT);
        m_cursors[ImGuiMouseCursor_Progress] = systemCursor(SDL_SYSTEM_CURSOR_WAITARROW);
    }

    Pointer::~Pointer()
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        for (SDL_Cursor *cursor : m_cursors)
        {
            if (cursor)
            {
                SDL_FreeCursor(cursor);
            }
        }
    }

    void Pointer::handleEvent(const SDL_Event &event)
    {
        if (event.type == SDL_MOUSEMOTION && SDL_GetRelativeMouseMode())
        {
            m_heldMotion.x += (float)event.motion.xrel;
            m_heldMotion.y += (float)event.motion.yrel;
        }
    }

    ImVec2 Pointer::takeHeldMotion()
    {
        ImVec2 motion = m_heldMotion;
        m_heldMotion = ImVec2(0, 0);
        return motion;
    }

    void Pointer::beginFrame()
    {
        if (!m_isWarpPending)
        {
            return;
        }
        // The jump to the other side is not movement, so a value does not take it as the drag.
        ImGuiIO &io = ImGui::GetIO();
        float dx = io.MousePos.x - m_warpTarget.x;
        float dy = io.MousePos.y - m_warpTarget.y;
        if (std::sqrt(dx * dx + dy * dy) < s_wrapLanding || ++m_warpFrames > s_warpWaitFrames)
        {
            m_isWarpPending = false;
        }
        io.MouseDelta = ImVec2(0, 0);
    }

    void Pointer::endFrame(bool isPanning, bool isDraggingValue)
    {
        if ((SDL_GetRelativeMouseMode() == SDL_TRUE) != isPanning)
        {
            SDL_SetRelativeMouseMode(isPanning ? SDL_TRUE : SDL_FALSE);
        }

        ImGuiMouseCursor wanted = ImGui::GetMouseCursor();
        if (isPanning || wanted == ImGuiMouseCursor_None)
        {
            SDL_ShowCursor(SDL_FALSE);
        }
        else
        {
            SDL_ShowCursor(SDL_TRUE);
            SDL_Cursor *cursor = m_cursors[wanted] ? m_cursors[wanted] : m_cursors[ImGuiMouseCursor_Arrow];
            if (cursor != m_shown)
            {
                SDL_SetCursor(cursor);
                m_shown = cursor;
            }
        }

        if (isDraggingValue && !isPanning && !m_isWarpPending)
        {
            wrap();
        }
    }

    void Pointer::wrap()
    {
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(m_window, &width, &height);
        ImVec2 position = ImGui::GetIO().MousePos;
        ImVec2 target = position;
        if (position.x <= s_wrapMargin)
        {
            target.x = width - s_wrapLanding;
        }
        else if (position.x >= width - 1 - s_wrapMargin)
        {
            target.x = s_wrapLanding;
        }
        if (position.y <= s_wrapMargin)
        {
            target.y = height - s_wrapLanding;
        }
        else if (position.y >= height - 1 - s_wrapMargin)
        {
            target.y = s_wrapLanding;
        }
        if (target.x == position.x && target.y == position.y)
        {
            return;
        }
        SDL_WarpMouseInWindow(m_window, (int)target.x, (int)target.y);
        m_isWarpPending = true;
        m_warpTarget = target;
        m_warpFrames = 0;
    }
}
