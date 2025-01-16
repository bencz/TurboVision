# Coletar arquivos fonte
file(GLOB_RECURSE SOURCES
        "core/*.cpp"
        "sources/*.cpp"
        "server/*.cpp"
        "*.cpp"
)

# Criar biblioteca dinâmica
add_library(turbovision SHARED ${SOURCES})

# Configurações específicas do Windows
if(WIN32)
    target_compile_definitions(turbovision
            PRIVATE
            TURBOVISION_EXPORTS
            PUBLIC
            TURBOVISION_DLL
    )
endif()

# Configurações de compilação
if(MSVC)
    target_compile_options(turbovision PRIVATE
            $<$<CONFIG:Release>:/O2>  # Otimização máxima
            $<$<CONFIG:Release>:/Oi>  # Expansão de funções inline
            $<$<CONFIG:Release>:/GL>  # Otimizações de link-time
            $<$<CONFIG:Release>:/Gy>  # Funções separadas para otimização
            $<$<CONFIG:Release>:/arch:AVX2> # Habilitar instruções AVX2
            $<$<CONFIG:Release>:/fp:fast>  # Matemática de ponto flutuante rápida
            $<$<CONFIG:Release>:/favor:INTEL64> # Otimização para CPUs modernas
            $<$<CONFIG:Release>:/Ot>  # Favor desempenho sobre tamanho
            $<$<CONFIG:Release>:/Qpar> # Paralelismo automático
    )
elseif(UNIX)
    target_compile_options(turbovision PRIVATE
            $<$<CONFIG:Release>:-O3>  # Otimização máxima
            $<$<CONFIG:Release>:-march=native> # Otimizações específicas do hardware local
            $<$<CONFIG:Release>:-ffast-math>  # Matemática de ponto flutuante rápida
            $<$<CONFIG:Release>:-funroll-loops> # Desenrolar loops para melhor performance
            $<$<CONFIG:Release>:-fomit-frame-pointer> # Remove ponteiro de quadro para otimização
            $<$<CONFIG:Release>:-fno-signed-zeros> # Otimizações adicionais de ponto flutuante
            $<$<CONFIG:Release>:-falign-functions=32> # Alinha funções em limites de 32 bytes
            $<$<CONFIG:Release>:-fno-math-errno> # Melhora o desempenho de operações matemáticas
    )
endif()

# Incluir diretórios
target_include_directories(turbovision
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Linkar com FFmpeg
if(WIN32)
    target_link_libraries(turbovision
            PUBLIC ${FFMPEG_LIBRARIES}
    )
else()
    target_link_libraries(turbovision
            PUBLIC PkgConfig::FFMPEG
    )
endif()

# Configuração da biblioteca
set_target_properties(turbovision PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
