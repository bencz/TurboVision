#pragma once

// Configuração de exportação DLL para Windows
#ifdef _WIN32
    #ifdef TURBOVISION_EXPORTS
        #define TURBOVISION_API __declspec(dllexport)
    #else
        #define TURBOVISION_API __declspec(dllimport)
    #endif
#else
    #define TURBOVISION_API
#endif
#include <stdexcept>

// Headers FFmpeg
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

namespace turbovision {

    // Enumerações comuns
    enum class DeviceType {
        NVIDIA_CUDA,
        INTEL_QSV,
        AMD_AMF,
        AUTO
    };

    enum class SourceType {
        CAMERA,
        RTSP,
        FILE,
        CUSTOM
    };

    // Exceção base
    class TURBOVISION_API Exception : public std::runtime_error {
    public:
        explicit Exception(const std::string& message)
            : std::runtime_error(message) {}
    };

} // namespace turbovision