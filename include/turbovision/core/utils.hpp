#pragma once

#include <functional>

#include "common.hpp"
#include <string>
#include <vector>

namespace turbovision {
namespace utils {

// Funções de conversão de formato de pixel
inline AVPixelFormat stringToPixelFormat(const std::string& format) {
    if (format == "bgr24") return AV_PIX_FMT_BGR24;
    if (format == "rgb24") return AV_PIX_FMT_RGB24;
    if (format == "yuv420p") return AV_PIX_FMT_YUV420P;
    if (format == "nv12") return AV_PIX_FMT_NV12;
    if (format == "yuyv422") return AV_PIX_FMT_YUYV422;
    return AV_PIX_FMT_NONE;
}

inline std::string pixelFormatToString(AVPixelFormat format) {
    switch (format) {
        case AV_PIX_FMT_BGR24: return "bgr24";
        case AV_PIX_FMT_RGB24: return "rgb24";
        case AV_PIX_FMT_YUV420P: return "yuv420p";
        case AV_PIX_FMT_NV12: return "nv12";
        case AV_PIX_FMT_YUYV422: return "yuyv422";
        default: return "unknown";
    }
}

// Funções para tratamento de erros FFmpeg
inline std::string getFFmpegError(int errnum) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, buf, AV_ERROR_MAX_STRING_SIZE);
    return std::string(buf);
}

// Funções para cálculo de tamanho de frame
inline int calculateFrameSize(int width, int height, AVPixelFormat format) {
    return av_image_get_buffer_size(format, width, height, 1);
}

// Funções de conversão de timestamp
inline int64_t convertTimestamp(int64_t ts, AVRational src_tb, AVRational dst_tb) {
    return av_rescale_q(ts, src_tb, dst_tb);
}

// Funções de validação
inline bool isValidResolution(int width, int height) {
    return width > 0 && height > 0 && width % 2 == 0 && height % 2 == 0;
}

inline bool isValidBitrate(int bitrate) {
    return bitrate >= 100000 && bitrate <= 50000000;  // 100kbps a 50Mbps
}

inline bool isValidFPS(int fps) {
    return fps > 0 && fps <= 120;  // 1 a 120 fps
}

// Funções auxiliares para áudio (caso implemente no futuro)
struct AudioInfo {
    int sampleRate;
    int channels;
    AVSampleFormat format;
    int64_t channelLayout;
};

inline int calculateAudioBufferSize(const AudioInfo& info, int samples) {
    return av_samples_get_buffer_size(nullptr, info.channels, samples, 
                                    info.format, 1);
}

// Funções de log
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

using LogCallback = std::function<void(LogLevel, const std::string&)>;

class Logger {
public:
    static void setCallback(LogCallback callback) {
        callback_ = std::move(callback);
    }

    static void log(LogLevel level, const std::string& message) {
        if (callback_) {
            callback_(level, message);
        }
    }

private:
    static inline LogCallback callback_;
};

} // namespace utils
} // namespace turbovision