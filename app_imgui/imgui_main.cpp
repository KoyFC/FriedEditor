#include "app.h"

#include "fried/editor/editor.h"

#include <SDL.h>

#include <cstring>
#include <filesystem>
#include <iostream>

namespace
{
    int fail(const std::string &message)
    {
        std::cerr << message << std::endl;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fried Editor", message.c_str(), nullptr);
        return 1;
    }
}

int main(int argc, char *argv[])
{
    std::string project;
    std::string engine;
    for (int index = 1; index < argc; ++index)
    {
        if (std::strcmp(argv[index], "--engine") == 0 && index + 1 < argc)
        {
            engine = argv[++index];
        }
        else if (std::strcmp(argv[index], "--help") == 0 || std::strcmp(argv[index], "-h") == 0)
        {
            std::cout << "Usage: fried_editor <project> [--engine <path>]\n"
                         "  <project>  the folder with project.fried\n"
                         "  --engine   the Fried Engine checkout, found next to the project by default\n";
            return 0;
        }
        else
        {
            project = argv[index];
        }
    }
    if (project.empty())
    {
        return fail("Start it with the project's folder: fried_editor <project> [--engine <path>]");
    }

    project = std::filesystem::absolute(project).lexically_normal().string();
    engine = engine.empty() ? fried::editor::Editor::findEngine(project) : std::filesystem::absolute(engine).string();
    if (engine.empty() || !std::filesystem::exists(engine + "/editor/player/build.hxml"))
    {
        return fail("No Fried Engine with an editor player was found. Pass its folder with --engine.");
    }

    fried::editor::Editor editor;
    std::string error;
    if (!editor.open(project, engine, &error))
    {
        return fail(error);
    }
    return imgui_backend::run(editor);
}
