#include "fried/editor/trash.h"

#include <cctype>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace fried::editor
{
    namespace
    {
        // The home trash of the freedesktop.org specification, the one file managers list and restore from. FRIED_EDITOR_TRASH replaces it, for the tests.
        fs::path trashDirectory()
        {
            const char *own = std::getenv("FRIED_EDITOR_TRASH");
            if (own != nullptr && *own != '\0')
            {
                return fs::path(own);
            }
            const char *data = std::getenv("XDG_DATA_HOME");
            if (data != nullptr && *data != '\0')
            {
                return fs::path(data) / "Trash";
            }
            const char *home = std::getenv("HOME");
            return fs::path(home != nullptr ? home : ".") / ".local" / "share" / "Trash";
        }

        std::string percentEncoded(const std::string &path)
        {
            static const char *s_hex = "0123456789ABCDEF";
            std::string encoded;
            for (unsigned char character : path)
            {
                if (std::isalnum(character) || character == '/' || character == '-' || character == '_' || character == '.' || character == '~')
                {
                    encoded += (char)character;
                }
                else
                {
                    encoded += '%';
                    encoded += s_hex[character >> 4];
                    encoded += s_hex[character & 15];
                }
            }
            return encoded;
        }
    }

    bool moveToTrash(const std::string &path, std::string *error)
    {
        std::error_code failure;
        fs::path source = fs::absolute(path, failure);
        fs::path trash = trashDirectory();
        fs::create_directories(trash / "files", failure);
        fs::create_directories(trash / "info", failure);
        const std::string stem = source.filename().string();
        std::string name = stem;
        for (int number = 2; fs::exists(trash / "files" / name, failure) || fs::exists(trash / "info" / (name + ".trashinfo"), failure); ++number)
        {
            name = stem + "." + std::to_string(number);
        }
        std::time_t now = std::time(nullptr);
        char date[32];
        std::strftime(date, sizeof date, "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
        fs::path info = trash / "info" / (name + ".trashinfo");
        {
            std::ofstream out(info);
            out << "[Trash Info]\nPath=" << percentEncoded(source.string()) << "\nDeletionDate=" << date << "\n";
            if (!out)
            {
                *error = "Could not write to the trash in " + trash.string() + ", so nothing was deleted.";
                return false;
            }
        }
        // A rename keeps the file whole; across file systems it fails, and the file stays where it was rather than be lost.
        fs::rename(source, trash / "files" / name, failure);
        if (failure)
        {
            std::string reason = failure.message();
            std::error_code ignored;
            fs::remove(info, ignored);
            *error = "Could not move " + stem + " to the trash, so it was not deleted: " + reason;
            return false;
        }
        return true;
    }
}
