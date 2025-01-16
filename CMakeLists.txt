cmake_minimum_required(VERSION 3.10)
project(TurboVision VERSION 1.0.0 LANGUAGES CXX)

# Opções
option(BUILD_EXAMPLES "Build example applications" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# Configurações globais
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configurar diretório de saída
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Configuração do FFmpeg para Windows
if(WIN32)
    set(FFMPEG_ROOT "C:/ffmpeg" CACHE PATH "FFmpeg root directory")
    set(FFMPEG_INCLUDE_DIRS "${FFMPEG_ROOT}/include")
    set(FFMPEG_LIBRARY_DIRS "${FFMPEG_ROOT}/lib")
    set(FFMPEG_LIBRARIES
            "${FFMPEG_ROOT}/lib/avcodec.lib"
            "${FFMPEG_ROOT}/lib/avformat.lib"
            "${FFMPEG_ROOT}/lib/avutil.lib"
            "${FFMPEG_ROOT}/lib/swscale.lib"
            "${FFMPEG_ROOT}/lib/avdevice.lib"
    )

    # Copia DLLs para output
    file(GLOB FFMPEG_DLLS "${FFMPEG_ROOT}/bin/*.dll")
    file(COPY ${FFMPEG_DLLS} DESTINATION ${CMAKE_BINARY_DIR}/bin)

    # Adiciona definições necessárias para Windows
    add_definitions(
            -D_WIN32_WINNT=0x0601
            -D__STDC_CONSTANT_MACROS
            -D__STDC_LIMIT_MACROS
            -D__STDC_FORMAT_MACROS
    )
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
            libavcodec
            libavformat
            libavutil
            libswscale
            libavdevice
    )
endif()

# Incluir diretórios
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${FFMPEG_INCLUDE_DIRS}
)

if(WIN32)
    link_directories(${FFMPEG_LIBRARY_DIRS})
endif()

# Adicionar subdiretórios
add_subdirectory(src)

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Instalação
install(
        DIRECTORY ${CMAKE_SOURCE_DIR}/include/turbovision
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp"
)

if(WIN32)
    install(
            FILES ${FFMPEG_DLLS}
            DESTINATION bin
    )
endif()

install(
        TARGETS turbovision
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include/turbovision
)

# Pacote de desenvolvimento
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libavcodec-dev, libavformat-dev, libavutil-dev, libswscale-dev, libavdevice-dev")
endif()

set(CPACK_PACKAGE_NAME "turbovision")
set(CPACK_PACKAGE_VENDOR "YourCompany")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Hardware-accelerated video processing library")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

include(CPack)