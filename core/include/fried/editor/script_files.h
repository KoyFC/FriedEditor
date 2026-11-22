#pragma once

#include <string>
#include <vector>

namespace fried::editor
{
    // A name for a Haxe class: a capital letter and then letters, digits and underscores.
    bool isScriptName(const std::string &name);

    // The package of a folder under the source directory: "nodes/enemies" is "nodes.enemies", and the source directory itself has none.
    std::string packageOfFolder(const std::string &folder);

    // The module a class is written in, such as "fried.scene.Node2D", found by looking for its file under each source root. Empty when no root has it.
    std::string findClassModule(const std::vector<std::string> &sourceRoots, const std::string &className);

    // The text of a new node class that extends a module, or Node when there is none.
    std::string scriptTemplate(const std::string &package, const std::string &name, const std::string &baseModule);

    // Writes a new script into a folder under the source directory. Says what went wrong in error.
    bool createScript(const std::string &sourceDirectory, const std::string &folder, const std::string &name, const std::string &baseModule, std::string *error);

    // Makes the package line of a script say where its file is, and the class that has the file's old name take the new one. Both are no-ops when nothing changes.
    bool fixScript(const std::string &file, const std::string &package, const std::string &oldName, const std::string &newName);

    // Makes every node of a scene file whose type is the old name take the new one. Returns how many it changed.
    int renameSceneType(const std::string &scenesDirectory, const std::string &oldName, const std::string &newName);
}
