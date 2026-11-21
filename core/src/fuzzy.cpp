#include "fried/editor/fuzzy.h"

#include <algorithm>
#include <cctype>

namespace fried::editor
{
    namespace
    {
        bool isBoundary(const std::string &text, std::size_t index)
        {
            if (index == 0)
            {
                return true;
            }
            char before = text[index - 1];
            return before == '/' || before == '_' || before == '-' || before == '.' || before == ' ';
        }

        int scoreOf(const std::string &query, const std::string &text)
        {
            std::string lowerQuery = query;
            std::string lowerText = text;
            for (char &letter : lowerQuery)
            {
                letter = (char)std::tolower((unsigned char)letter);
            }
            for (char &letter : lowerText)
            {
                letter = (char)std::tolower((unsigned char)letter);
            }

            int score = 0;
            std::size_t position = 0;
            bool wasPrevious = false;
            for (char letter : lowerQuery)
            {
                std::size_t found = lowerText.find(letter, position);
                if (found == std::string::npos)
                {
                    return -1;
                }
                score += 1;
                if (found == position && wasPrevious)
                {
                    score += 10;
                }
                if (isBoundary(lowerText, found))
                {
                    score += 8;
                }
                score -= (int)std::min<std::size_t>(found - position, 5);
                position = found + 1;
                wasPrevious = true;
            }
            return score - (int)(text.size() / 16);
        }
    }

    int fuzzyScore(const std::string &query, const std::string &text)
    {
        if (query.empty())
        {
            return 0;
        }
        std::size_t slash = text.rfind('/');
        int inName = slash == std::string::npos ? -1 : scoreOf(query, text.substr(slash + 1));
        int whole = scoreOf(query, text);
        if (slash == std::string::npos)
        {
            return whole;
        }
        return inName >= 0 ? std::max(whole, inName + 20) : whole;
    }

    std::vector<std::size_t> fuzzyRank(const std::string &query, const std::vector<std::string> &candidates)
    {
        std::vector<std::pair<int, std::size_t>> matches;
        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            int score = fuzzyScore(query, candidates[index]);
            if (score >= 0 || query.empty())
            {
                matches.push_back({score, index});
            }
        }
        std::stable_sort(matches.begin(), matches.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
        std::vector<std::size_t> ranked;
        for (const auto &match : matches)
        {
            ranked.push_back(match.second);
        }
        return ranked;
    }
}
