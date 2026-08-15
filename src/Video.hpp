#pragma once

#include "Media.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#include <filesystem>


namespace fs = std::filesystem;

class Video : public Media {
public:
    Video(const fs::path &path);

    ~Video() override;

    // Media methods
    ImageData getImageData() override;
    MetaData getMetaData() override;

protected:
    int width_;
    int height_;
    int orientation_ = 0;

    AVFormatContext* fmt_ctx;
    int video_stream_idx;
    AVCodecContext* codec_ctx;
    AVPacket* packet;
    AVFrame* frame;

    SwsContext* sws_ctx = nullptr;
    uint8_t* rgb_data[4] = { nullptr };
    int rgb_linesize[4] = { 0 };
    int rgb_bufsize = 0;

    MetaData metaData_;
};
