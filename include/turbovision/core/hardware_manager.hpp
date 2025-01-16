#pragma once

#include "common.hpp"
#include <vector>
#include <string>

namespace turbovision {

    class TURBOVISION_API HardwareManager {
    public:
        struct DeviceInfo {
            std::string name;
            std::string description;
            DeviceType type;
            bool available;
            std::vector<AVPixelFormat> supportedFormats;
        };

        explicit HardwareManager(DeviceType type = DeviceType::AUTO);
        ~HardwareManager();

        // Previne cópia
        HardwareManager(const HardwareManager&) = delete;
        HardwareManager& operator=(const HardwareManager&) = delete;

        // Permite movimento
        HardwareManager(HardwareManager&& other) noexcept;
        HardwareManager& operator=(HardwareManager&& other) noexcept;

        // Getters
        AVBufferRef* getContext() const { return hwContext_; }
        AVHWDeviceType getType() const { return hwType_; }
        bool isHardwareAvailable() const { return hwContext_ != nullptr; }

        // Informações do dispositivo
        DeviceInfo getDeviceInfo() const;
        static std::vector<DeviceInfo> getAvailableDevices();

    private:
        AVBufferRef* hwContext_;
        AVHWDeviceType hwType_;

        bool initializeDevice(DeviceType type);
        static AVHWDeviceType convertDeviceType(DeviceType type);
        static DeviceType convertFFmpegType(AVHWDeviceType type);
    };

} // namespace turbovision