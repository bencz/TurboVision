#include "turbovision/server/rtsp_server.hpp"
#include <chrono>
#include <sstream>

namespace turbovision {
    RTSPServer::RTSPServer(const ServerConfig &config, const VideoConfig &videoConfig)
        : config_(config)
          , videoConfig_(videoConfig)
          , formatContext_(nullptr)
          , encoderContext_(nullptr)
          , videoStream_(nullptr)
          , isRunning_(false) {
        hwManager_ = std::make_shared<HardwareManager>(videoConfig.deviceType);
        resetStats();
    }

    RTSPServer::~RTSPServer() {
        stop();

        if (encoderContext_) {
            avcodec_free_context(&encoderContext_);
        }

        if (formatContext_) {
            if (!(formatContext_->oformat->flags & AVFMT_NOFILE)) {
                avio_closep(&formatContext_->pb);
            }
            avformat_free_context(formatContext_);
        }
    }

    bool RTSPServer::start() {
        if (isRunning_) {
            return false;
        }

        if (!initializeServer()) {
            return false;
        }

        isRunning_ = true;
        resetStats();
        serverThread_ = std::thread(&RTSPServer::serverLoop, this);

        return true;
    }

    void RTSPServer::stop() {
        isRunning_ = false;

        if (serverThread_.joinable()) {
            serverThread_.join();
        }

        clearFrameQueue();
    }

    bool RTSPServer::pushFrame(const uint8_t *frameData, int size) {
        if (!isRunning_ || !frameData) {
            return false;
        }

        AVFrame *frame = createVideoFrame(videoConfig_.width,
                                          videoConfig_.height,
                                          encoderContext_->pix_fmt);
        if (!frame) {
            return false;
        }

        if (!convertFrame(frameData, size, frame)) {
            av_frame_free(&frame);
            return false;
        } {
            std::lock_guard<std::mutex> lock(frameMutex_);
            frameQueue_.push(frame);

            // Limitar tamanho da fila
            while (frameQueue_.size() > 30) {
                AVFrame *oldFrame = frameQueue_.front();
                frameQueue_.pop();
                av_frame_free(&oldFrame);

                std::lock_guard<std::mutex> statsLock(statsMutex_);
                stats_.droppedFrames++;
            }
        }

        return true;
    }

    bool RTSPServer::pushFrame(const FramePtr &frame) {
        if (!frame) {
            return false;
        }
        return pushFrame(frame->data(), frame->dataSize());
    }

    bool RTSPServer::initializeServer() {
        // Criar contexto de saída
        std::string url = "rtsp://" + config_.address + ":" +
                          std::to_string(config_.port) + "/" +
                          config_.streamName;

        avformat_alloc_output_context2(&formatContext_, nullptr, "rtsp", url.c_str());
        if (!formatContext_) {
            return false;
        }

        if (!setupEncoder()) {
            return false;
        }

        if (!configureOutput()) {
            return false;
        }

        return true;
    }

    bool RTSPServer::setupEncoder() {
        // Encontrar encoder apropriado
        const AVCodec *codec = nullptr;
        if (hwManager_->isHardwareAvailable()) {
            // Tentar usar encoder de hardware
            codec = avcodec_find_encoder_by_name(config_.encoder.encoder.c_str());
        }

        if (!codec) {
            // Fallback para encoder por software
            codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        }

        if (!codec) {
            return false;
        }

        // Criar stream
        videoStream_ = avformat_new_stream(formatContext_, codec);
        if (!videoStream_) {
            return false;
        }

        // Configurar encoder
        encoderContext_ = avcodec_alloc_context3(codec);
        if (!encoderContext_) {
            return false;
        }

        // Configurações básicas
        encoderContext_->width = videoConfig_.width;
        encoderContext_->height = videoConfig_.height;
        encoderContext_->time_base = AVRational{1, videoConfig_.fps};
        encoderContext_->framerate = AVRational{videoConfig_.fps, 1};
        encoderContext_->bit_rate = videoConfig_.bitrate;
        encoderContext_->gop_size = config_.encoder.gopSize;
        encoderContext_->max_b_frames = config_.encoder.advanced.maxBFrames;
        encoderContext_->pix_fmt = AV_PIX_FMT_YUV420P;

        // Configurar hardware
        if (hwManager_->isHardwareAvailable()) {
            encoderContext_->hw_device_ctx = av_buffer_ref(hwManager_->getContext());
        }

        // Configurar opções específicas do encoder
        AVDictionary *opts = nullptr;
        if (config_.encoder.lowLatency) {
            av_dict_set(&opts, "preset", "ultrafast", 0);
            av_dict_set(&opts, "tune", "zerolatency", 0);
        }

        int ret = avcodec_open2(encoderContext_, codec, &opts);
        av_dict_free(&opts);

        if (ret < 0) {
            return false;
        }

        return avcodec_parameters_from_context(videoStream_->codecpar,
                                               encoderContext_) >= 0;
    }

    bool RTSPServer::configureOutput() {
        AVDictionary *options = nullptr;
        if (!setupNetworking(&options)) {
            return false;
        }

        // Abrir saída
        if (!(formatContext_->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&formatContext_->pb,
                          formatContext_->url,
                          AVIO_FLAG_WRITE) < 0) {
                av_dict_free(&options);
                return false;
            }
        }

        // Escrever header
        int ret = avformat_write_header(formatContext_, &options);
        av_dict_free(&options);

        return ret >= 0;
    }

    bool RTSPServer::setupNetworking(AVDictionary **options) {
        // Configurar protocolo de transporte
        av_dict_set(options, "rtsp_transport",
                    config_.useTCP ? "tcp" : "udp", 0);

        // Configurar buffer
        av_dict_set_int(options, "buffer_size",
                        config_.network.bufferSize, 0);

        // Configurar timeout
        av_dict_set_int(options, "timeout",
                        config_.network.timeout * 1000000, 0);

        // Configurar multicast se necessário
        if (config_.network.enableMulticast) {
            av_dict_set(options, "ttl",
                        std::to_string(config_.network.multicastTTL).c_str(), 0);
            av_dict_set(options, "mcast_rate",
                        std::to_string(config_.network.maxBitrate).c_str(), 0);
        }

        return true;
    }

    void RTSPServer::serverLoop() {
        AVPacket *packet = av_packet_alloc();
        int64_t pts = 0;
        auto lastStatsUpdate = std::chrono::steady_clock::now();
        auto startTime = std::chrono::steady_clock::now();

        while (isRunning_) {
            AVFrame *frame = nullptr; {
                std::lock_guard<std::mutex> lock(frameMutex_);
                if (!frameQueue_.empty()) {
                    frame = frameQueue_.front();
                    frameQueue_.pop();
                }
            }

            if (frame) {
                frame->pts = pts++;

                if (encodeAndTransmit(frame)) {
                    std::lock_guard<std::mutex> statsLock(statsMutex_);
                    stats_.framesTransferred++;
                }

                av_frame_free(&frame);

                // Atualizar estatísticas a cada segundo
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(
                        now - lastStatsUpdate).count() >= 1) {
                    updateStats();
                    lastStatsUpdate = now;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        // Escrever trailer
        av_write_trailer(formatContext_);
        av_packet_free(&packet);
    }

    bool RTSPServer::encodeAndTransmit(AVFrame *frame) {
        if (!encoderContext_ || !frame) {
            return false;
        }

        if (avcodec_send_frame(encoderContext_, frame) < 0) {
            return false;
        }

        AVPacket *packet = av_packet_alloc();
        bool success = false;

        while (avcodec_receive_packet(encoderContext_, packet) >= 0) {
            packet->stream_index = videoStream_->index;

            // Converter timestamps
            av_packet_rescale_ts(packet,
                                 encoderContext_->time_base,
                                 videoStream_->time_base);

            if (av_interleaved_write_frame(formatContext_, packet) >= 0) {
                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.bytesTransferred += packet->size;
                success = true;
            }

            av_packet_unref(packet);
        }

        av_packet_free(&packet);
        return success;
    }

    void RTSPServer::updateStats() {
        std::lock_guard<std::mutex> lock(statsMutex_);

        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            now - std::chrono::steady_clock::time_point()).count();

        stats_.uptime = uptime;

        // Calcular FPS atual
        if (uptime > 0) {
            stats_.currentFps = static_cast<float>(stats_.framesTransferred) / uptime;
        }

        // Calcular bitrate atual
        stats_.currentBitrate = (stats_.bytesTransferred * 8) / uptime;

        // Outros campos são atualizados durante a operação
    }

    void RTSPServer::resetStats() {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_ = ServerStats{};
    }

    void RTSPServer::clearFrameQueue() {
        std::lock_guard<std::mutex> lock(frameMutex_);
        while (!frameQueue_.empty()) {
            AVFrame *frame = frameQueue_.front();
            frameQueue_.pop();
            av_frame_free(&frame);
        }
    }

    AVFrame *RTSPServer::createVideoFrame(int width, int height, AVPixelFormat pixFormat) {
        AVFrame *frame = av_frame_alloc();
        if (!frame) {
            return nullptr;
        }

        frame->format = pixFormat;
        frame->width = width;
        frame->height = height;

        // Alocar buffers para o frame
        if (av_frame_get_buffer(frame, 32) < 0) {
            av_frame_free(&frame);
            return nullptr;
        }

        // Garantir que o frame seja gravável
        if (av_frame_make_writable(frame) < 0) {
            av_frame_free(&frame);
            return nullptr;
        }

        return frame;
    }

    bool RTSPServer::convertFrame(const uint8_t *data, int size, AVFrame *frame) {
        if (!data || !frame || size <= 0) {
            return false;
        }

        // Assumindo entrada BGR24 e saída YUV420P
        const int in_linesize = frame->width * 3; // BGR24 = 3 bytes por pixel

        // Conversão de cor usando tabelas de lookup (mais rápido que cálculos diretos)
        static const int YR = 77; // 0.299 * 256
        static const int YG = 150; // 0.587 * 256
        static const int YB = 29; // 0.114 * 256
        static const int UR = -43; // -0.169 * 256
        static const int UG = -84; // -0.331 * 256
        static const int UB = 127; // 0.500 * 256
        static const int VR = 127; // 0.500 * 256
        static const int VG = -106; // -0.419 * 256
        static const int VB = -21; // -0.081 * 256

        // Y plane
        for (int y = 0; y < frame->height; y++) {
            for (int x = 0; x < frame->width; x++) {
                const uint8_t *pixel = data + y * in_linesize + x * 3;
                uint8_t b = pixel[0];
                uint8_t g = pixel[1];
                uint8_t r = pixel[2];

                frame->data[0][y * frame->linesize[0] + x] = static_cast<uint8_t>(
                    (YR * r + YG * g + YB * b + 128) >> 8);
            }
        }

        // U and V planes (chroma subsampling 4:2:0)
        for (int y = 0; y < frame->height / 2; y++) {
            for (int x = 0; x < frame->width / 2; x++) {
                int sum_r = 0, sum_g = 0, sum_b = 0;

                // Soma dos 4 pixels adjacentes
                for (int dy = 0; dy < 2; dy++) {
                    for (int dx = 0; dx < 2; dx++) {
                        const uint8_t *pixel = data +
                                               (y * 2 + dy) * in_linesize +
                                               (x * 2 + dx) * 3;
                        sum_b += pixel[0];
                        sum_g += pixel[1];
                        sum_r += pixel[2];
                    }
                }

                // Média dos 4 pixels
                int r = sum_r / 4;
                int g = sum_g / 4;
                int b = sum_b / 4;

                // U plane (Cb)
                frame->data[1][y * frame->linesize[1] + x] = static_cast<uint8_t>(
                    ((UR * r + UG * g + UB * b + 128) >> 8) + 128);

                // V plane (Cr)
                frame->data[2][y * frame->linesize[2] + x] = static_cast<uint8_t>(
                    ((VR * r + VG * g + VB * b + 128) >> 8) + 128);
            }
        }

        return true;
    }

    RTSPServer::ServerStats RTSPServer::getStats() const {
        std::lock_guard<std::mutex> lock(statsMutex_);
        return stats_;
    }

    void RTSPServer::setClientConnectedCallback(ClientConnectedCallback callback) {
        clientConnectedCallback_ = std::move(callback);
    }

    void RTSPServer::setClientDisconnectedCallback(ClientDisconnectedCallback callback) {
        clientDisconnectedCallback_ = std::move(callback);
    }
} // namespace turbovision
