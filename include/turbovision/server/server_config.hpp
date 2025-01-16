#pragma once

#include "turbovision/core/common.hpp"
#include <string>

namespace turbovision {

struct TURBOVISION_API ServerConfig {
    // Configurações básicas
    std::string address = "0.0.0.0";      // Endereço de bind
    int port = 8554;                      // Porta RTSP padrão
    std::string streamName = "stream";    // Nome do stream
    int maxClients = 10;                  // Máximo de clientes simultâneos
    bool useTCP = true;                   // Usar TCP ao invés de UDP

    // Configurações do codificador
    struct EncoderConfig {
        std::string encoder = "h264_nvenc";  // Encoder padrão (NVIDIA)
        int bitrate = 4000000;               // 4 Mbps
        int gopSize = 30;                    // GOP size
        bool lowLatency = true;              // Modo de baixa latência

        // Configurações avançadas do encoder
        struct Advanced {
            std::string preset = "p4";       // Preset de qualidade
            std::string tune = "ull";        // Ultra low latency
            bool zeroLatency = true;         // Zero latency mode
            int maxBFrames = 0;              // Sem B-frames para menor latência
            int keyintMin = 15;              // GOP mínimo
            int threads = 0;                 // 0 = auto
            int qp = 23;                     // Qualidade (0-51, menor = melhor)
            std::string profile = "high";    // Perfil H.264

            Advanced() = default;
        } advanced;

        EncoderConfig() = default;
    } encoder;

    // Configurações de rede
    struct NetworkConfig {
        int bufferSize = 1024 * 1024;       // 1MB buffer de rede
        int timeout = 30;                    // Timeout em segundos
        bool enableMulticast = false;        // Habilitar multicast
        std::string multicastAddress;        // Endereço multicast
        int multicastTTL = 1;               // TTL multicast
        int maxBitrate = 10000000;          // Bitrate máximo em bps (10 Mbps)
        int rateControl = 0;                // 0 = auto

        // Configurações de QoS
        struct QoSConfig {
            bool enabled = false;
            int dscp = 0;                   // Differentiated Services Code Point
            int priority = 0;               // Prioridade SO

            QoSConfig() = default;
        } qos;

        NetworkConfig() = default;
    } network;

    // Configurações de segurança
    struct SecurityConfig {
        bool requireAuth = false;
        std::string username;
        std::string password;
        bool enableTLS = false;
        std::string certificatePath;
        std::string privateKeyPath;

        SecurityConfig() = default;
    } security;

    ServerConfig() = default;
};

} // namespace turbovision