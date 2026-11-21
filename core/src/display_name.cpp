#include "fried/editor/display_name.h"

#include <cctype>

namespace fried::editor
{
    std::string displayName(const std::string &name)
    {
        std::string result;
        for (std::size_t index = 0; index < name.size(); ++index)
        {
            unsigned char letter = (unsigned char)name[index];
            if (letter == '_')
            {
                if (!result.empty() && result.back() != ' ')
                {
                    result += ' ';
                }
                continue;
            }
            if (index > 0 && std::isupper(letter))
            {
                unsigned char before = (unsigned char)name[index - 1];
                bool isAfterLower = std::islower(before) || std::isdigit(before);
                bool isEndOfAcronym = std::isupper(before) && index + 1 < name.size() && std::islower((unsigned char)name[index + 1]);
                if ((isAfterLower || isEndOfAcronym) && !result.empty() && result.back() != ' ')
                {
                    result += ' ';
                }
            }
            result += (char)(result.empty() || result.back() == ' ' ? std::toupper(letter) : letter);
        }
        return result;
    }
}
