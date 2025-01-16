#include <turbovision/turbovision.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>

using namespace turbovision;

// Flag global para controle
std::atomic<bool> running{true};

int main() {
    try {
        std::cout << "Inicializando TurboVision..." << std::endl;
        initialize();

        std::cout << "Configurando conexão RTSP..." << std::endl;

        VideoConfig vidConfig;
        vidConfig.width = 1920;
        vidConfig.height = 1080;
        vidConfig.fps = 20;
        vidConfig.deviceType = DeviceType::AUTO;

        RTSPSource::RTSPConfig rtspConfig;
        rtspConfig.url = "rtsp://107.178.220.235:8554/live/liveStream_ROZL4721943W7_0_0";
        rtspConfig.useTCP = true;
        rtspConfig.reconnectOnError = true;
        rtspConfig.timeout = 5000000; // 5 segundos em microssegundos

        std::cout << "Criando fonte RTSP..." << std::endl;
        auto source = std::make_shared<RTSPSource>(vidConfig, rtspConfig);
        if (!source) {
            throw Exception("Falha ao criar RTSPSource");
        }

        // Configurar callback
        source->setFrameCallback([](FramePtr frame) {
            static int frameCount = 0;
            static auto startTime = std::chrono::steady_clock::now();
            static auto lastPrint = startTime;

            frameCount++;
            auto currentTime = std::chrono::steady_clock::now();

            // Atualizar estatísticas a cada segundo
            auto timeSinceLastPrint = std::chrono::duration_cast<std::chrono::milliseconds>(
                                          currentTime - lastPrint).count() / 1000.0;

            if (timeSinceLastPrint >= 1.0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   currentTime - startTime).count() / 1000.0;

                if (elapsed >= 1.0) {
                    float fps = frameCount / elapsed;

                    // Limpar linha atual e imprimir estatísticas
                    std::cout << "\r\033[K" // Limpa a linha atual
                            << "FPS Médio: " << std::fixed << std::setprecision(2) << fps
                            << " | Frames Recebidos: " << frameCount
                            << " | Pixel format: " << frame->format()
                            << " | Tempo: " << std::setprecision(2) << elapsed << "s"
                            << std::flush;

                    lastPrint = currentTime;
                }
            }
        });

        std::cout << "Iniciando captura..." << std::endl;
        if (!source->start()) {
            throw Exception("Falha ao iniciar captura");
        }

        std::cout << "Captura iniciada. Pressione Ctrl+C para sair." << std::endl;

        // Loop principal
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            if (!source->isRunning()) {
                std::cout << "Fonte parou de executar. Tentando reiniciar..." << std::endl;
                if (!source->start()) {
                    throw Exception("Falha ao reiniciar fonte");
                }
            }
        }

        std::cout << "Encerrando..." << std::endl;
        source->stop();
    } catch (const Exception &e) {
        std::cerr << "Erro: " << e.what() << std::endl;
        return 1;
    }

    shutdown();
    return 0;
}
