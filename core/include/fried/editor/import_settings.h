#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>

namespace fried::editor
{
    // How an asset is prepared for each platform, kept in a sidecar next to it ("sprite.png.import") that the engine's import tool reads when it builds. The settings are the sidecar's JSON: the top level applies everywhere, and "platforms" holds a block for each platform that is laid over it.
    using ImportSettings = nlohmann::json;

    // The sidecar's path for an asset file.
    std::string importSidecarPath(const std::string &assetPath);

    // Whether a file name is a sidecar, which the asset lists leave out.
    bool isImportSidecar(const std::string &fileName);

    // The settings of an asset, or none at all when it has no sidecar or the sidecar is not valid.
    ImportSettings loadImportSettings(const std::string &assetPath);

    // Writes the settings after leaving out what holds nothing, and removes the sidecar when nothing is left.
    bool saveImportSettings(const std::string &assetPath, ImportSettings settings, std::string *error);

    // A field of a group ("image" or "audio") for a platform: its own block's value when it has one, else the top level's, else the fallback. An empty platform is the top level.
    nlohmann::json importValue(const ImportSettings &settings, const std::string &platform, const std::string &group, const std::string &field, const nlohmann::json &fallback);

    // Whether a platform's block holds anything for the group.
    bool hasImportOverride(const ImportSettings &settings, const std::string &platform, const std::string &group);

    void setImportValue(ImportSettings *settings, const std::string &platform, const std::string &group, const std::string &field, const nlohmann::json &value);

    // Takes a platform's block of the group away, so the platform uses the top level's.
    void clearImportOverride(ImportSettings *settings, const std::string &platform, const std::string &group);

    // Gives a platform a block for the group that holds every field of the defaults, with the top level's values where it has them, so it can be edited.
    void startImportOverride(ImportSettings *settings, const std::string &platform, const std::string &group, const nlohmann::json &defaults);

    // What a group holds when nothing is said.
    const nlohmann::json &importDefaults(const std::string &group);

    // What a file holds, read from its header.
    struct ImageInfo
    {
        bool m_isValid = false;
        int m_width = 0;
        int m_height = 0;
        std::uint64_t m_bytes = 0;
    };
    ImageInfo readImageInfo(const std::string &path);

    struct SoundInfo
    {
        bool m_isValid = false;
        int m_channels = 0;
        int m_sampleRate = 0;
        int m_bits = 0;
        double m_seconds = 0;
        std::uint64_t m_bytes = 0;
    };
    SoundInfo readSoundInfo(const std::string &path);

    struct FontInfo
    {
        bool m_isValid = false;
        std::uint64_t m_bytes = 0;
        // What the file comes to on the platforms that read a whole font into RAM, once the build has left out the variation and substitution tables the engine never reads. The same as m_bytes for a font with none, or that is not a single TrueType font.
        std::uint64_t m_bytesOnSoftwareText = 0;
    };
    FontInfo readFontInfo(const std::string &path);
}
