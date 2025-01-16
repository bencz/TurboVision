#include "turbovision/core/hardware_manager.hpp"

namespace turbovision {
    namespace {
        struct HWDeviceMapping {
            DeviceType type;
            AVHWDeviceType ffmpegType;
            const char *name;
            const char *description;
        };

        static const HWDeviceMapping deviceMappings[] = {
            {
                DeviceType::NVIDIA_CUDA, AV_HWDEVICE_TYPE_CUDA,
                "NVIDIA CUDA", "NVIDIA GPU using CUDA"
            },
            {
                DeviceType::INTEL_QSV, AV_HWDEVICE_TYPE_QSV,
                "Intel QuickSync", "Intel GPU using QuickSync"
            },
            {
                DeviceType::AMD_AMF, AV_HWDEVICE_TYPE_DRM,
                "AMD AMF", "AMD GPU using AMF"
            }
        };
    }

    HardwareManager::HardwareManager(DeviceType type)
        : hwContext_(nullptr)
          , hwType_(AV_HWDEVICE_TYPE_NONE) {
        if (!initializeDevice(type)) {
            throw Exception("Failed to initialize hardware device");
        }
    }

    HardwareManager::~HardwareManager() {
        if (hwContext_) {
            av_buffer_unref(&hwContext_);
        }
    }

    HardwareManager::HardwareManager(HardwareManager &&other) noexcept
        : hwContext_(other.hwContext_)
          , hwType_(other.hwType_) {
        other.hwContext_ = nullptr;
        other.hwType_ = AV_HWDEVICE_TYPE_NONE;
    }

    HardwareManager &HardwareManager::operator=(HardwareManager &&other) noexcept {
        if (this != &other) {
            if (hwContext_) {
                av_buffer_unref(&hwContext_);
            }
            hwContext_ = other.hwContext_;
            hwType_ = other.hwType_;
            other.hwContext_ = nullptr;
            other.hwType_ = AV_HWDEVICE_TYPE_NONE;
        }
        return *this;
    }

    bool HardwareManager::initializeDevice(DeviceType type) {
        std::vector<AVHWDeviceType> devices;

        if (type == DeviceType::AUTO) {
            // Tentar todos os dispositivos na ordem de preferência
            for (const auto &mapping: deviceMappings) {
                devices.push_back(mapping.ffmpegType);
            }
        } else {
            devices.push_back(convertDeviceType(type));
        }

        for (auto deviceType: devices) {
            if (av_hwdevice_ctx_create(&hwContext_, deviceType, nullptr, nullptr, 0) >= 0) {
                hwType_ = deviceType;
                return true;
            }
        }

        return false;
    }

    AVHWDeviceType HardwareManager::convertDeviceType(DeviceType type) {
        for (const auto &mapping: deviceMappings) {
            if (mapping.type == type) {
                return mapping.ffmpegType;
            }
        }
        return AV_HWDEVICE_TYPE_NONE;
    }

    DeviceType HardwareManager::convertFFmpegType(AVHWDeviceType type) {
        for (const auto &mapping: deviceMappings) {
            if (mapping.ffmpegType == type) {
                return mapping.type;
            }
        }
        return DeviceType::AUTO;
    }

    HardwareManager::DeviceInfo HardwareManager::getDeviceInfo() const {
        DeviceInfo info;
        for (const auto &mapping: deviceMappings) {
            if (mapping.ffmpegType == hwType_) {
                info.name = mapping.name;
                info.description = mapping.description;
                info.type = mapping.type;
                info.available = true;

                // Obter formatos suportados
                AVHWFramesConstraints *constraints = av_hwdevice_get_hwframe_constraints(hwContext_, nullptr);
                if (constraints) {
                    for (const AVPixelFormat *format = constraints->valid_sw_formats;
                         format && *format != AV_PIX_FMT_NONE; format++) {
                        info.supportedFormats.push_back(*format);
                    }
                    av_hwframe_constraints_free(&constraints);
                }
                break;
            }
        }
        return info;
    }

    std::vector<HardwareManager::DeviceInfo> HardwareManager::getAvailableDevices() {
        std::vector<DeviceInfo> devices;

        for (const auto &mapping: deviceMappings) {
            AVBufferRef *ctx = nullptr;
            if (av_hwdevice_ctx_create(&ctx, mapping.ffmpegType, nullptr, nullptr, 0) >= 0) {
                DeviceInfo info;
                info.name = mapping.name;
                info.description = mapping.description;
                info.type = mapping.type;
                info.available = true;

                // Obter formatos suportados
                AVHWFramesConstraints *constraints =
                        av_hwdevice_get_hwframe_constraints(ctx, nullptr);
                if (constraints) {
                    for (const AVPixelFormat *format = constraints->valid_sw_formats;
                         format && *format != AV_PIX_FMT_NONE; format++) {
                        info.supportedFormats.push_back(*format);
                    }
                    av_hwframe_constraints_free(&constraints);
                }

                devices.push_back(info);
                av_buffer_unref(&ctx);
            }
        }

        return devices;
    }
} // namespace turbovision
