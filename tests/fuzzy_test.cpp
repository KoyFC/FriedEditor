#include "doctest/doctest.h"

#include "fried/editor/fuzzy.h"

using namespace fried::editor;

TEST_CASE("a query matches when its letters are in the text in order, whatever the case")
{
    CHECK(fuzzyScore("spr", "game/sprites/hero.png") >= 0);
    CHECK(fuzzyScore("HRO", "game/sprites/hero.png") >= 0);
    CHECK(fuzzyScore("hrp", "game/sprites/hero.png") >= 0);
    CHECK(fuzzyScore("orh", "game/sprites/hero.png") == -1);
    CHECK(fuzzyScore("zz", "hero.png") == -1);
    CHECK(fuzzyScore("", "anything") == 0);
}

TEST_CASE("better matches come first: together, at the start of a word, in the file's own name")
{
    std::vector<std::string> candidates = {"game/scenes/boss_arena.scene.json", "game/sprites/enemy.png", "game/scenes/main.scene.json", "engine/NunitoSans.ttf"};

    std::vector<std::size_t> ranked = fuzzyRank("main", candidates);
    REQUIRE(!ranked.empty());
    CHECK(ranked[0] == 2);

    // Letters that are together beat letters that are spread out, and a match in the name beats one in the folders.
    ranked = fuzzyRank("scene", candidates);
    REQUIRE(ranked.size() >= 2);
    CHECK((ranked[0] == 0 || ranked[0] == 2));
    CHECK(fuzzyRank("nun", candidates)[0] == 3);

    // Without a query everything is there in its own order.
    CHECK(fuzzyRank("", candidates) == std::vector<std::size_t>({0, 1, 2, 3}));
    CHECK(fuzzyRank("qqq", candidates).empty());
}
