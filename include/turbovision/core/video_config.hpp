#pragma once

#include "common.hpp"

namespace turbovision {

    struct TURBOVISION_API VideoConfig {
        int width = 1920;
        int height = 1080;
        int fps = 30;
        int bitrate = 4000000;  // 4 Mbps
        DeviceType deviceType = DeviceType::AUTO;

        // Configurações avançadas
        struct Advanced {
            bool zeroCopy = true;      // Usar zero-copy quando possível
            int threadCount = 0;       // 0 = automático
            int bufferSize = 1024*1024; // 1MB buffer
            int maxLatency = 500000;   // 500ms em microsegundos
        } advanced;
    };

} // namespace turbovision