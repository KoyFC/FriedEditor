#include "doctest/doctest.h"

#include "fried/editor/asset_validation.h"
#include "fried/editor/image_io.h"

#include <cstdint>
#include <filesystem>
#include <fstream>

using namespace fried::editor;
namespace fs = std::filesystem;

namespace
{
    struct Project
    {
        fs::path m_root = fs::temp_directory_path() / "fried-editor-validation-test";

        Project()
        {
            fs::remove_all(m_root);
            fs::create_directories(m_root / "assets");
        }

        ~Project() { fs::remove_all(m_root); }

        std::string assets() const { return (m_root / "assets").string(); }
        std::string backups() const { return (m_root / "backup").string(); }

        void image(const std::string &name, int width, int height) const
        {
            std::vector<unsigned char> pixels((size_t)width * height * 4, 200);
            REQUIRE(writeImage((m_root / "assets" / name).string(), pixels.data(), width, height));
        }

        // A 16 bit PCM WAV with a few frames of silence.
        void wav(const std::string &name, int channels, int format = 1) const
        {
            std::vector<unsigned char> bytes = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0};
            auto put16 = [&](int v) { bytes.push_back(v & 255); bytes.push_back(v >> 8); };
            auto put32 = [&](std::uint32_t v) { for (int s = 0; s < 32; s += 8) bytes.push_back((unsigned char)(v >> s)); };
            put16(format);
            put16(channels);
            put32(22050);
            put32(22050 * 2 * channels);
            put16(2 * channels);
            put16(16);
            bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
            put32(40 * channels);
            for (int i = 0; i < 20 * channels; ++i)
            {
                put16(i % 2 == 0 ? 1000 : 3000);
            }
            std::ofstream((m_root / "assets" / name).string(), std::ios::binary).write((const char *)bytes.data(), (std::streamsize)bytes.size());
        }
    };

    int count(const std::vector<AssetIssue> &issues, AssetIssue::Fix fix)
    {
        int found = 0;
        for (const AssetIssue &issue : issues)
        {
            found += issue.m_fix == fix;
        }
        return found;
    }
}

TEST_CASE("a texture over 1024 pixels is refused on the 3DS and DS, shrunk by its fix, and fine on the PC")
{
    Project project;
    project.image("big.png", 2048, 1024);
    project.image("small.png", 64, 64);

    CHECK(validateAssets(Platform::Pc, project.assets()).empty());

    auto issues = validateAssets(Platform::Nintendo3ds, project.assets());
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].m_asset == "game/big.png");
    CHECK(issues[0].m_fix == AssetIssue::Fix::ShrinkImage);

    std::string error;
    REQUIRE(applyFix(issues[0], project.assets(), project.backups(), &error));
    int width = 0, height = 0;
    REQUIRE(imageSize(project.assets() + "/big.png", &width, &height));
    CHECK(width == 1024);
    CHECK(height == 512);
    CHECK(validateAssets(Platform::Nintendo3ds, project.assets()).empty());

    CHECK(imageSize(project.backups() + "/big.png", &width, &height));
    CHECK(width == 2048);
}

TEST_CASE("the DS wants mono PCM WAV, and a stereo file is converted")
{
    Project project;
    project.wav("stereo.wav", 2);
    project.wav("mono.wav", 1);
    project.wav("adpcm.wav", 1, 2);
    std::ofstream(project.assets() + "/music.ogg") << "not a wav";

    auto issues = validateAssets(Platform::NintendoDs, project.assets());
    CHECK(issues.size() == 3);
    CHECK(count(issues, AssetIssue::Fix::DownmixWav) == 1);
    CHECK(validateAssets(Platform::Pc, project.assets()).empty());

    AssetIssue stereo;
    for (const AssetIssue &issue : issues)
    {
        if (issue.m_fix == AssetIssue::Fix::DownmixWav)
        {
            stereo = issue;
        }
    }
    CHECK(stereo.m_asset == "game/stereo.wav");
    std::string error;
    REQUIRE(applyFix(stereo, project.assets(), project.backups(), &error));
    CHECK(validateAssets(Platform::NintendoDs, project.assets()).size() == 2);
    CHECK(fs::file_size(project.assets() + "/stereo.wav") < fs::file_size(project.backups() + "/stereo.wav"));
}

TEST_CASE("the fx-CG50 warns about audio and about a project that does not fit")
{
    Project project;
    project.wav("beep.wav", 1);
    auto issues = validateAssets(Platform::Cg50, project.assets());
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].m_severity == AssetIssue::Severity::Warning);

    std::ofstream big(project.assets() + "/big.bin", std::ios::binary);
    std::vector<char> zeros(3 * 1024 * 1024);
    big.write(zeros.data(), (std::streamsize)zeros.size());
    big.close();
    issues = validateAssets(Platform::Cg50, project.assets());
    REQUIRE(issues.size() == 2);
    CHECK(issues[1].m_asset.empty());
    CHECK(issues[1].m_severity == AssetIssue::Severity::Error);

    std::string error;
    CHECK_FALSE(applyFix(issues[1], project.assets(), project.backups(), &error));
}
