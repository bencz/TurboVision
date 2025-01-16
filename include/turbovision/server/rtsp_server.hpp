#pragma once

#include "turbovision/core/common.hpp"
#include "turbovision/core/video_config.hpp"
#include "turbovision/core/hardware_manager.hpp"
#include "turbovision/core/frame_data.hpp"
#include "server_config.hpp"

#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <functional>

namespace turbovision {

class TURBOVISION_API RTSPServer {
public:
    // Estatísticas do servidor
    struct ServerStats {
        int connectedClients;         // Número de clientes conectados
        float currentFps;             // FPS atual
        int64_t currentBitrate;       // Bitrate atual em bits/s
        int64_t bytesTransferred;     // Total de bytes transferidos
        int64_t framesTransferred;    // Total de frames transferidos
        int64_t uptime;              // Tempo de execução em segundos
        float avgLatency;             // Latência média em ms
        int droppedFrames;           // Frames descartados
    };

    RTSPServer(const ServerConfig& config, const VideoConfig& videoConfig);
    ~RTSPServer();

    // Interface principal
    bool start();
    void stop();
    bool isRunning() const { return isRunning_; }

    // Envio de frames
    bool pushFrame(const uint8_t* frameData, int size);
    bool pushFrame(const FramePtr& frame);

    // Estatísticas
    ServerStats getStats() const;

    // Callbacks para eventos
    using ClientConnectedCallback = std::function<void(const std::string& clientAddress)>;
    using ClientDisconnectedCallback = std::function<void(const std::string& clientAddress)>;

    void setClientConnectedCallback(ClientConnectedCallback callback);
    void setClientDisconnectedCallback(ClientDisconnectedCallback callback);

private:
    // Configurações
    ServerConfig config_;
    VideoConfig videoConfig_;
    std::shared_ptr<HardwareManager> hwManager_;

    // Contextos FFmpeg
    AVFormatContext* formatContext_;
    AVCodecContext* encoderContext_;
    AVStream* videoStream_;

    // Estado do servidor
    std::atomic<bool> isRunning_;
    std::thread serverThread_;
    std::mutex frameMutex_;
    std::queue<AVFrame*> frameQueue_;
    ServerStats stats_;
    mutable std::mutex statsMutex_;

    // Callbacks
    ClientConnectedCallback clientConnectedCallback_;
    ClientDisconnectedCallback clientDisconnectedCallback_;

    // Métodos de inicialização
    bool initializeServer();
    bool setupEncoder();
    bool configureOutput();
    bool setupNetworking(AVDictionary** options);

    // Loop principal e processamento
    void serverLoop();
    void processFrame(AVFrame* frame);
    bool encodeAndTransmit(AVFrame* frame);
    void clearFrameQueue();

    // Gerenciamento de estatísticas
    void updateStats();
    void resetStats();

    // Utilitários
    static AVFrame* createVideoFrame(int width, int height, AVPixelFormat pixFormat);
    bool convertFrame(const uint8_t* data, int size, AVFrame* frame);
};

} // namespace turbovision