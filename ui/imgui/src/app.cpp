#include "app.h"

#include "panels.h"
#include "style.h"

#include "fried/editor/version.h"

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <SDL.h>
#include <GL/gl.h>

#include <cstdlib>
#include <filesystem>

namespace imgui_backend
{
    namespace
    {
        using fried::editor::Editor;

        std::string layoutFile()
        {
            const char *config = std::getenv("XDG_CONFIG_HOME");
            std::filesystem::path directory = config && *config ? std::filesystem::path(config) : std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : ".") / ".config";
            directory /= "fried-editor";
            std::error_code ignored;
            std::filesystem::create_directories(directory, ignored);
            return (directory / "layout.ini").string();
        }

        // The first time there is no saved layout, so the panels are put where an editor has them.
        void buildLayout(ImGuiID dockspace, ImVec2 size)
        {
            ImGui::DockBuilderRemoveNode(dockspace);
            ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace, size);

            ImGuiID center = dockspace;
            ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
            ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.26f, nullptr, &center);
            ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.26f, nullptr, &center);
            ImGui::DockBuilderDockWindow("Hierarchy", left);
            ImGui::DockBuilderDockWindow("Inspector", right);
            ImGui::DockBuilderDockWindow("Assets", bottom);
            ImGui::DockBuilderDockWindow("Viewport", center);
            ImGui::DockBuilderFinish(dockspace);
        }

        void save(Editor &editor)
        {
            std::string error;
            if (!editor.save(&error))
            {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Save", error.c_str(), nullptr);
            }
        }

        bool sideBar(const char *name, ImGuiDir direction, float height, const std::function<void()> &content)
        {
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
            bool isOpen = ImGui::BeginViewportSideBar(name, ImGui::GetMainViewport(), direction, height, flags);
            if (isOpen)
            {
                content();
            }
            ImGui::End();
            return isOpen;
        }
    }

    int run(Editor &editor)
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fried Editor", SDL_GetError(), nullptr);
            return 1;
        }
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        SDL_Window *window = SDL_CreateWindow(("Fried Editor " + fried::editor::version()).c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1500, 900,
                                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window)
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fried Editor", SDL_GetError(), nullptr);
            SDL_Quit();
            return 1;
        }
        SDL_GLContext context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, context);
        SDL_GL_SetSwapInterval(1);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        static const std::string s_layout = layoutFile();
        io.IniFilename = s_layout.c_str();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
        applyStyle(editor.enginePath() + "/assets/NunitoSans.ttf");
        ImGui_ImplSDL2_InitForOpenGL(window, context);
        ImGui_ImplOpenGL3_Init("#version 330");

        Panels panels(editor);
        bool isRunning = true;
        bool isCloseAsked = false;
        bool isLayoutBuilt = false;

        while (isRunning)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                ImGui_ImplSDL2_ProcessEvent(&event);
                if (event.type == SDL_QUIT || (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE))
                {
                    if (editor.commands().isClean())
                    {
                        isRunning = false;
                    }
                    else
                    {
                        isCloseAsked = true;
                    }
                }
            }

            editor.pump();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            if (!io.WantTextInput)
            {
                bool isControl = io.KeyCtrl;
                if (isControl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
                {
                    io.KeyShift ? editor.commands().redo() : editor.commands().undo();
                }
                else if (isControl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
                {
                    editor.commands().redo();
                }
                else if (isControl && ImGui::IsKeyPressed(ImGuiKey_S, false))
                {
                    save(editor);
                }
                else if (!isControl && ImGui::IsKeyPressed(ImGuiKey_W, false))
                {
                    editor.setGizmoMode(fried::editor::GizmoMode::Move);
                }
                else if (!isControl && ImGui::IsKeyPressed(ImGuiKey_E, false))
                {
                    editor.setGizmoMode(fried::editor::GizmoMode::Rotate);
                }
                else if (!isControl && ImGui::IsKeyPressed(ImGuiKey_R, false))
                {
                    editor.setGizmoMode(fried::editor::GizmoMode::Scale);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                {
                    editor.removeSelected();
                }
            }

            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Save", "Ctrl+S"))
                    {
                        save(editor);
                    }
                    if (ImGui::MenuItem("Rebuild player"))
                    {
                        editor.rebuildPlayer();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Quit"))
                    {
                        isCloseAsked = !editor.commands().isClean();
                        isRunning = !editor.commands().isClean();
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Edit"))
                {
                    auto &commands = editor.commands();
                    std::string undo = "Undo " + commands.undoLabel();
                    std::string redo = "Redo " + commands.redoLabel();
                    if (ImGui::MenuItem(undo.c_str(), "Ctrl+Z", false, commands.canUndo()))
                    {
                        commands.undo();
                    }
                    if (ImGui::MenuItem(redo.c_str(), "Ctrl+Y", false, commands.canRedo()))
                    {
                        commands.redo();
                    }
                    ImGui::EndMenu();
                }
                ImGui::SameLine(ImGui::GetWindowWidth() - 260);
                ImGui::TextDisabled("%s%s", editor.project().name().c_str(), editor.commands().isClean() ? "" : "  *");
                ImGui::EndMainMenuBar();
            }

            float bar = ImGui::GetFrameHeight() + 10;
            sideBar("##toolbar", ImGuiDir_Up, bar, [&] { panels.toolbar(); });
            sideBar("##status", ImGuiDir_Down, bar - 6, [&] { panels.statusBar(); });

            ImGuiID dockspace = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
            if (!isLayoutBuilt)
            {
                isLayoutBuilt = true;
                if (!ImGui::DockBuilderGetNode(dockspace) || ImGui::DockBuilderGetNode(dockspace)->IsLeafNode())
                {
                    if (!std::filesystem::exists(s_layout) || std::filesystem::file_size(s_layout) < 64)
                    {
                        buildLayout(dockspace, ImGui::GetMainViewport()->Size);
                    }
                }
            }

            if (ImGui::Begin("Viewport"))
            {
                panels.viewport();
            }
            ImGui::End();
            if (ImGui::Begin("Hierarchy"))
            {
                panels.hierarchy();
            }
            ImGui::End();
            if (ImGui::Begin("Inspector"))
            {
                panels.inspector();
            }
            ImGui::End();
            if (ImGui::Begin("Assets"))
            {
                panels.assets();
            }
            ImGui::End();

            if (isCloseAsked)
            {
                ImGui::OpenPopup("Unsaved changes");
                isCloseAsked = false;
            }
            if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted("Save the scene before closing?");
                ImGui::Spacing();
                if (ImGui::Button("Save"))
                {
                    save(editor);
                    isRunning = !editor.commands().isClean();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Discard"))
                {
                    isRunning = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::Render();
            int width = 0, height = 0;
            SDL_GL_GetDrawableSize(window, &width, &height);
            glViewport(0, 0, width, height);
            glClearColor(0.085f, 0.085f, 0.105f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(window);

            std::string title = editor.document().name() + (editor.commands().isClean() ? "" : "*") + " - " + editor.project().name() + " - Fried Editor";
            SDL_SetWindowTitle(window, title.c_str());
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }
}
