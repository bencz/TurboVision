#include "turbovision/sources/camera_source.hpp"
#include <iostream>

namespace turbovision {
    CameraSource::CameraSource(const VideoConfig &config, const CameraConfig &camConfig)
        : VideoSource(config)
          , cameraConfig_(camConfig)
          , cameraOptions_(nullptr) {
    }

    CameraSource::~CameraSource() {
        stop();
        if (cameraOptions_) {
            av_dict_free(&cameraOptions_);
        }
    }

    bool CameraSource::initializeSource() {
        avdevice_register_all();
        formatContext_ = avformat_alloc_context();

        if (!setupCamera()) {
            return false;
        }

        // Encontrar stream de vídeo
        for (unsigned int i = 0; i < formatContext_->nb_streams; i++) {
            if (formatContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIndex_ = i;
                break;
            }
        }

        if (videoStreamIndex_ < 0) {
            return false;
        }

        if (!initializeDecoder()) {
            return false;
        }

        return configureFormat();
    }

    void CameraSource::captureLoop() {
        AVPacket *packet = av_packet_alloc();

        while (isRunning_) {
            if (isPaused_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            int ret = av_read_frame(formatContext_, packet);
            if (ret >= 0) {
                if (packet->stream_index == videoStreamIndex_) {
                    processPacket(packet);
                }
                av_packet_unref(packet);
            } else {
                // Erro na leitura - pode acontecer se a câmera for desconectada
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        av_packet_free(&packet);
    }

    void CameraSource::cleanupSource() {
        if (cameraOptions_) {
            av_dict_free(&cameraOptions_);
            cameraOptions_ = nullptr;
        }
    }

    bool CameraSource::initializeDecoder() {
        return true;
    }

    bool CameraSource::setupCamera() {
        const AVInputFormat *inputFormat = nullptr;
        std::string devicePath = cameraConfig_.deviceId;

#ifdef _WIN32
        if (cameraConfig_.useDirectShow) {
            inputFormat = av_find_input_format("dshow");
            devicePath = "video=" + devicePath;
        } else {
            inputFormat = av_find_input_format("vfwcap");
        }
#else
    if (cameraConfig_.useV4L2) {
        inputFormat = av_find_input_format("v4l2");
        if (!devicePath.empty() && devicePath[0] != '/') {
            devicePath = "/dev/video" + devicePath;
        }
    }
#endif

        if (!inputFormat) {
            return false;
        }

        updateCameraOptions();

        AVFormatContext *tempContext = nullptr; // Ponteiro temporário
        if (avformat_open_input(&tempContext, devicePath.c_str(),
                                inputFormat, &cameraOptions_) < 0) {
            return false;
        }

        formatContext_ = tempContext; // Atualiza formatContext_ após sucesso

        return avformat_find_stream_info(formatContext_, nullptr) >= 0;
    }

    bool CameraSource::configureFormat() {
        if (!formatContext_ || videoStreamIndex_ < 0 || !codecContext_) {
            return false;
        }

        AVStream *stream = formatContext_->streams[videoStreamIndex_];

        // Configurar formato de pixel
        if (cameraConfig_.advanced.forceFormat && !cameraConfig_.advanced.format.empty()) {
            av_opt_set_int(formatContext_->priv_data, "video_format",
                           cameraConfig_.advanced.inputFormat, 0);
        }

        // Configurar resolução
        stream->codecpar->width = config_.width;
        stream->codecpar->height = config_.height;

        // Aplicar configurações de imagem
        if (cameraConfig_.advanced.exposure >= 0) {
            setProperty("exposure", cameraConfig_.advanced.exposure);
        }
        if (cameraConfig_.advanced.brightness >= 0) {
            setProperty("brightness", cameraConfig_.advanced.brightness);
        }
        if (cameraConfig_.advanced.contrast >= 0) {
            setProperty("contrast", cameraConfig_.advanced.contrast);
        }
        if (cameraConfig_.advanced.saturation >= 0) {
            setProperty("saturation", cameraConfig_.advanced.saturation);
        }

        return true;
    }

    bool CameraSource::setProperty(const std::string &property, int value) {
        if (!formatContext_) {
            return false;
        }

        return av_opt_set_int(formatContext_->priv_data, property.c_str(), value, 0) >= 0;
    }

    void CameraSource::updateCameraOptions() {
        if (cameraOptions_) {
            av_dict_free(&cameraOptions_);
        }

        // Configurar resolução
        av_dict_set(&cameraOptions_, "video_size",
                    (std::to_string(config_.width) + "x" +
                     std::to_string(config_.height)).c_str(), 0);

        // Configurar FPS
        av_dict_set(&cameraOptions_, "framerate",
                    std::to_string(config_.fps).c_str(), 0);

        // Configurar formato de pixel se especificado
        if (!cameraConfig_.advanced.format.empty()) {
            av_dict_set(&cameraOptions_, "pixel_format",
                        cameraConfig_.advanced.format.c_str(), 0);
        }
    }

    bool CameraSource::setExposure(int value) {
        cameraConfig_.advanced.exposure = value;
        return setProperty("exposure", value);
    }

    bool CameraSource::setBrightness(int value) {
        cameraConfig_.advanced.brightness = value;
        return setProperty("brightness", value);
    }

    bool CameraSource::setContrast(int value) {
        cameraConfig_.advanced.contrast = value;
        return setProperty("contrast", value);
    }

    bool CameraSource::setSaturation(int value) {
        cameraConfig_.advanced.saturation = value;
        return setProperty("saturation", value);
    }

    bool CameraSource::setAutoFocus(bool enable) {
        cameraConfig_.advanced.autoFocus = enable;
        return setProperty("autofocus", enable ? 1 : 0);
    }

    CameraSource::CameraInfo CameraSource::getCameraInfo() const {
        CameraInfo info;

        if (!formatContext_) {
            return info;
        }

        AVDeviceInfoList *deviceList = nullptr;

        // Obter informações sobre dispositivos disponíveis
        if (avdevice_list_input_sources(formatContext_->iformat, nullptr,
                                        nullptr, &deviceList) >= 0) {
            if (deviceList && deviceList->nb_devices > 0) {
                for (int i = 0; i < deviceList->nb_devices; i++) {
                    if (deviceList->devices[i]->device_name == cameraConfig_.deviceId) {
                        info.name = deviceList->devices[i]->device_name;
                        info.description = deviceList->devices[i]->device_description;
                        break;
                    }
                }
            }
            avdevice_free_list_devices(&deviceList);
        }

        // Obter resoluções e formatos suportados
        if (codecContext_) {
            const std::pair<int, int> resolutions[] = {
                {640, 480}, {800, 600}, {1280, 720},
                {1920, 1080}, {2560, 1440}, {3840, 2160}
            };

            for (const auto &res: resolutions) {
                AVDictionary *opts = nullptr;
                av_dict_set(&opts, "video_size",
                            (std::to_string(res.first) + "x" +
                             std::to_string(res.second)).c_str(), 0);

                // Use um ponteiro temporário para testar resoluções
                AVFormatContext *tempContext = nullptr;
                if (avformat_open_input(&tempContext,
                                        cameraConfig_.deviceId.c_str(),
                                        nullptr, &opts) >= 0) {
                    info.supportedResolutions.push_back(res);
                    avformat_close_input(&tempContext);
                }
                av_dict_free(&opts);
            }
        }

        return info;
    }

    std::vector<CameraSource::CameraInfo> CameraSource::getAvailableCameras() {
        std::vector<CameraInfo> cameras;
        AVDeviceInfoList *deviceList = nullptr;

#ifdef _WIN32
        const AVInputFormat *format = av_find_input_format("dshow");
#else
    const AVInputFormat* format = av_find_input_format("v4l2");
#endif

        if (avdevice_list_input_sources(const_cast<AVInputFormat *>(format),
                                        nullptr, nullptr, &deviceList) >= 0) {
            if (deviceList) {
                for (int i = 0; i < deviceList->nb_devices; i++) {
                    CameraInfo info;
                    info.name = deviceList->devices[i]->device_name;
                    info.description = deviceList->devices[i]->device_description;
                    cameras.push_back(info);
                }
            }
            avdevice_free_list_devices(&deviceList);
        }

        return cameras;
    }
} // namespace turbovision
