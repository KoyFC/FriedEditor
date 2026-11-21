#pragma once

#include "fried/core/line_kind.h"

#include <cstdint>
#include <deque>
#include <string>

namespace fried::editor
{
    // A text file that another process is writing, read as it grows: the whole lines that came since the last poll, each with what kind of line it is. A file that shrinks was started over, so what was read of it is dropped.
    class LogTail
    {
    public:
        static constexpr std::size_t s_maxLines = 10000;

        // Follows another file from its start, or none. The same path keeps what was read.
        void follow(const std::string &path);

        // Reads what was added. True when the lines changed.
        bool poll();

        // Forgets the lines, and what the file holds now is not read again.
        void clear();

        const std::deque<fried::core::OutputLine> &lines() const { return m_lines; }

        // Changes every time the lines do, for what is derived from them.
        unsigned long revision() const { return m_revision; }
        int errorCount() const { return m_errors; }
        int warningCount() const { return m_warnings; }

    private:
        void add(const std::string &text);
        void reset();

        std::string m_path;
        std::uintmax_t m_offset = 0;
        std::string m_partial;
        std::deque<fried::core::OutputLine> m_lines;
        unsigned long m_revision = 0;
        int m_errors = 0;
        int m_warnings = 0;
    };
}
