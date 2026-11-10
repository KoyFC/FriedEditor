#include "fried/editor/image_io.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <algorithm>
#include <cmath>

namespace fried::editor
{
    namespace
    {
        bool endsWith(const std::string &text, const std::string &suffix)
        {
            return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        bool isJpeg(const std::string &file)
        {
            std::string lower = file;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            return endsWith(lower, ".jpg") || endsWith(lower, ".jpeg");
        }
    }

    bool imageSize(const std::string &file, int *width, int *height)
    {
        int channels = 0;
        return stbi_info(file.c_str(), width, height, &channels) != 0;
    }

    bool writeImage(const std::string &file, const unsigned char *rgba, int width, int height)
    {
        if (isJpeg(file))
        {
            return stbi_write_jpg(file.c_str(), width, height, 4, rgba, 92) != 0;
        }
        return stbi_write_png(file.c_str(), width, height, 4, rgba, width * 4) != 0;
    }

    bool shrinkImage(const std::string &source, const std::string &destination, int maxSide, std::string *error)
    {
        int width = 0, height = 0, channels = 0;
        unsigned char *pixels = stbi_load(source.c_str(), &width, &height, &channels, 4);
        if (!pixels)
        {
            *error = "Cannot read " + source + ".";
            return false;
        }

        double ratio = std::min(1.0, (double)maxSide / std::max(width, height));
        int newWidth = std::max(1, (int)std::lround(width * ratio));
        int newHeight = std::max(1, (int)std::lround(height * ratio));

        // Each new pixel is the average of the area of the old image it covers, weighted by alpha so transparent edges do not bleed their colour.
        std::vector<unsigned char> scaled((size_t)newWidth * newHeight * 4);
        for (int y = 0; y < newHeight; ++y)
        {
            int top = y * height / newHeight;
            int bottom = std::max(top + 1, (y + 1) * height / newHeight);
            for (int x = 0; x < newWidth; ++x)
            {
                int left = x * width / newWidth;
                int right = std::max(left + 1, (x + 1) * width / newWidth);
                double red = 0, green = 0, blue = 0, alpha = 0;
                int count = 0;
                for (int sourceY = top; sourceY < bottom; ++sourceY)
                {
                    for (int sourceX = left; sourceX < right; ++sourceX)
                    {
                        const unsigned char *pixel = pixels + ((size_t)sourceY * width + sourceX) * 4;
                        red += pixel[0] * pixel[3];
                        green += pixel[1] * pixel[3];
                        blue += pixel[2] * pixel[3];
                        alpha += pixel[3];
                        ++count;
                    }
                }
                unsigned char *out = scaled.data() + ((size_t)y * newWidth + x) * 4;
                out[0] = alpha > 0 ? (unsigned char)(red / alpha) : 0;
                out[1] = alpha > 0 ? (unsigned char)(green / alpha) : 0;
                out[2] = alpha > 0 ? (unsigned char)(blue / alpha) : 0;
                out[3] = (unsigned char)(alpha / count);
            }
        }
        stbi_image_free(pixels);
        if (!writeImage(destination, scaled.data(), newWidth, newHeight))
        {
            *error = "Cannot write " + destination + ".";
            return false;
        }
        return true;
    }
}
