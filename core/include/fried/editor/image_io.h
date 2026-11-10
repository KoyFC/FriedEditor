#pragma once

#include <string>
#include <vector>

namespace fried::editor
{
    // Reads only the header, so it is cheap. False when the file is not a PNG or JPEG.
    bool imageSize(const std::string &file, int *width, int *height);

    // Writes a PNG, or a JPEG when the name says so.
    bool writeImage(const std::string &file, const unsigned char *rgba, int width, int height);

    // Scales an image down, keeping its proportions, until neither side is longer than maxSide, and writes it to the destination.
    bool shrinkImage(const std::string &source, const std::string &destination, int maxSide, std::string *error);
}
