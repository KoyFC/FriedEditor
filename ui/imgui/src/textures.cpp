#include "textures.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb/stb_image.h>

#include <GL/gl.h>

namespace imgui_backend
{
    Texture::~Texture()
    {
        if (m_texture)
        {
            glDeleteTextures(1, &m_texture);
        }
    }

    void Texture::upload(const unsigned char *rgba, int width, int height)
    {
        if (!m_texture)
        {
            glGenTextures(1, &m_texture);
        }
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        m_width = width;
        m_height = height;
    }

    ImTextureID Texture::id() const
    {
        return (ImTextureID)(intptr_t)m_texture;
    }

    const Texture *ImageCache::get(const std::string &file)
    {
        auto found = m_images.find(file);
        if (found != m_images.end())
        {
            return &found->second;
        }
        if (m_failed.count(file))
        {
            return nullptr;
        }

        int width = 0, height = 0, channels = 0;
        unsigned char *pixels = stbi_load(file.c_str(), &width, &height, &channels, 4);
        if (!pixels)
        {
            m_failed[file] = true;
            return nullptr;
        }
        Texture &texture = m_images[file];
        texture.upload(pixels, width, height);
        stbi_image_free(pixels);
        return &texture;
    }
}
