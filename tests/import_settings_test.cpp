#include "doctest/doctest.h"

#include "fried/editor/asset_browser.h"
#include "fried/editor/asset_files.h"
#include "fried/editor/import_settings.h"

#include <filesystem>
#include <fstream>

using namespace fried::editor;
namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace
{
    fs::path assets()
    {
        fs::path directory = fs::temp_directory_path() / "fried-editor-import-settings-test";
        fs::remove_all(directory);
        fs::create_directories(directory / "scenes");
        std::ofstream(directory / "hero.png") << "x";
        return directory;
    }
}

TEST_CASE("a platform's value is its own block's, then the top level's, then the fallback")
{
    ImportSettings settings = Json::object();
    CHECK(importValue(settings, "3ds", "image", "maxSize", 0) == 0);
    setImportValue(&settings, "", "image", "maxSize", 1024);
    CHECK(importValue(settings, "3ds", "image", "maxSize", 0) == 1024);
    CHECK_FALSE(hasImportOverride(settings, "3ds", "image"));
    startImportOverride(&settings, "3ds", "image", importDefaults("image"));
    CHECK(hasImportOverride(settings, "3ds", "image"));
    setImportValue(&settings, "3ds", "image", "maxSize", 256);
    CHECK(importValue(settings, "3ds", "image", "maxSize", 0) == 256);
    CHECK(importValue(settings, "vita", "image", "maxSize", 0) == 1024);
    CHECK(importValue(settings, "", "image", "maxSize", 0) == 1024);
    clearImportOverride(&settings, "3ds", "image");
    CHECK(importValue(settings, "3ds", "image", "maxSize", 0) == 1024);
}

TEST_CASE("settings are written without what holds nothing, and the sidecar goes when nothing is left")
{
    fs::path directory = assets();
    std::string asset = (directory / "hero.png").string();
    CHECK(loadImportSettings(asset).empty());

    ImportSettings settings = Json::object();
    setImportValue(&settings, "", "image", "filter", "nearest");
    setImportValue(&settings, "", "image", "maxSize", 0);
    setImportValue(&settings, "nds", "audio", "mono", true);
    clearImportOverride(&settings, "nds", "audio");
    std::string error;
    REQUIRE(saveImportSettings(asset, settings, &error));
    CHECK(fs::exists(importSidecarPath(asset)));
    Json saved = loadImportSettings(asset);
    CHECK(saved == Json{{"image", {{"filter", "nearest"}}}});

    setImportValue(&saved, "", "image", "filter", "default");
    REQUIRE(saveImportSettings(asset, saved, &error));
    CHECK_FALSE(fs::exists(importSidecarPath(asset)));
}

TEST_CASE("an override keeps a value that is the default, since it overrides the top level")
{
    fs::path directory = assets();
    std::string asset = (directory / "hero.png").string();
    ImportSettings settings = Json::object();
    setImportValue(&settings, "", "image", "maxSize", 512);
    startImportOverride(&settings, "pc", "image", importDefaults("image"));
    CHECK(importValue(settings, "pc", "image", "maxSize", -1) == 512);
    setImportValue(&settings, "pc", "image", "maxSize", 0);
    std::string error;
    REQUIRE(saveImportSettings(asset, settings, &error));
    CHECK(importValue(loadImportSettings(asset), "pc", "image", "maxSize", -1) == 0);
    CHECK(importValue(loadImportSettings(asset), "web", "image", "maxSize", -1) == 512);
}

TEST_CASE("the sidecar follows its file when it is moved, copied or deleted, and the lists leave it out")
{
    fs::path directory = assets();
    std::string asset = (directory / "hero.png").string();
    ImportSettings settings = Json::object();
    setImportValue(&settings, "", "image", "filter", "linear");
    std::string error;
    REQUIRE(saveImportSettings(asset, settings, &error));

    CHECK(listAssets("game", directory.string()).size() == 1);

    int rewritten = 0;
    REQUIRE(moveAsset(directory.string(), (directory / "scenes").string(), "hero.png", "knight.png", &rewritten, &error));
    CHECK_FALSE(fs::exists(directory / "hero.png.import"));
    CHECK(fs::exists(directory / "knight.png.import"));

    REQUIRE(copyAsset(directory.string(), "knight.png", "copy.png", &error));
    CHECK(importValue(loadImportSettings((directory / "copy.png").string()), "", "image", "filter", "") == "linear");

    REQUIRE(deleteAsset(directory.string(), "knight.png", &error));
    CHECK_FALSE(fs::exists(directory / "knight.png.import"));
    CHECK(fs::exists(directory / "copy.png.import"));
}

TEST_CASE("a PNG and a WAV report what their headers say")
{
    fs::path directory = assets();
    const unsigned char png[] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0, 16, 0, 0, 0, 8, 8, 6, 0, 0, 0};
    std::ofstream((directory / "a.png"), std::ios::binary).write((const char *)png, sizeof(png));
    ImageInfo image = readImageInfo((directory / "a.png").string());
    CHECK(image.m_isValid);
    CHECK(image.m_width == 16);
    CHECK(image.m_height == 8);
    CHECK_FALSE(readImageInfo((directory / "hero.png").string()).m_isValid);

    std::vector<unsigned char> wav = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0, 0x44, 0xac, 0, 0, 0x10, 0xb1, 2, 0, 4, 0, 16, 0,
                                      'd', 'a', 't', 'a', 0x10, 0xb1, 2, 0};
    wav.resize(wav.size() + 0x2b110);
    std::ofstream((directory / "a.wav"), std::ios::binary).write((const char *)wav.data(), (std::streamsize)wav.size());
    SoundInfo sound = readSoundInfo((directory / "a.wav").string());
    CHECK(sound.m_isValid);
    CHECK(sound.m_channels == 2);
    CHECK(sound.m_sampleRate == 44100);
    CHECK(sound.m_bits == 16);
    CHECK(sound.m_seconds == doctest::Approx(1.0).epsilon(0.01));
}
