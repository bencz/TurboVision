#include "turbovision/sources/video_source.hpp"

#include <iostream>

namespace turbovision {
    VideoSource::VideoSource(const VideoConfig &config)
        : config_(config)
          , formatContext_(nullptr)
          , codecContext_(nullptr)
          , videoStreamIndex_(-1)
          , isRunning_(false)
          , isPaused_(false) {
        hwManager_ = std::make_shared<HardwareManager>(config.deviceType);
    }

    VideoSource::~VideoSource() {
        stop();

        if (codecContext_) {
            avcodec_free_context(&codecContext_);
        }
        if (formatContext_) {
            avformat_close_input(&formatContext_);
        }
    }

    bool VideoSource::start() {
        if (isRunning_) {
            return false;
        }

        if (!initializeSource()) {
            return false;
        }

        isRunning_ = true;
        isPaused_ = false;
        captureThread_ = std::thread(&VideoSource::captureLoop, this);
        return true;
    }

    void VideoSource::stop() {
        isRunning_ = false;
        isPaused_ = false;

        if (captureThread_.joinable()) {
            captureThread_.join();
        }

        clearFrameQueue();
        cleanupSource();
    }

    bool VideoSource::pause() {
        if (!isRunning_) return false;
        isPaused_ = true;
        return true;
    }

    bool VideoSource::resume() {
        if (!isRunning_) return false;
        isPaused_ = false;
        return true;
    }

    bool VideoSource::seek(int64_t timestamp) {
        if (!formatContext_ || videoStreamIndex_ < 0) {
            return false;
        }

        // Converter timestamp para a base de tempo do stream
        int64_t seekTarget = av_rescale_q(timestamp,
                                          AV_TIME_BASE_Q,
                                          formatContext_->streams[videoStreamIndex_]->time_base);

        if (av_seek_frame(formatContext_, videoStreamIndex_, seekTarget,
                          AVSEEK_FLAG_BACKWARD) < 0) {
            return false;
        }

        if (codecContext_) {
            avcodec_flush_buffers(codecContext_);
        }

        clearFrameQueue();
        return true;
    }

    void VideoSource::setFrameCallback(FrameCallback callback) {
        std::lock_guard<std::mutex> lock(frameMutex_);
        frameCallback_ = std::move(callback);
    }

    VideoSource::StreamInfo VideoSource::getStreamInfo() const {
        StreamInfo info{};

        if (codecContext_) {
            info.width = codecContext_->width;
            info.height = codecContext_->height;
            info.pixelFormat = codecContext_->pix_fmt;
            info.timeBase = codecContext_->time_base;
            info.frameRate = codecContext_->framerate;
        }

        if (formatContext_ && videoStreamIndex_ >= 0) {
            AVStream *stream = formatContext_->streams[videoStreamIndex_];
            info.duration = stream->duration;
            info.bitRate = stream->codecpar->bit_rate;
        }

        return info;
    }

    bool VideoSource::processPacket(AVPacket *packet) {
        // std::cout << "VideoSource::processPacket - Iniciando processamento..." << std::endl;

        if (!codecContext_) {
            std::cerr << "VideoSource::processPacket - Codec context é nulo" << std::endl;
            return false;
        }

        // std::cout << "VideoSource::processPacket - Enviando packet para decodificador..." << std::endl;
        int ret = avcodec_send_packet(codecContext_, packet);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "VideoSource::processPacket - Erro ao enviar packet: " << errbuf << std::endl;
            return false;
        }

        // std::cout << "VideoSource::processPacket - Alocando frames..." << std::endl;
        AVFrame *frame = av_frame_alloc();
        AVFrame *swFrame = av_frame_alloc();
        bool success = false;

        try {
            // std::cout << "VideoSource::processPacket - Iniciando loop de recebimento de frames..." << std::endl;
            while (true) {
                ret = avcodec_receive_frame(codecContext_, frame);

                // EAGAIN significa que precisamos de mais packets para gerar um frame
                if (ret == AVERROR(EAGAIN)) {
                    // std::cout << "VideoSource::processPacket - Precisa de mais dados (EAGAIN)" << std::endl;
                    success = true; // Não é um erro, apenas precisa de mais dados
                    break;
                }

                // EOF significa fim do stream
                if (ret == AVERROR_EOF) {
                    // std::cout << "VideoSource::processPacket - Fim do stream (EOF)" << std::endl;
                    break;
                }

                // Qualquer outro erro negativo é um erro real
                if (ret < 0) {
                    char errbuf[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    std::cerr << "VideoSource::processPacket - Erro ao receber frame: " << errbuf << std::endl;
                    break;
                }

                // Se chegamos aqui, temos um frame válido
                // std::cout << "VideoSource::processPacket - Frame recebido com sucesso" << std::endl;

                if (frame->hw_frames_ctx) {
                    // std::cout << "VideoSource::processPacket - Frame está na GPU, transferindo..." << std::endl;
                    if (!transferFrameFromGPU(frame, swFrame)) {
                        std::cerr << "VideoSource::processPacket - Falha na transferência GPU->CPU" << std::endl;
                        continue;
                    }
                    success = processFrame(swFrame);
                } else {
                    // std::cout << "VideoSource::processPacket - Processando frame da CPU..." << std::endl;
                    success = processFrame(frame);
                }
            }
        } catch (const std::exception &e) {
            std::cerr << "VideoSource::processPacket - Exceção: " << e.what() << std::endl;
        }

        // std::cout << "VideoSource::processPacket - Liberando frames..." << std::endl;
        av_frame_free(&frame);
        av_frame_free(&swFrame);
        return success;
    }

    bool VideoSource::processFrame(AVFrame *frame) {
        // std::cout << "VideoSource::processFrame - Iniciando processamento..." << std::endl;

        if (!frame) {
            std::cerr << "VideoSource::processFrame - Frame é nulo" << std::endl;
            return false;
        }

        // std::cout << "VideoSource::processFrame - Informações do frame:"
        //           << "\n  Width: " << frame->width
        //           << "\n  Height: " << frame->height
        //           << "\n  Format: " << av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format))
        //           << "\n  LineSize[0]: " << frame->linesize[0]
        //           << std::endl;

        try {
            auto frameData = std::make_shared<FrameData>(
                frame->width,
                frame->height,
                static_cast<AVPixelFormat>(frame->format)
            );

            // std::cout << "VideoSource::processFrame - FrameData criado, copiando planos..." << std::endl;

            // Verificar número de planos e copiar cada um
            for (int i = 0; i < AV_NUM_DATA_POINTERS && frame->data[i]; i++) {
                if (!frame->data[i]) break;

                int planeSize;
                if (i == 0) {
                    // Plano Y (luminância)
                    planeSize = frame->linesize[0] * frame->height;
                } else {
                    // Planos UV (crominância) - tipicamente metade da altura e largura
                    planeSize = frame->linesize[i] * (frame->height / 2);
                }

                // std::cout << "VideoSource::processFrame - Copiando plano " << i
                //           << ", tamanho: " << planeSize << std::endl;

                memcpy(frameData->data() + frameData->getPlanOffset(i),
                       frame->data[i],
                       planeSize);
            }

            frameData->setTimestamp(frame->pts);

            // std::cout << "VideoSource::processFrame - Chamando callback..." << std::endl;
            std::lock_guard<std::mutex> lock(frameMutex_);
            if (frameCallback_) {
                frameCallback_(frameData);
            }

            // std::cout << "VideoSource::processFrame - Frame processado com sucesso" << std::endl;
            return true;
        } catch (const std::exception &e) {
            std::cerr << "VideoSource::processFrame - Exceção: " << e.what()
                    << "\nStack trace (se disponível): " << std::endl;
            return false;
        }
    }

    bool VideoSource::transferFrameFromGPU(AVFrame *hwFrame, AVFrame *swFrame) {
        if (av_hwframe_transfer_data(swFrame, hwFrame, 0) < 0) {
            return false;
        }
        swFrame->pts = hwFrame->pts;
        return true;
    }

    void VideoSource::clearFrameQueue() {
        std::lock_guard<std::mutex> lock(frameMutex_);
        while (!frameQueue_.empty()) {
            frameQueue_.pop();
        }
    }
} // namespace turbovision
