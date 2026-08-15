#pragma once

#include <chrono>


struct MetaData {
    // creation time from file or meta data
    std::chrono::time_point<std::chrono::file_clock> time;

    // true if time is local time, false if time is UTC
    bool localTime = false;

    // geo location (0 if invalid)
    double latitude = 0;
    double longitude = 0;

    // frame duration in milliseconds or 0 for still images
    int frameDuration = 0;
};

struct ImageData {
    // image size
    int width, height;

    // image orientation, see http://jpegclub.org/exif_orientation.html
    int orientation;

    // pointer to image data, rgb24 format, 3 bytes per pixel, row-major order, top-to-bottom
    unsigned char *data;
};

/// @brief Base class for pictures and videos
class Media {
public:
    virtual ~Media() = default;

    /// @brief Get next image frame.
    /// Note that the data pointer is only valid until the next call to this function.
    /// @return Image data
    virtual ImageData getImageData() = 0;

    virtual MetaData getMetaData() = 0;
};
