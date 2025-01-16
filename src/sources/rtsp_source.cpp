#include "turbovision/sources/rtsp_source.hpp"
#include <chrono>
#include <iostream>

namespace turbovision {
    RTSPSource::RTSPSource(const VideoConfig &config, const RTSPConfig &rtspConfig)
        : VideoSource(config)
          , rtspConfig_(rtspConfig)
          , reconnectAttempts_(0) {
        status_.connected = false;
        status_.reconnectAttempts = 0;
        status_.averageLatency = 0;
        status_.packetLoss = 0;
        status_.bytesReceived = 0;
    }

    RTSPSource::~RTSPSource() {
        stop();
        disconnect();
    }

    bool RTSPSource::initializeSource() {
        return connect();
    }

    void RTSPSource::captureLoop() {
        std::cout << "RTSPSource::captureLoop() - Iniciando loop de captura..." << std::endl;

        AVPacket *packet = av_packet_alloc();
        if (!packet) {
            std::cerr << "RTSPSource::captureLoop() - Falha ao alocar packet" << std::endl;
            return;
        }

        auto lastStatusUpdate = std::chrono::steady_clock::now();

        try {
            while (isRunning_) {
                if (isPaused_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                int ret = av_read_frame(formatContext_, packet);
                if (ret >= 0) {
                    if (packet->stream_index == videoStreamIndex_) {
                        reconnectAttempts_ = 0; // Reset contador em caso de sucesso
                        status_.bytesReceived += packet->size;
                        status_.framesReceived++;

                        if (!processPacket(packet)) {
                            std::cerr << "RTSPSource::captureLoop() - Falha ao processar packet" << std::endl;
                        }
                    }
                    av_packet_unref(packet);

                    // Atualizar estatísticas a cada segundo
                    auto now = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::seconds>(
                            now - lastStatusUpdate).count() >= 1) {
                        updateStatus();
                        lastStatusUpdate = now;
                    }
                } else {
                    char errbuf[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    std::cerr << "RTSPSource::captureLoop() - Erro na leitura: " << errbuf << std::endl;

                    // Erro na leitura
                    if (rtspConfig_.reconnectOnError &&
                        reconnectAttempts_ < rtspConfig_.maxReconnectAttempts) {
                        std::cout << "RTSPSource::captureLoop() - Tentando reconectar... (tentativa "
                                << reconnectAttempts_ + 1 << ")" << std::endl;

                        disconnect();
                        std::this_thread::sleep_for(std::chrono::seconds(1));

                        if (reconnect()) {
                            reconnectAttempts_++;
                            continue;
                        }
                    }
                    break;
                }
            }
        } catch (const std::exception &e) {
            std::cerr << "RTSPSource::captureLoop() - Exceção: " << e.what() << std::endl;
        }

        std::cout << "RTSPSource::captureLoop() - Finalizando loop de captura" << std::endl;
        av_packet_free(&packet);
    }

    void RTSPSource::cleanupSource() {
        disconnect();
    }

    bool RTSPSource::initializeDecoder() {
        std::cout << "RTSPSource::initializeDecoder() - Iniciando..." << std::endl;

        if (!formatContext_ || videoStreamIndex_ < 0) {
            std::cerr << "RTSPSource::initializeDecoder() - Contexto inválido" << std::endl;
            return false;
        }

        AVStream *stream = formatContext_->streams[videoStreamIndex_];
        const AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);

        if (!decoder) {
            std::cerr << "RTSPSource::initializeDecoder() - Decoder não encontrado" << std::endl;
            return false;
        }

        codecContext_ = avcodec_alloc_context3(decoder);
        if (!codecContext_) {
            std::cerr << "RTSPSource::initializeDecoder() - Falha ao alocar contexto do codec" << std::endl;
            return false;
        }

        if (avcodec_parameters_to_context(codecContext_, stream->codecpar) < 0) {
            std::cerr << "RTSPSource::initializeDecoder() - Falha ao copiar parâmetros" << std::endl;
            avcodec_free_context(&codecContext_);
            return false;
        }

        // Configurar hardware se disponível
        if (hwManager_->isHardwareAvailable()) {
            std::cout << "RTSPSource::initializeDecoder() - Configurando hardware" << std::endl;
            codecContext_->hw_device_ctx = av_buffer_ref(hwManager_->getContext());
        }

        if (avcodec_open2(codecContext_, decoder, nullptr) < 0) {
            std::cerr << "RTSPSource::initializeDecoder() - Falha ao abrir codec" << std::endl;
            avcodec_free_context(&codecContext_);
            return false;
        }

        std::cout << "RTSPSource::initializeDecoder() - Decoder inicializado com sucesso" << std::endl;
        return true;
    }

    bool RTSPSource::connect() {
        std::cout << "RTSPSource::connect() - Iniciando..." << std::endl;

        formatContext_ = avformat_alloc_context();
        if (!formatContext_) {
            std::cerr << "RTSPSource::connect() - Falha ao alocar formato de contexto" << std::endl;
            return false;
        }

        std::cout << "RTSPSource::connect() - Configurando rede..." << std::endl;
        if (!setupNetworking()) {
            std::cerr << "RTSPSource::connect() - Falha na configuração de rede" << std::endl;
            avformat_free_context(formatContext_);
            formatContext_ = nullptr;
            return false;
        }

        // Encontrar stream de vídeo
        std::cout << "RTSPSource::connect() - Procurando stream de vídeo..." << std::endl;
        for (unsigned int i = 0; i < formatContext_->nb_streams; i++) {
            if (formatContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIndex_ = i;
                std::cout << "RTSPSource::connect() - Stream de vídeo encontrado: índice " << i << std::endl;
                break;
            }
        }

        if (videoStreamIndex_ < 0) {
            std::cerr << "RTSPSource::connect() - Nenhum stream de vídeo encontrado" << std::endl;
            disconnect();
            return false;
        }

        std::cout << "RTSPSource::connect() - Inicializando decodificador..." << std::endl;
        if (!initializeDecoder()) {
            std::cerr << "RTSPSource::connect() - Falha ao inicializar decodificador" << std::endl;
            disconnect();
            return false;
        }

        status_.connected = true;
        std::cout << "RTSPSource::connect() - Conexão estabelecida com sucesso" << std::endl;
        return true;
    }

    void RTSPSource::disconnect() {
        if (codecContext_) {
            avcodec_free_context(&codecContext_);
            codecContext_ = nullptr;
        }

        if (formatContext_) {
            avformat_close_input(&formatContext_);
            formatContext_ = nullptr;
        }

        videoStreamIndex_ = -1;
        status_.connected = false;
    }

    bool RTSPSource::reconnect() {
        disconnect();
        return connect();
    }

    bool RTSPSource::setupNetworking() {
        AVDictionary *options = nullptr;

        try {
            std::cout << "RTSPSource::setupNetworking() - URL: " << rtspConfig_.url << std::endl;

            // Configurar opções RTSP
            const char *rtsp_transport = rtspConfig_.useTCP ? "tcp" : "udp";
            av_dict_set(&options, "rtsp_transport", rtsp_transport, 0);
            std::cout << "RTSPSource::setupNetworking() - Transporte: " << rtsp_transport << std::endl;

            // Configurar timeout
            std::string timeout = std::to_string(rtspConfig_.timeout);
            av_dict_set(&options, "stimeout", timeout.c_str(), 0);
            std::cout << "RTSPSource::setupNetworking() - Timeout: " << timeout << std::endl;

            // Configurar buffer
            std::string bufferSize = std::to_string(rtspConfig_.advanced.bufferSize);
            av_dict_set(&options, "buffer_size", bufferSize.c_str(), 0);
            std::cout << "RTSPSource::setupNetworking() - Buffer size: " << bufferSize << std::endl;

            // Abrir conexão
            std::cout << "RTSPSource::setupNetworking() - Tentando abrir conexão..." << std::endl;
            int result = avformat_open_input(&formatContext_, rtspConfig_.url.c_str(), nullptr, &options);

            if (result < 0) {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(result, errbuf, sizeof(errbuf));
                std::string error = "Falha ao abrir stream: ";
                error += errbuf;
                throw std::runtime_error(error);
            }

            std::cout << "RTSPSource::setupNetworking() - Conexão aberta, buscando informações do stream..." <<
                    std::endl;

            if (avformat_find_stream_info(formatContext_, nullptr) < 0) {
                throw std::runtime_error("Falha ao obter informações do stream");
            }

            std::cout << "RTSPSource::setupNetworking() - Setup completo com sucesso" << std::endl;
            return true;
        } catch (const std::exception &e) {
            std::cerr << "RTSPSource::setupNetworking() - Erro: " << e.what() << std::endl;
            if (options) {
                av_dict_free(&options);
            }
            if (formatContext_) {
                avformat_close_input(&formatContext_);
                formatContext_ = nullptr;
            }
            return false;
        }
    }

    bool RTSPSource::setupRTSPOptions(AVDictionary **options) {
        av_dict_set_int(options, "stimeout", rtspConfig_.timeout, 0);
        av_dict_set_int(options, "buffer_size", rtspConfig_.advanced.bufferSize, 0);
        av_dict_set_int(options, "max_delay", rtspConfig_.advanced.maxDelay, 0);

        if (rtspConfig_.advanced.lowLatency) {
            av_dict_set(options, "flags", "low_delay", 0);
        }

        av_dict_set_int(options, "reorder_queue_size", rtspConfig_.advanced.rtpBufferSize, 0);

        return true;
    }

    bool RTSPSource::configureRTSPTransport() {
        if (!formatContext_) {
            std::cerr << "Format context is null\n";
            return false;
        }

        // Criar um dicionário de opções em vez de usar av_opt_set
        AVDictionary *options = nullptr;
        const char *rtsp_transport = rtspConfig_.useTCP ? "tcp" : "udp";

        // Adicionar opção ao dicionário
        int ret = av_dict_set(&options, "rtsp_transport", rtsp_transport, 0);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "Failed to set rtsp_transport option: " << errbuf << "\n";
            return false;
        }

        // Reabrir o input com as novas opções
        ret = avformat_open_input(&formatContext_, rtspConfig_.url.c_str(), nullptr, &options);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "Failed to reopen input with transport options: " << errbuf << "\n";
            av_dict_free(&options);
            return false;
        }

        // Liberar o dicionário
        av_dict_free(&options);

        // Encontrar informações do stream
        ret = avformat_find_stream_info(formatContext_, nullptr);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "Failed to find stream info: " << errbuf << "\n";
            return false;
        }

        return true;
    }

    void RTSPSource::updateStatus() {
        if (!formatContext_ || videoStreamIndex_ < 0) return;

        AVStream *stream = formatContext_->streams[videoStreamIndex_];

        // Calcular latência média com base na taxa de quadros (frame rate)
        if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0) {
            float frameTime = static_cast<float>(stream->avg_frame_rate.den) /
                              static_cast<float>(stream->avg_frame_rate.num);
            status_.averageLatency = frameTime * 1000.0f; // Convertido para milissegundos
        }

        // Calcular perda de pacotes - Implementação manual
        if (stream->nb_frames > 0) {
            int64_t totalFrames = stream->nb_frames;

            // Verifique se o rastreamento manual está implementado
            int64_t droppedFrames = totalFrames - status_.framesReceived; // Exemplificando uma lógica
            if (droppedFrames > 0) {
                status_.packetLoss = static_cast<float>(droppedFrames) / totalFrames;
            } else {
                status_.packetLoss = 0.0f; // Sem perda detectada
            }
        } else {
            status_.packetLoss = 0.0f; // Nenhum frame registrado ainda
        }
    }

    RTSPSource::RTSPStatus RTSPSource::getStatus() const {
        return status_;
    }
} // namespace turbovision
