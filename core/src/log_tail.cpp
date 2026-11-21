#include "fried/editor/log_tail.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fried::editor
{
    namespace fs = std::filesystem;

    void LogTail::follow(const std::string &path)
    {
        if (path == m_path)
        {
            return;
        }
        m_path = path;
        reset();
    }

    void LogTail::reset()
    {
        m_offset = 0;
        m_partial.clear();
        m_lines.clear();
        m_errors = 0;
        m_warnings = 0;
        ++m_revision;
    }

    void LogTail::clear()
    {
        std::error_code ignored;
        std::uintmax_t size = m_path.empty() ? 0 : fs::file_size(m_path, ignored);
        reset();
        m_offset = ignored ? 0 : size;
    }

    namespace
    {
        // The colors a program asks the terminal for, which a panel has no use for.
        std::string withoutEscapes(const std::string &text)
        {
            std::string plain;
            for (std::size_t index = 0; index < text.size(); ++index)
            {
                if (text[index] == '\x1b' && index + 1 < text.size() && text[index + 1] == '[')
                {
                    index += 2;
                    while (index < text.size() && !std::isalpha((unsigned char)text[index]))
                    {
                        ++index;
                    }
                    continue;
                }
                plain += text[index];
            }
            return plain;
        }
    }

    void LogTail::add(const std::string &raw)
    {
        std::string text = withoutEscapes(raw);

        // The engine's own log marks the level of each line, and the build tools' lines are told apart by what they say.
        fried::core::LineKind kind = text.find("[ERROR]") != std::string::npos   ? fried::core::LineKind::Error
                                     : text.find("[WARN") != std::string::npos ? fried::core::LineKind::Warning
                                                                                : fried::core::kindOf(text);
        m_errors += kind == fried::core::LineKind::Error;
        m_warnings += kind == fried::core::LineKind::Warning;
        m_lines.push_back({text, kind});
        if (m_lines.size() > s_maxLines)
        {
            for (std::size_t drop = 0; drop < s_maxLines / 10; ++drop)
            {
                m_errors -= m_lines.front().m_kind == fried::core::LineKind::Error;
                m_warnings -= m_lines.front().m_kind == fried::core::LineKind::Warning;
                m_lines.pop_front();
            }
        }
    }

    bool LogTail::poll()
    {
        if (m_path.empty())
        {
            return false;
        }
        std::error_code error;
        std::uintmax_t size = fs::file_size(m_path, error);
        if (error)
        {
            return false;
        }
        bool isChanged = false;
        if (size < m_offset)
        {
            reset();
            isChanged = true;
        }
        if (size == m_offset)
        {
            return isChanged;
        }

        std::ifstream file(m_path, std::ios::binary);
        if (!file)
        {
            return isChanged;
        }
        file.seekg((std::streamoff)m_offset);
        std::vector<char> bytes((std::size_t)(size - m_offset));
        file.read(bytes.data(), (std::streamsize)bytes.size());
        m_offset += (std::uintmax_t)file.gcount();

        m_partial.append(bytes.data(), (std::size_t)file.gcount());
        std::size_t start = 0;
        for (std::size_t end = m_partial.find('\n'); end != std::string::npos; end = m_partial.find('\n', start))
        {
            std::string line = m_partial.substr(start, end - start);
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            add(line);
            isChanged = true;
            start = end + 1;
        }
        m_partial.erase(0, start);
        if (isChanged)
        {
            ++m_revision;
        }
        return isChanged;
    }
}
