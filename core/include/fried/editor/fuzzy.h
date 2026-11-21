#pragma once

#include <string>
#include <vector>

namespace fried::editor
{
    // How well a query matches a text when its letters have to appear in the text in order, though not side by side: -1 when they do not, and otherwise a score that is higher for letters that are together, that start a word or a file name, and for shorter texts. Case does not matter, and a match in the file's own name counts for more than one in the folders before it.
    int fuzzyScore(const std::string &query, const std::string &text);

    // The positions of the candidates that match, the best first and in their own order among equals. An empty query matches them all.
    std::vector<std::size_t> fuzzyRank(const std::string &query, const std::vector<std::string> &candidates);
}
