#include "fried/editor/import_settings.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace fried::editor
{
    namespace fs = std::filesystem;
    using Json = nlohmann::json;

    namespace
    {
        const char *const s_extension = ".import";

        // The group's object of a platform's block or of the top level, or null when there is none.
        const Json *groupOf(const ImportSettings &settings, const std::string &platform, const std::string &group)
        {
            const Json *scope = &settings;
            if (!platform.empty())
            {
                auto platforms = settings.find("platforms");
                if (platforms == settings.end() || !platforms->is_object())
                {
                    return nullptr;
                }
                auto block = platforms->find(platform);
                if (block == platforms->end() || !block->is_object())
                {
                    return nullptr;
                }
                scope = &*block;
            }
            if (!scope->is_object())
            {
                return nullptr;
            }
            auto found = scope->find(group);
            return found != scope->end() && found->is_object() ? &*found : nullptr;
        }

        Json &groupFor(ImportSettings *settings, const std::string &platform, const std::string &group)
        {
            if (!settings->is_object())
            {
                *settings = Json::object();
            }
            Json *scope = settings;
            if (!platform.empty())
            {
                scope = &(*settings)["platforms"][platform];
            }
            Json &found = (*scope)[group];
            if (!found.is_object())
            {
                found = Json::object();
            }
            return found;
        }

        // What a group holds when the sidecar says nothing, so the top level does not need to say it.
        const Json &defaultsOf(const std::string &group)
        {
            static const Json image = {{"maxSize", 0}, {"filter", "default"}, {"format", "rgba8"}};
            static const Json audio = {{"mono", false}, {"sampleRate", 0}, {"bits", 0}};
            static const Json none = Json::object();
            return group == "image" ? image : (group == "audio" ? audio : none);
        }

        void pruneDefaults(Json *settings)
        {
            for (const char *group : {"image", "audio"})
            {
                auto found = settings->find(group);
                if (found == settings->end() || !found->is_object())
                {
                    continue;
                }
                for (auto field = found->begin(); field != found->end();)
                {
                    const Json &defaults = defaultsOf(group);
                    if (defaults.contains(field.key()) && defaults[field.key()] == field.value())
                    {
                        field = found->erase(field);
                    }
                    else
                    {
                        ++field;
                    }
                }
            }
        }

        // Takes out every empty object, from the inside out.
        void prune(Json *value)
        {
            if (!value->is_object())
            {
                return;
            }
            for (auto child = value->begin(); child != value->end();)
            {
                prune(&*child);
                if (child->is_object() && child->empty())
                {
                    child = value->erase(child);
                }
                else
                {
                    ++child;
                }
            }
        }

        bool readBytes(const std::string &path, std::vector<unsigned char> *bytes, std::size_t limit)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                return false;
            }
            bytes->resize(limit);
            stream.read((char *)bytes->data(), (std::streamsize)limit);
            bytes->resize((std::size_t)stream.gcount());
            return true;
        }

        std::uint32_t little32(const unsigned char *at)
        {
            return at[0] | (at[1] << 8) | (at[2] << 16) | ((std::uint32_t)at[3] << 24);
        }
    }

    std::string importSidecarPath(const std::string &assetPath)
    {
        return assetPath + s_extension;
    }

    const Json &importDefaults(const std::string &group)
    {
        return defaultsOf(group);
    }

    bool isImportSidecar(const std::string &fileName)
    {
        std::size_t length = std::strlen(s_extension);
        return fileName.size() > length && fileName.compare(fileName.size() - length, length, s_extension) == 0;
    }

    ImportSettings loadImportSettings(const std::string &assetPath)
    {
        std::ifstream stream(importSidecarPath(assetPath));
        if (!stream)
        {
            return Json::object();
        }
        Json data = Json::parse(stream, nullptr, false);
        return data.is_object() ? data : Json::object();
    }

    bool saveImportSettings(const std::string &assetPath, ImportSettings settings, std::string *error)
    {
        if (settings.is_object())
        {
            pruneDefaults(&settings);
        }
        prune(&settings);
        std::string sidecar = importSidecarPath(assetPath);
        std::error_code failure;
        if (!settings.is_object() || settings.empty())
        {
            fs::remove(sidecar, failure);
            if (failure)
            {
                *error = "The settings file could not be removed: " + failure.message();
                return false;
            }
            return true;
        }
        std::ofstream stream(sidecar, std::ios::binary | std::ios::trunc);
        stream << settings.dump(2) << "\n";
        if (!stream)
        {
            *error = "The settings file could not be written.";
            return false;
        }
        return true;
    }

    Json importValue(const ImportSettings &settings, const std::string &platform, const std::string &group, const std::string &field, const Json &fallback)
    {
        for (const std::string &scope : {platform, std::string()})
        {
            const Json *object = groupOf(settings, scope, group);
            if (object && object->contains(field))
            {
                return (*object)[field];
            }
            if (platform.empty())
            {
                break;
            }
        }
        return fallback;
    }

    bool hasImportOverride(const ImportSettings &settings, const std::string &platform, const std::string &group)
    {
        const Json *object = groupOf(settings, platform, group);
        return object && !object->empty();
    }

    void setImportValue(ImportSettings *settings, const std::string &platform, const std::string &group, const std::string &field, const Json &value)
    {
        groupFor(settings, platform, group)[field] = value;
    }

    void clearImportOverride(ImportSettings *settings, const std::string &platform, const std::string &group)
    {
        if (platform.empty() || !settings->is_object())
        {
            return;
        }
        auto platforms = settings->find("platforms");
        if (platforms == settings->end() || !platforms->is_object())
        {
            return;
        }
        auto block = platforms->find(platform);
        if (block != platforms->end() && block->is_object())
        {
            block->erase(group);
        }
    }

    void startImportOverride(ImportSettings *settings, const std::string &platform, const std::string &group, const Json &defaults)
    {
        Json values = Json::object();
        for (auto field = defaults.begin(); field != defaults.end(); ++field)
        {
            values[field.key()] = importValue(*settings, std::string(), group, field.key(), field.value());
        }
        groupFor(settings, platform, group) = values;
    }

    ImageInfo readImageInfo(const std::string &path)
    {
        ImageInfo info;
        std::error_code failure;
        std::uint64_t size = fs::file_size(path, failure);
        std::vector<unsigned char> head;
        static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
        if (failure || !readBytes(path, &head, 64) || head.size() < 24 || std::memcmp(head.data(), signature, 8) != 0)
        {
            return info;
        }
        auto bigEndian = [&](std::size_t at) { return ((int)head[at] << 24) | ((int)head[at + 1] << 16) | ((int)head[at + 2] << 8) | head[at + 3]; };
        info.m_isValid = true;
        info.m_width = bigEndian(16);
        info.m_height = bigEndian(20);
        info.m_bytes = size;
        return info;
    }

    SoundInfo readSoundInfo(const std::string &path)
    {
        SoundInfo info;
        std::error_code failure;
        info.m_bytes = fs::file_size(path, failure);
        std::vector<unsigned char> head;
        if (failure || !readBytes(path, &head, 4096) || head.size() < 12 || std::memcmp(head.data(), "RIFF", 4) != 0 || std::memcmp(head.data() + 8, "WAVE", 4) != 0)
        {
            return info;
        }
        std::size_t at = 12;
        std::uint32_t dataSize = 0;
        while (at + 8 <= head.size())
        {
            std::uint32_t size = little32(&head[at + 4]);
            if (std::memcmp(&head[at], "fmt ", 4) == 0 && at + 8 + 16 <= head.size())
            {
                info.m_channels = head[at + 10] | (head[at + 11] << 8);
                info.m_sampleRate = (int)little32(&head[at + 12]);
                info.m_bits = head[at + 22] | (head[at + 23] << 8);
            }
            else if (std::memcmp(&head[at], "data", 4) == 0)
            {
                dataSize = size;
                break;
            }
            at += 8 + size + (size & 1);
        }
        if (info.m_channels > 0 && info.m_sampleRate > 0 && info.m_bits > 0)
        {
            info.m_isValid = true;
            std::uint64_t data = dataSize != 0 ? std::min<std::uint64_t>(dataSize, info.m_bytes) : info.m_bytes;
            info.m_seconds = (double)data / ((double)info.m_channels * info.m_bits / 8.0 * info.m_sampleRate);
        }
        return info;
    }
}
