#pragma once

#include "video_source.hpp"
#include <string>

namespace turbovision {

class TURBOVISION_API CameraSource : public VideoSource {
public:
    struct CameraConfig {
        std::string deviceId;          // ID ou caminho do dispositivo
        bool useDirectShow = false;    // Windows: usar DirectShow
        bool useV4L2 = false;         // Linux: usar V4L2

        struct Advanced {
            std::string format;        // Formato preferido
            int inputFormat = -1;      // Formato de entrada raw
            bool forceFormat = false;  // Forçar formato específico
            int exposure = -1;         // -1 = auto
            int brightness = -1;       // -1 = padrão
            int contrast = -1;         // -1 = padrão
            int saturation = -1;       // -1 = padrão
            bool autoFocus = true;     // Auto foco

            Advanced() = default;
        } advanced;

        CameraConfig() = default;
    };

    CameraSource(const VideoConfig& config, const CameraConfig& camConfig);
    ~CameraSource() override;

    // Controles de câmera
    bool setExposure(int value);
    bool setBrightness(int value);
    bool setContrast(int value);
    bool setSaturation(int value);
    bool setAutoFocus(bool enable);

    // Informações da câmera
    struct CameraInfo {
        std::string name;
        std::string description;
        std::vector<std::pair<int, int>> supportedResolutions;
        std::vector<AVPixelFormat> supportedFormats;
        std::vector<int> supportedFramerates;
    };

    CameraInfo getCameraInfo() const;
    static std::vector<CameraInfo> getAvailableCameras();

protected:
    bool initializeSource() override;
    void captureLoop() override;
    void cleanupSource() override;

private:
    CameraConfig cameraConfig_;
    AVDictionary* cameraOptions_;

    bool initializeDecoder();
    bool setupCamera();
    bool configureFormat();
    bool setProperty(const std::string& property, int value);
    void updateCameraOptions();
};

} // namespace turbovision