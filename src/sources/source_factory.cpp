#include "turbovision/sources/source_factory.hpp"
#include <regex>

namespace turbovision {
    std::shared_ptr<VideoSource> SourceFactory::createSource(
        SourceType type,
        const VideoConfig &config,
        const std::string &path) {
        switch (type) {
            case SourceType::CAMERA:
                return createCameraSource(config, createDefaultCameraConfig(path));

            case SourceType::RTSP:
                return createRTSPSource(config, createDefaultRTSPConfig(path));

            default:
                throw Exception("Tipo de fonte não suportado");
        }
    }

    std::shared_ptr<CameraSource> SourceFactory::createCameraSource(
        const VideoConfig &config,
        const CameraSource::CameraConfig &camConfig) {
        try {
            auto source = std::make_shared<CameraSource>(config, camConfig);
            return source;
        } catch (const Exception &e) {
            throw Exception("Falha ao criar fonte de câmera: " + std::string(e.what()));
        }
    }

    std::shared_ptr<RTSPSource> SourceFactory::createRTSPSource(
        const VideoConfig &config,
        const RTSPSource::RTSPConfig &rtspConfig) {
        try {
            auto source = std::make_shared<RTSPSource>(config, rtspConfig);
            return source;
        } catch (const Exception &e) {
            throw Exception("Falha ao criar fonte RTSP: " + std::string(e.what()));
        }
    }

    std::vector<CameraSource::CameraInfo> SourceFactory::listAvailableCameras() {
        return CameraSource::getAvailableCameras();
    }

    bool SourceFactory::isCameraPath(const std::string &path) {
#ifdef _WIN32
        // No Windows, pode ser um número ou nome de dispositivo
        return std::regex_match(path, std::regex("^[0-9]+$")) ||
               path.find("video=") != std::string::npos;
#else
    // No Linux, procura por /dev/video* ou um número
    return path.find("/dev/video") != std::string::npos ||
           std::regex_match(path, std::regex("^[0-9]+$"));
#endif
    }

    bool SourceFactory::isRTSPUrl(const std::string &path) {
        return path.find("rtsp://") == 0;
    }

    CameraSource::CameraConfig SourceFactory::createDefaultCameraConfig(
        const std::string &path) {
        CameraSource::CameraConfig config;
        config.deviceId = path;

#ifdef _WIN32
        config.useDirectShow = true;
        // Se for apenas um número, adicionar ao path
        if (std::regex_match(path, std::regex("^[0-9]+$"))) {
            config.deviceId = path;
        }
#else
    config.useV4L2 = true;
    // Se for apenas um número, converter para /dev/video*
    if (std::regex_match(path, std::regex("^[0-9]+$"))) {
        config.deviceId = "/dev/video" + path;
    }
#endif

        return config;
    }

    RTSPSource::RTSPConfig SourceFactory::createDefaultRTSPConfig(
        const std::string &url) {
        RTSPSource::RTSPConfig config;
        config.url = url;
        config.useTCP = true; // TCP é geralmente mais confiável
        config.reconnectOnError = true;
        config.timeout = 5000000; // 5 segundos

        // Configurações avançadas para baixa latência
        config.advanced.lowLatency = true;
        config.advanced.maxDelay = 500000; // 500ms
        config.advanced.bufferSize = 1024 * 1024; // 1MB buffer

        return config;
    }
} // namespace turbovision
