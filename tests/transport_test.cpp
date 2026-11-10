#include "doctest/doctest.h"

#include "fried/editor/message_channel.h"

#include <chrono>
#include <thread>

using namespace fried::editor;

namespace
{
    struct Link
    {
        std::unique_ptr<Listener> m_listener;
        MessageChannel m_editor;
        MessageChannel m_player;
    };

    // The editor listens and the player connects, as they do for real.
    Link connectedPair()
    {
        std::string error;
        auto listener = listenOnLoopback(&error);
        REQUIRE(listener);
        auto playerEnd = connectToLoopback(listener->port(), &error);
        REQUIRE(playerEnd);
        std::unique_ptr<Transport> editorEnd;
        for (int attempt = 0; attempt < 100 && !editorEnd; ++attempt)
        {
            editorEnd = listener->accept();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        REQUIRE(editorEnd);
        return {std::move(listener), MessageChannel(std::move(editorEnd)), MessageChannel(std::move(playerEnd))};
    }

    std::vector<Message> waitFor(MessageChannel &channel, std::size_t count)
    {
        std::vector<Message> all;
        for (int attempt = 0; attempt < 200 && all.size() < count; ++attempt)
        {
            for (Message &message : channel.poll())
            {
                all.push_back(std::move(message));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return all;
    }
}

TEST_CASE("messages cross the loopback in both directions")
{
    Link link = connectedPair();

    REQUIRE(link.m_editor.send({{"type", "load"}, {"platform", "nds"}}));
    REQUIRE(link.m_editor.send({{"type", "render"}}));
    auto atPlayer = waitFor(link.m_player, 2);
    REQUIRE(atPlayer.size() == 2);
    CHECK(atPlayer[0].m_header["type"] == "load");
    CHECK(atPlayer[1].m_header["type"] == "render");

    REQUIRE(link.m_player.send({{"type", "hello"}, {"protocol", 1}}));
    auto atEditor = waitFor(link.m_editor, 1);
    REQUIRE(atEditor.size() == 1);
    CHECK(atEditor[0].m_header["protocol"] == 1);
}

TEST_CASE("a message with pixels arrives whole, even when it is large")
{
    // The player writes the JSON and then the raw bytes, so this does the same.
    std::vector<unsigned char> bytes(1228800);
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        bytes[index] = (unsigned char)(index * 7);
    }
    nlohmann::json header = {{"type", "frame"}, {"pixels", (int)bytes.size()}};
    std::string text = header.dump();
    std::vector<unsigned char> framed = {(unsigned char)text.size(), 0, 0, 0};
    framed.insert(framed.end(), text.begin(), text.end());
    framed.insert(framed.end(), bytes.begin(), bytes.end());

    std::string error;
    auto listener = listenOnLoopback(&error);
    auto sender = connectToLoopback(listener->port(), &error);
    std::unique_ptr<Transport> receiver;
    for (int attempt = 0; attempt < 100 && !receiver; ++attempt)
    {
        receiver = listener->accept();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(receiver);
    MessageChannel channel(std::move(receiver));

    std::thread writer([&] { sender->send(framed.data(), framed.size()); });
    auto frames = waitFor(channel, 1);
    writer.join();

    REQUIRE(frames.size() == 1);
    CHECK(frames[0].m_header["type"] == "frame");
    CHECK(frames[0].m_pixels == bytes);
}

TEST_CASE("a channel notices that the other end has gone")
{
    Link link = connectedPair();
    {
        MessageChannel gone = std::move(link.m_player);
    }
    for (int attempt = 0; attempt < 200 && link.m_editor.isOpen(); ++attempt)
    {
        link.m_editor.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK_FALSE(link.m_editor.isOpen());
}

TEST_CASE("a subprocess reports its exit code, with variables added to its environment")
{
    std::string error;
    auto process = startSubprocess({{"sh", "-c", "exit $FRIED_TEST_CODE"}, {{"FRIED_TEST_CODE", "3"}}, "", ""}, &error);
    REQUIRE(process);
    for (int attempt = 0; attempt < 200 && process->isRunning(); ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK_FALSE(process->isRunning());
    CHECK(process->exitCode() == 3);

    CHECK_FALSE(startSubprocess({{"/nonexistent/program"}, {}, "", ""}, &error));
}

TEST_CASE("connecting to a port nobody listens on fails with a reason")
{
    std::string error;
    CHECK_FALSE(connectToLoopback(1, &error));
    CHECK_FALSE(error.empty());
}
