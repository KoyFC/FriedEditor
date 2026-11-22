#include "shell.h"
#include "doctest/doctest.h"

#include "fried/editor/game_session.h"
#include "fried/editor/player_session.h"

#include <chrono>
#include <thread>

using namespace fried::editor;

namespace
{
    std::vector<Message> pumpAndPoll(GameSession &session, MessageChannel &game, std::size_t count)
    {
        std::vector<Message> all;
        for (int attempt = 0; attempt < 400 && all.size() < count; ++attempt)
        {
            session.pump();
            for (Message &message : game.poll())
            {
                all.push_back(std::move(message));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return all;
    }
}

TEST_CASE("a game is told what to play when it greets, and its frames are answered and delivered with their stats")
{
    GameSession session;
    std::string error;
    nlohmann::json scene = {{"name", "Main"}, {"nodes", nlohmann::json::array()}};
    REQUIRE(session.listen("main", scene, false, &error));

    GameFrame received;
    int frames = 0;
    session.m_onFrame = [&](const GameFrame &frame) {
        received = frame;
        ++frames;
    };

    // What the real game does, played by the test.
    MessageChannel game(connectToLoopback(session.port(), &error));
    REQUIRE(game.send({{"type", "hello"}, {"protocol", PlayerSession::s_protocol}, {"platform", "pc"}}));

    std::vector<Message> asked = pumpAndPoll(session, game, 4);
    REQUIRE(asked.size() == 4);
    CHECK(asked[0].m_header["type"] == "play");
    CHECK(asked[0].m_header["name"] == "main");
    CHECK(asked[0].m_header["scene"] == scene);
    CHECK(asked[0].m_header["fromStart"] == false);
    CHECK(asked[1].m_header["type"] == "showGame");
    CHECK(asked[2].m_header["type"] == "frameLimit");
    CHECK(asked[2].m_header["fps"] == -1);
    CHECK(asked[3].m_header["type"] == "watch");
    CHECK(session.state() == GameSession::State::Running);

    nlohmann::json stats = {{"fps", 59.5}, {"frameMs", 16.8}, {"workMs", 2.5}, {"nodes", 16}};
    REQUIRE(game.send({{"type", "gameFrame"}, {"width", 2}, {"height", 1}, {"stats", stats}}));
    std::vector<Message> answered = pumpAndPoll(session, game, 1);
    REQUIRE(answered.size() == 1);
    CHECK(answered[0].m_header["type"] == "frameShown");
    CHECK(frames == 1);
    CHECK(received.m_width == 2);
    CHECK(received.m_stats.m_fps == doctest::Approx(59.5));
    CHECK(received.m_stats.m_nodes == 16);

    session.setFrameLimit(0, 144);
    session.setField("wall", "x", 4);
    session.key(7, true);
    session.mouseButton(1, 10, 20, true);
    std::vector<Message> sent = pumpAndPoll(session, game, 4);
    REQUIRE(sent.size() == 4);
    CHECK(sent[0].m_header["fps"] == 0);
    CHECK(sent[0].m_header["refresh"] == 144);
    CHECK(sent[1].m_header["type"] == "setField");
    CHECK(sent[2].m_header["code"] == 7);
    CHECK(sent[3].m_header["y"] == 20);

    nlohmann::json changes = nlohmann::json::array({{{"id", "wall"}, {"fields", {{"x", 4}}}}});
    nlohmann::json reportedState;
    session.m_onState = [&](const nlohmann::json &nodes) { reportedState = nodes; };
    REQUIRE(game.send({{"type", "state"}, {"nodes", changes}}));
    pumpAndPoll(session, game, 1);
    CHECK(reportedState == changes);

    std::string sceneName;
    session.m_onSceneChanged = [&](const std::string &name) { sceneName = name; };
    REQUIRE(game.send({{"type", "sceneChanged"}, {"name", "sandbox_3d"}}));
    pumpAndPoll(session, game, 1);
    CHECK(sceneName == "sandbox_3d");

    session.setWatch("wall", true);
    std::vector<Message> watching = pumpAndPoll(session, game, 1);
    REQUIRE(watching.size() == 1);
    CHECK(watching[0].m_header["selected"] == "wall");
    CHECK(watching[0].m_header["all"] == true);

    REQUIRE(game.send({{"type", "error"}, {"request", "setField"}, {"message", "The scene has no node \"x\"."}}));
    std::string reported;
    session.m_onError = [&](const std::string &message) { reported = message; };
    pumpAndPoll(session, game, 1);
    CHECK(reported == "The scene has no node \"x\".");
    CHECK(session.state() == GameSession::State::Running);
}

TEST_CASE("a game on another protocol is refused with an explanation")
{
    GameSession session;
    std::string error;
    REQUIRE(session.listen("main", nlohmann::json::object(), true, &error));
    std::string reported;
    session.m_onError = [&](const std::string &message) { reported = message; };

    MessageChannel game(connectToLoopback(session.port(), &error));
    REQUIRE(game.send({{"type", "hello"}, {"protocol", 99}}));
    pumpAndPoll(session, game, 1);
    CHECK(session.state() == GameSession::State::Closed);
    CHECK(reported.find("protocol 99") != std::string::npos);
}
