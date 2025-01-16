#pragma once

#include "turbovision/core/common.hpp"
#include "turbovision/core/video_config.hpp"
#include "turbovision/core/frame_data.hpp"
#include "turbovision/core/hardware_manager.hpp"

#include <thread>
#include <mutex>
#include <queue>
#include <functional>

namespace turbovision {

class TURBOVISION_API VideoSource {
public:
    using FrameCallback = std::function<void(FramePtr)>;

    explicit VideoSource(const VideoConfig& config);
    virtual ~VideoSource();

    // Previne cópia
    VideoSource(const VideoSource&) = delete;
    VideoSource& operator=(const VideoSource&) = delete;

    // Interface pública
    virtual bool start();
    virtual void stop();
    virtual bool pause();
    virtual bool resume();
    virtual bool seek(int64_t timestamp);
    void setFrameCallback(FrameCallback callback);

    // Status
    bool isRunning() const { return isRunning_; }
    bool isPaused() const { return isPaused_; }

    // Informações do stream
    struct StreamInfo {
        int width;
        int height;
        AVPixelFormat pixelFormat;
        AVRational timeBase;
        AVRational frameRate;
        int64_t duration;
        int64_t bitRate;
    };

    virtual StreamInfo getStreamInfo() const;

protected:
    // Métodos que devem ser implementados pelas classes derivadas
    virtual bool initializeSource() = 0;
    virtual void captureLoop() = 0;
    virtual void cleanupSource() = 0;

    // Membros protegidos
    VideoConfig config_;
    std::shared_ptr<HardwareManager> hwManager_;
    AVFormatContext* formatContext_;
    AVCodecContext* codecContext_;
    int videoStreamIndex_;

    bool isRunning_;
    bool isPaused_;

    std::thread captureThread_;
    std::mutex frameMutex_;
    std::queue<FramePtr> frameQueue_;
    FrameCallback frameCallback_;

    // Métodos utilitários protegidos
    bool processPacket(AVPacket* packet);
    bool processFrame(AVFrame* frame);
    void clearFrameQueue();

    // Helper para lidar com frames de hardware
    bool transferFrameFromGPU(AVFrame* hwFrame, AVFrame* swFrame);
};

} // namespace turbovision