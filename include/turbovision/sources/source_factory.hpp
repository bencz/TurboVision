#pragma once

#include "video_source.hpp"
#include "camera_source.hpp"
#include "rtsp_source.hpp"
#include <memory>

namespace turbovision {

    class TURBOVISION_API SourceFactory {
    public:
        // Factory method principal
        static std::shared_ptr<VideoSource> createSource(
            SourceType type,
            const VideoConfig& config,
            const std::string& path);

        // Factory methods específicos
        static std::shared_ptr<CameraSource> createCameraSource(
            const VideoConfig& config,
            const CameraSource::CameraConfig& camConfig);

        static std::shared_ptr<RTSPSource> createRTSPSource(
            const VideoConfig& config,
            const RTSPSource::RTSPConfig& rtspConfig);

        // Métodos de descoberta de dispositivos
        static std::vector<CameraSource::CameraInfo> listAvailableCameras();

        // Métodos utilitários
        static bool isCameraPath(const std::string& path);
        static bool isRTSPUrl(const std::string& path);

    private:
        // Previne instanciação
        SourceFactory() = delete;
        ~SourceFactory() = delete;

        // Helpers internos
        static CameraSource::CameraConfig createDefaultCameraConfig(const std::string& path);
        static RTSPSource::RTSPConfig createDefaultRTSPConfig(const std::string& url);
    };

} // namespace turbovision