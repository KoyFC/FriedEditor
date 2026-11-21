#include "doctest/doctest.h"

#include "fried/editor/log_tail.h"

#include <filesystem>
#include <fstream>

using namespace fried::editor;
namespace fs = std::filesystem;

TEST_CASE("a log is read as it grows, by whole lines, and starts over when it is truncated")
{
    fs::path file = fs::temp_directory_path() / "fried-editor-log-tail-test.log";
    fs::remove(file);

    LogTail tail;
    tail.follow(file.string());
    CHECK(!tail.poll());

    std::ofstream(file) << "one\r\nsrc/Main.hx:3: characters 1-2 : error: bad\nhalf";
    CHECK(tail.poll());
    REQUIRE(tail.lines().size() == 2);
    CHECK(tail.lines()[0].m_text == "one");
    CHECK(tail.lines()[1].m_kind == fried::core::LineKind::Error);
    CHECK(tail.errorCount() == 1);
    CHECK(!tail.poll());

    // The rest of a line that was half written completes it.
    std::ofstream(file, std::ios::app) << " done\nwarning: careful\n";
    unsigned long revision = tail.revision();
    CHECK(tail.poll());
    CHECK(tail.revision() != revision);
    REQUIRE(tail.lines().size() == 4);
    CHECK(tail.lines()[2].m_text == "half done");
    CHECK(tail.warningCount() == 1);

    // Clearing forgets the lines and does not read them again, only what comes after.
    tail.clear();
    CHECK(tail.lines().empty());
    CHECK(tail.errorCount() == 0);
    CHECK(!tail.poll());
    std::ofstream(file, std::ios::app) << "after\n";
    CHECK(tail.poll());
    REQUIRE(tail.lines().size() == 1);
    CHECK(tail.lines()[0].m_text == "after");

    // The terminal colors are dropped, and the engine's levels give the kind of a line.
    std::ofstream(file, std::ios::app) << "\x1b[37m[0 | 0.001s] [WARN] low\x1b[0m\n\x1b[31m[1 | 0.002s] [ERROR] bad\x1b[0m\n";
    CHECK(tail.poll());
    REQUIRE(tail.lines().size() == 3);
    CHECK(tail.lines()[1].m_text == "[0 | 0.001s] [WARN] low");
    CHECK(tail.lines()[1].m_kind == fried::core::LineKind::Warning);
    CHECK(tail.lines()[2].m_kind == fried::core::LineKind::Error);

    // A shorter file is a new one.
    std::ofstream(file, std::ios::trunc) << "new\n";
    CHECK(tail.poll());
    REQUIRE(tail.lines().size() == 1);
    CHECK(tail.lines()[0].m_text == "new");

    fs::remove(file);
}
