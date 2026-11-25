#include "fried/editor/trash.h"

#include <windows.h>
#include <shellapi.h>

#include <filesystem>

namespace fried::editor
{
    bool moveToTrash(const std::string &path, std::string *error)
    {
        std::filesystem::path source = std::filesystem::absolute(std::filesystem::u8path(path));
        // SHFileOperation reads a list of paths that ends with two nulls.
        std::wstring from = source.wstring();
        from.push_back(L'\0');
        SHFILEOPSTRUCTW operation = {};
        operation.wFunc = FO_DELETE;
        operation.pFrom = from.c_str();
        operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
        if (SHFileOperationW(&operation) != 0 || operation.fAnyOperationsAborted)
        {
            *error = "Could not move " + source.filename().u8string() + " to the Recycle Bin, so it was not deleted.";
            return false;
        }
        return true;
    }
}
