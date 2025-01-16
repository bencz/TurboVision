# examples/CMakeLists.txt

# Define os exemplos C++
set(CPP_EXAMPLES
        capture_example
        server_example
)

# Criar diretório para executáveis
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

# Criar executáveis para cada exemplo C++
foreach(EXAMPLE ${CPP_EXAMPLES})
    # Adiciona executável com o source da pasta cpp/
    add_executable(${EXAMPLE}
            cpp/${EXAMPLE}.cpp
    )

    # Linka com a biblioteca TurboVision
    target_link_libraries(${EXAMPLE}
            PRIVATE turbovision
    )

    # Configurações de compilação
    set_target_properties(${EXAMPLE} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    if(MSVC)
        # Define o local de saída para Debug/Release no Visual Studio
        set_target_properties(${EXAMPLE} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
                RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
        )

        # Adiciona otimizações em Release
        target_compile_options(${EXAMPLE} PRIVATE
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
    else()
        # Otimizações para GCC/Clang em Release
        target_compile_options(${EXAMPLE} PRIVATE
                $<$<CONFIG:Release>:-O3>
                $<$<CONFIG:Release>:-ffast-math>
                $<$<CONFIG:Release>:-funroll-loops>
                $<$<CONFIG:Release>:-fomit-frame-pointer>
                $<$<CONFIG:Release>:-fno-signed-zeros>
        )
    endif()

    # No Windows, adiciona comando post-build para copiar DLLs necessárias
    if(WIN32)
        add_custom_command(TARGET ${EXAMPLE} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:turbovision>
                $<TARGET_FILE_DIR:${EXAMPLE}>
        )
    endif()
endforeach()

# Instalação dos exemplos
install(
        TARGETS ${CPP_EXAMPLES}
        RUNTIME DESTINATION bin/examples
)