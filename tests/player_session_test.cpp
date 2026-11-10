#include "doctest/doctest.h"

#include "fried/editor/player_builder.h"
#include "fried/editor/player_session.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <thread>

using namespace fried::editor;

namespace
{
    void pumpUntil(PlayerSession &session, const std::function<bool()> &done)
    {
        for (int attempt = 0; attempt < 400 && !done(); ++attempt)
        {
            session.pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    std::vector<Message> waitFor(MessageChannel &channel, std::size_t count)
    {
        std::vector<Message> all;
        for (int attempt = 0; attempt < 400 && all.size() < count; ++attempt)
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

TEST_CASE("a session greets the player, learns the schema and relays requests")
{
    PlayerSession session;
    std::string error;
    REQUIRE(session.listen(&error));

    std::string reported;
    session.m_onError = [&](const std::string &message) { reported = message; };
    Frame received;
    session.m_onFrame = [&](const Frame &frame) { received = frame; };

    // What the real player does, played by the test.
    MessageChannel player(connectToLoopback(session.port(), &error));
    REQUIRE(player.send({{"type", "hello"}, {"protocol", PlayerSession::s_protocol}, {"platform", "pc"}}));

    std::vector<Message> asked;
    for (int attempt = 0; attempt < 400 && asked.empty(); ++attempt)
    {
        session.pump();
        asked = player.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(asked.size() == 1);
    CHECK(asked[0].m_header["type"] == "schema");

    nlohmann::json flip = {{"name", "flip"}, {"kind", "flip"}, {"values", nlohmann::json::array({"None", "Horizontal"})}, {"defaultValue", "None"}};
    nlohmann::json sprite = {{"name", "Sprite"}, {"fields", nlohmann::json::array({flip})}};
    REQUIRE(player.send({{"type", "schema"}, {"components", nlohmann::json::array({sprite})}}));
    pumpUntil(session, [&] { return session.state() == PlayerSession::State::Ready; });
    REQUIRE(session.state() == PlayerSession::State::Ready);
    REQUIRE(session.schema().size() == 1);
    CHECK(session.schema()[0].m_name == "Sprite");
    CHECK(session.schema()[0].m_fields[0].m_values.size() == 2);
    CHECK(session.schema()[0].m_fields[0].m_default == "None");

    session.setField("player", "Transform", "x", 12);
    session.render();
    auto requests = waitFor(player, 2);
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].m_header["field"] == "x");
    CHECK(requests[1].m_header["type"] == "render");

    nlohmann::json bounds = {{"id", "player"}, {"x", 1.5}, {"y", 2}, {"width", 3}, {"height", 4}};
    nlohmann::json frame = {{"type", "frame"}, {"width", 1}, {"height", 1}, {"bounds", nlohmann::json::array({bounds})}};
    REQUIRE(player.send(frame));
    pumpUntil(session, [&] { return received.m_width != 0; });
    REQUIRE(received.m_bounds.size() == 1);
    CHECK(received.m_bounds[0].m_x == 1.5);
    CHECK(reported.empty());
}

TEST_CASE("a player on another protocol is refused with an explanation")
{
    PlayerSession session;
    std::string error;
    REQUIRE(session.listen(&error));
    std::string reported;
    session.m_onError = [&](const std::string &message) { reported = message; };

    MessageChannel player(connectToLoopback(session.port(), &error));
    REQUIRE(player.send({{"type", "hello"}, {"protocol", 99}}));
    pumpUntil(session, [&] { return !reported.empty(); });

    CHECK(session.state() == PlayerSession::State::Closed);
    CHECK(reported.find("Rebuild the player") != std::string::npos);
}

TEST_CASE("a build runs its steps in order and stops at the first that fails")
{
    auto finish = [](PlayerBuilder &builder) {
        for (int attempt = 0; attempt < 400 && builder.pump() == PlayerBuilder::State::Building; ++attempt)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return builder.pump();
    };

    PlayerBuilder good({{{"sh", "-c", "exit 0"}, {}, "", ""}, {{"sh", "-c", "exit 0"}, {}, "", ""}});
    CHECK(finish(good) == PlayerBuilder::State::Succeeded);

    PlayerBuilder bad({{{"sh", "-c", "exit 0"}, {}, "", ""}, {{"sh", "-c", "exit 4"}, {}, "", ""}, {{"sh", "-c", "exit 0"}, {}, "", ""}});
    CHECK(finish(bad) == PlayerBuilder::State::Failed);
    CHECK(bad.failure().find("step 2 of 3") != std::string::npos);
}

// Runs only when FRIED_ENGINE points at an engine checkout whose player is built, since that is a build of another repository.
TEST_CASE("the real player answers a scene with a frame" * doctest::skip(std::getenv("FRIED_ENGINE") == nullptr))
{
    std::string engine = std::getenv("FRIED_ENGINE");
    PlayerSession session;
    std::string error;
    REQUIRE(session.listen(&error));
    REQUIRE(session.launch(engine + "/editor/player/build/fried_editor_player", engine, engine + "/sandbox", 640, 480, &error));

    Frame received;
    std::string reported;
    session.m_onFrame = [&](const Frame &frame) { received = frame; };
    session.m_onError = [&](const std::string &message) { reported = message; };

    pumpUntil(session, [&] { return session.state() == PlayerSession::State::Ready || !reported.empty(); });
    REQUIRE(reported.empty());
    REQUIRE(session.state() == PlayerSession::State::Ready);
    CHECK(session.platform() == "pc");
    CHECK(session.schema().size() == 6);

    std::ifstream file(engine + "/sandbox/assets/scenes/sample.scene.json");
    nlohmann::json scene = nlohmann::json::parse(file);
    session.load(scene, "");
    session.setEditorCamera(320, 240, 1);
    session.render();
    pumpUntil(session, [&] { return received.m_width != 0 || !reported.empty(); });

    CHECK(reported.empty());
    CHECK(received.m_width == 640);
    CHECK(received.m_rgba.size() == 640u * 480u * 4u);
    CHECK(received.m_bounds.size() == 3);
}
