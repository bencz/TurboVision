#include "turbovision/turbovision.hpp"
#include <mutex>

namespace turbovision {
    namespace {
        std::once_flag initFlag;
        bool isInitialized = false;
        const char *VERSION = "1.0.0";
        utils::LogLevel currentLogLevel = utils::LogLevel::Info;

        void logCallback(void *ptr, int level, const char *fmt, va_list vargs) {
            if (!fmt) return;

            char buffer[1024];
            vsnprintf(buffer, sizeof(buffer), fmt, vargs);

            utils::LogLevel tvLevel;
            switch (level) {
                case AV_LOG_ERROR: tvLevel = utils::LogLevel::Error;
                    break;
                case AV_LOG_WARNING: tvLevel = utils::LogLevel::Warning;
                    break;
                case AV_LOG_INFO: tvLevel = utils::LogLevel::Info;
                    break;
                default: tvLevel = utils::LogLevel::Debug;
                    break;
            }

            if (tvLevel >= currentLogLevel) {
                utils::Logger::log(tvLevel, buffer);
            }
        }

        void doInitialize() {
            // Registrar todos os codecs, muxers, demuxers
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
            av_register_all();
#endif

            // Registrar dispositivos
            avdevice_register_all();

            // Inicializar rede
            avformat_network_init();

            // Configurar logging do FFmpeg
            av_log_set_callback(logCallback);

            isInitialized = true;
        }
    }

    bool initialize() {
        std::call_once(initFlag, doInitialize);
        return isInitialized;
    }

    void shutdown() {
        if (isInitialized) {
            avformat_network_deinit();
            isInitialized = false;
        }
    }

    bool hasHardwareSupport() {
        try {
            HardwareManager hw(DeviceType::AUTO);
            return hw.isHardwareAvailable();
        } catch (...) {
            return false;
        }
    }

    std::vector<HardwareManager::DeviceInfo> getHardwareInfo() {
        return HardwareManager::getAvailableDevices();
    }

    std::string getVersion() {
        return VERSION;
    }

    void setLogLevel(utils::LogLevel level) {
        currentLogLevel = level;

        int avLevel;
        switch (level) {
            case utils::LogLevel::Error:
                avLevel = AV_LOG_ERROR;
                break;
            case utils::LogLevel::Warning:
                avLevel = AV_LOG_WARNING;
                break;
            case utils::LogLevel::Info:
                avLevel = AV_LOG_INFO;
                break;
            case utils::LogLevel::Debug:
                avLevel = AV_LOG_DEBUG;
                break;
        }

        av_log_set_level(avLevel);
    }

    void setLogCallback(utils::LogCallback callback) {
        utils::Logger::setCallback(std::move(callback));
    }

    bool isHardwareAvailable(DeviceType type) {
        try {
            HardwareManager hw(type);
            return hw.isHardwareAvailable();
        } catch (...) {
            return false;
        }
    }

    namespace {
        // Classe para inicialização automática (opcional)
        class AutoInitializer {
        public:
            AutoInitializer() {
                initialize();
            }

            ~AutoInitializer() {
                shutdown();
            }
        };

        // Instância estática para inicialização automática
#ifdef TURBOVISION_AUTO_INIT
    static AutoInitializer autoInit;
#endif
    }
} // namespace turbovision
