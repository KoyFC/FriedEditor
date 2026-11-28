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
    std::string scene;
    for (int index = 1; index < argc; ++index)
    {
        if (std::strcmp(argv[index], "--engine") == 0 && index + 1 < argc)
        {
            engine = argv[++index];
        }
        else if (std::strcmp(argv[index], "--scene") == 0 && index + 1 < argc)
        {
            scene = argv[++index];
        }
        else if (std::strcmp(argv[index], "--help") == 0 || std::strcmp(argv[index], "-h") == 0)
        {
            std::cout << "Usage: fried_editor <project> [--engine <path>] [--scene <file>]\n"
                         "  <project>  the folder with project.fried\n"
                         "  --engine   the Fried Engine checkout, found next to the project by default\n"
                         "  --scene    the scene file to open, named as in assets/scenes, the first one by default\n";
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
    if (!editor.open(project, engine, &error) && editor.isSceneMigrationOffered())
    {
        std::cout << "The scenes of this project are version 2, made of nodes, and the editor reads game objects with components.\n"
                     "Rewrite them with the engine's fried_import? The originals are kept in .fried/v2-backup. [y/N] ";
        std::string answer;
        std::getline(std::cin, answer);
        if (answer.empty() || (answer[0] != 'y' && answer[0] != 'Y'))
        {
            return fail(error);
        }
        std::string report;
        bool isMigrated = editor.migrateScenes(&report);
        std::cout << report << "\n";
        if (!isMigrated)
        {
            return fail("The scenes were not converted.");
        }
        error.clear();
    }
    else if (!error.empty())
    {
        return fail(error);
    }
    if (!scene.empty() && !editor.openScene(editor.project().scenesDirectory() + "/" + scene, &error))
    {
        return fail(error);
    }
    return editor_ui::run(editor);
}
