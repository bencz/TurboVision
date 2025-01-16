#pragma once

#include "video_source.hpp"
#include <string>

namespace turbovision {

    class TURBOVISION_API RTSPSource : public VideoSource {
    public:
        struct RTSPConfig {
            std::string url;               // URL do stream RTSP
            bool useTCP = true;           // Usar TCP ao invés de UDP
            int timeout = 5000000;        // Timeout em microsegundos (5 segundos)
            bool reconnectOnError = true;  // Tentar reconectar em caso de erro
            int maxReconnectAttempts = 5;  // Número máximo de tentativas de reconexão

            struct Advanced {
                int bufferSize = 1024*1024;  // Buffer de rede (1MB)
                int maxDelay = 500000;       // Delay máximo (500ms)
                bool lowLatency = true;      // Modo de baixa latência
                int rtpBufferSize = 8192;    // Tamanho do buffer RTP

                Advanced() = default;
            } advanced;

            RTSPConfig() = default;
        };

        RTSPSource(const VideoConfig& config, const RTSPConfig& rtspConfig);
        ~RTSPSource() override;

        // Status do RTSP
        struct RTSPStatus {
            bool connected;
            int reconnectAttempts;
            float averageLatency;
            float packetLoss;
            int64_t bytesReceived;
            int64_t framesReceived;
        };

        RTSPStatus getStatus() const;

    protected:
        bool initializeSource() override;
        void captureLoop() override;
        void cleanupSource() override;

    private:
        RTSPConfig rtspConfig_;
        RTSPStatus status_;
        int reconnectAttempts_;

        bool initializeDecoder();
        bool connect();
        void disconnect();
        bool reconnect();
        bool setupNetworking();
        void updateStatus();

        // Configurações de rede
        bool setupRTSPOptions(AVDictionary** options);
        bool configureRTSPTransport();
    };

} // namespace turbovision