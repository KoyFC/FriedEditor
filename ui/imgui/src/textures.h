#pragma once

#include <imgui.h>

#include <map>
#include <string>

namespace imgui_backend
{
    // An OpenGL texture that can be replaced with new pixels.
    class Texture
    {
    public:
        Texture() = default;
        ~Texture();
        Texture(const Texture &) = delete;
        Texture &operator=(const Texture &) = delete;

        void upload(const unsigned char *rgba, int width, int height);
        ImTextureID id() const;
        int width() const { return m_width; }
        int height() const { return m_height; }
        bool isEmpty() const { return m_texture == 0; }

    private:
        unsigned int m_texture = 0;
        int m_width = 0;
        int m_height = 0;
    };

    // Images from disk, loaded the first time they are drawn.
    class ImageCache
    {
    public:
        // Null while the file cannot be read.
        const Texture *get(const std::string &file);

    private:
        std::map<std::string, Texture> m_images;
        std::map<std::string, bool> m_failed;
    };
}
