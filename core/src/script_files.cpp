#include "fried/editor/script_files.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace fried::editor
{
    namespace
    {
        std::string readFile(const fs::path &file)
        {
            std::ifstream in(file, std::ios::binary);
            std::stringstream text;
            text << in.rdbuf();
            return text.str();
        }

        int renameType(Json *value, const std::string &oldName, const std::string &newName)
        {
            int count = 0;
            if (value->is_object())
            {
                auto type = value->find("type");
                if (type != value->end() && type->is_string() && type->get<std::string>() == oldName)
                {
                    *type = newName;
                    ++count;
                }
            }
            if (value->is_object() || value->is_array())
            {
                for (Json &child : *value)
                {
                    count += renameType(&child, oldName, newName);
                }
            }
            return count;
        }
    }

    bool openInVsCode(const std::string &projectDirectory, const std::string &file)
    {
        auto quoted = [](const std::string &text) {
#ifdef _WIN32
            return "\"" + text + "\"";
#else
            std::string result = "'";
            for (char c : text)
            {
                result += c == '\'' ? std::string("'\\''") : std::string(1, c);
            }
            return result + "'";
#endif
        };
#ifdef _WIN32
        std::string command = "where code >nul 2>nul && start \"\" /b code " + quoted(projectDirectory) + " -g " + quoted(file);
#else
        std::string command = "command -v code >/dev/null 2>&1 && { nohup code " + quoted(projectDirectory) + " -g " + quoted(file) + " >/dev/null 2>&1 & }";
#endif
        return std::system(command.c_str()) == 0;
    }

    bool isScriptName(const std::string &name)
    {
        if (name.empty() || !std::isupper((unsigned char)name[0]))
        {
            return false;
        }
        for (char c : name)
        {
            if (!std::isalnum((unsigned char)c) && c != '_')
            {
                return false;
            }
        }
        return true;
    }

    std::string packageOfFolder(const std::string &folder)
    {
        std::string package = folder;
        for (char &c : package)
        {
            c = c == '/' ? '.' : c;
        }
        return package;
    }

    std::string findClassModule(const std::vector<std::string> &sourceRoots, const std::string &className)
    {
        std::error_code failure;
        for (const std::string &root : sourceRoots)
        {
            for (fs::recursive_directory_iterator it(root, failure), end; !failure && it != end; it.increment(failure))
            {
                if (it->is_regular_file(failure) && it->path().filename() == className + ".hx")
                {
                    std::string folder = fs::relative(it->path().parent_path(), root, failure).generic_string();
                    std::string package = folder == "." ? std::string() : packageOfFolder(folder);
                    return package.empty() ? className : package + "." + className;
                }
            }
            failure.clear();
        }
        return std::string();
    }

    std::string scriptTemplate(const std::string &package, const std::string &name, const std::string &baseModule)
    {
        std::string base = baseModule.empty() ? "fried.scene.Node" : baseModule;
        std::string baseName = base.substr(base.rfind('.') == std::string::npos ? 0 : base.rfind('.') + 1);
        std::string text;
        if (!package.empty())
        {
            text += "package " + package + ";\n\n";
        }
        if (base.find('.') != std::string::npos && base.compare(0, package.size() + 1, package + ".") != 0)
        {
            text += "import " + base + ";\n\n";
        }
        text += "@:serializable\n";
        text += "class " + name + " extends " + baseName + " {\n";
        text += "\tpublic function new() {\n";
        text += "\t\tsuper();\n";
        text += "\t\tname = \"" + name + "\";\n";
        text += "\t}\n\n";
        text += "\toverride function ready():Void {}\n\n";
        text += "\toverride function update(delta:Float):Void {}\n";
        text += "}\n";
        return text;
    }

    bool createScript(const std::string &sourceDirectory, const std::string &folder, const std::string &name, const std::string &baseModule, std::string *error)
    {
        if (!isScriptName(name))
        {
            *error = "A script is named like a class: it starts with a capital letter and has only letters, digits and underscores.";
            return false;
        }
        std::error_code failure;
        fs::path directory = fs::path(sourceDirectory) / folder;
        fs::path file = directory / (name + ".hx");
        if (fs::exists(file, failure))
        {
            *error = "There is already a script named " + name + " there.";
            return false;
        }
        fs::create_directories(directory, failure);
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            *error = "The script could not be written.";
            return false;
        }
        out << scriptTemplate(packageOfFolder(folder), name, baseModule);
        return true;
    }

    bool fixScript(const std::string &file, const std::string &package, const std::string &oldName, const std::string &newName)
    {
        std::string text = readFile(file);
        std::string fixed = text;
        std::string line = package.empty() ? std::string() : "package " + package + ";";
        std::regex packageLine("^[ \\t]*package[ \\t]*[A-Za-z0-9_.]*[ \\t]*;", std::regex::multiline);
        if (std::regex_search(fixed, packageLine))
        {
            fixed = std::regex_replace(fixed, packageLine, line, std::regex_constants::format_first_only);
        }
        else if (!line.empty())
        {
            fixed = line + "\n\n" + fixed;
        }
        if (oldName != newName)
        {
            std::regex declaration("\\b(class|interface|enum|abstract|typedef)([ \\t]+)" + oldName + "\\b");
            fixed = std::regex_replace(fixed, declaration, "$1$2" + newName);
        }
        if (fixed == text)
        {
            return true;
        }
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << fixed;
        return (bool)out;
    }

    int renameSceneType(const std::string &scenesDirectory, const std::string &oldName, const std::string &newName)
    {
        int total = 0;
        std::error_code ignored;
        for (const auto &entry : fs::directory_iterator(scenesDirectory, ignored))
        {
            std::string name = entry.path().filename().string();
            if (!entry.is_regular_file(ignored) || name.size() <= 11 || name.compare(name.size() - 11, 11, ".scene.json") != 0)
            {
                continue;
            }
            Json data = Json::parse(readFile(entry.path()), nullptr, false);
            if (data.is_discarded())
            {
                continue;
            }
            int count = renameType(&data, oldName, newName);
            if (count > 0)
            {
                std::ofstream out(entry.path(), std::ios::binary | std::ios::trunc);
                out << data.dump(1, '\t') << "\n";
                total += count;
            }
        }
        return total;
    }
}
