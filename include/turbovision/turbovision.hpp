#pragma once

// Core
#include "core/common.hpp"
#include "core/frame_data.hpp"
#include "core/hardware_manager.hpp"
#include "core/video_config.hpp"
#include "core/utils.hpp"

// Sources
#include "sources/video_source.hpp"
#include "sources/camera_source.hpp"
#include "sources/rtsp_source.hpp"
#include "sources/source_factory.hpp"

// Server
#include "server/server_config.hpp"
#include "server/rtsp_server.hpp"

namespace turbovision {

/**
 * @brief Inicializa a biblioteca TurboVision
 *
 * Esta função deve ser chamada antes de usar qualquer funcionalidade da biblioteca.
 * Ela inicializa os componentes internos necessários e registra os dispositivos.
 *
 * @return true se a inicialização foi bem sucedida
 */
TURBOVISION_API bool initialize();

/**
 * @brief Finaliza a biblioteca TurboVision
 *
 * Deve ser chamada quando não for mais necessário usar a biblioteca.
 * Libera recursos e finaliza componentes.
 */
TURBOVISION_API void shutdown();

/**
 * @brief Verifica se há suporte a aceleração de hardware
 *
 * @return true se houver suporte a hardware
 */
TURBOVISION_API bool hasHardwareSupport();

/**
 * @brief Retorna informações sobre os dispositivos de hardware disponíveis
 *
 * @return std::vector<HardwareManager::DeviceInfo> lista de dispositivos
 */
TURBOVISION_API std::vector<HardwareManager::DeviceInfo> getHardwareInfo();

/**
 * @brief Retorna a versão da biblioteca
 *
 * @return std::string versão no formato "X.Y.Z"
 */
TURBOVISION_API std::string getVersion();

/**
 * @brief Configura o nível de log
 *
 * @param level Nível de log desejado
 */
TURBOVISION_API void setLogLevel(utils::LogLevel level);

/**
 * @brief Configura callback para logs
 *
 * @param callback Função que será chamada para cada mensagem de log
 */
TURBOVISION_API void setLogCallback(utils::LogCallback callback);

/**
 * @brief Verifica se um dispositivo de hardware específico está disponível
 *
 * @param type Tipo de dispositivo a verificar
 * @return true se o dispositivo estiver disponível
 */
TURBOVISION_API bool isHardwareAvailable(DeviceType type);

} // namespace turbovision