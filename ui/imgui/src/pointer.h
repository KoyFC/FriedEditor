#pragma once

#include <imgui.h>

#include <SDL.h>

#include <array>

namespace imgui_backend
{
    // The pointer's look and reach. It shows the cursor ImGui asks for, hides and holds it still while the scene is panned, and wraps it at the window's edges while a value is dragged so the drag can go on past them.
    class Pointer
    {
    public:
        explicit Pointer(SDL_Window *window);
        ~Pointer();
        Pointer(const Pointer &) = delete;
        Pointer &operator=(const Pointer &) = delete;

        // For every event, to add up the pointer's movement while it is held still.
        void handleEvent(const SDL_Event &event);

        // Right after ImGui::NewFrame, before anything reads the pointer's movement.
        void beginFrame();

        // The movement since the last call, which is the only movement there is while the pointer is held still.
        ImVec2 takeHeldMotion();

        // After the panels are drawn.
        void endFrame(bool isPanning, bool isDraggingValue);

    private:
        void wrap();

        SDL_Window *m_window;
        std::array<SDL_Cursor *, ImGuiMouseCursor_COUNT> m_cursors{};
        SDL_Cursor *m_shown = nullptr;
        ImVec2 m_heldMotion;
        bool m_isWarpPending = false;
        ImVec2 m_warpTarget;
        int m_warpFrames = 0;
    };
}
