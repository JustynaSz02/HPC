cmake_minimum_required(VERSION 3.18)
project(bin_packing_tabu_search_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sprawdź czy CUDA jest dostępne
set(USE_CUDA FALSE)
set(CUDA_FOUND FALSE)

# Znajdź CUDA Toolkit używając find_package
if(EXISTS "${CMAKE_SOURCE_DIR}/src/GpuScoring.cu")
    message(STATUS "GpuScoring.cu found - attempting to enable CUDA")
    
    # Znajdź CUDA_PATH
    set(CUDA_PATH "$ENV{CUDA_PATH}")
    if(NOT CUDA_PATH)
        set(CUDA_PATH "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.4")
    endif()
    
    # Ustaw CUDA_PATH dla find_package
    if(EXISTS "${CUDA_PATH}")
        set(CUDAToolkit_ROOT "${CUDA_PATH}")
        set(ENV{CUDAToolkit_ROOT} "${CUDA_PATH}")
    endif()
    
    # Znajdź CUDA Toolkit
    find_package(CUDAToolkit QUIET)
    
    if(CUDAToolkit_FOUND)
        message(STATUS "✅ CUDAToolkit found: ${CUDAToolkit_ROOT_DIR}")
        if(CUDAToolkit_ROOT_DIR)
            set(CUDA_PATH "${CUDAToolkit_ROOT_DIR}")
        else()
            # Fallback na domyślną ścieżkę
            set(CUDA_PATH "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.4")
        endif()
    endif()
    
    # Debug: wyświetl aktualną ścieżkę CUDA
    message(STATUS "CUDA_PATH is set to: ${CUDA_PATH}")
    
    # Znajdź nvcc - sprawdź różne możliwe ścieżki
    set(NVCC_EXECUTABLE "")
    set(NVCC_PATHS
        "${CUDA_PATH}/bin/nvcc.exe"
        "$ENV{CUDA_PATH}/bin/nvcc.exe"
        "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.4/bin/nvcc.exe"
        "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/bin/nvcc.exe"
        "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.2/bin/nvcc.exe"
        "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.1/bin/nvcc.exe"
        "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.0/bin/nvcc.exe"
    )
    
    foreach(NVCC_PATH ${NVCC_PATHS})
        if(EXISTS "${NVCC_PATH}")
            set(NVCC_EXECUTABLE "${NVCC_PATH}")
            break()
        endif()
    endforeach()
    
    # Jeśli nie znaleziono, spróbuj find_program
    if(NOT NVCC_EXECUTABLE)
        find_program(NVCC_EXECUTABLE nvcc.exe
            PATHS
            "${CUDA_PATH}/bin"
            "$ENV{CUDA_PATH}/bin"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.4/bin"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/bin"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.2/bin"
        )
    endif()
    
    if(NVCC_EXECUTABLE AND EXISTS "${CUDA_PATH}")
        message(STATUS "Found nvcc: ${NVCC_EXECUTABLE}")
        message(STATUS "CUDA path: ${CUDA_PATH}")
        
        # Ustaw zmienne CUDA PRZED enable_language
        set(CMAKE_CUDA_COMPILER "${NVCC_EXECUTABLE}" CACHE FILEPATH "CUDA compiler" FORCE)
        set(CMAKE_CUDA_TOOLKIT_ROOT_DIR "${CUDA_PATH}" CACHE PATH "CUDA toolkit root" FORCE)
        
        # Wymuś użycie nvcc (pomiń sprawdzanie toolset)
        set(CMAKE_CUDA_COMPILER_FORCED TRUE CACHE BOOL "Force CUDA compiler" FORCE)
        
        # Visual Studio nie wspiera CUDA toolset bezpośrednio przez enable_language
        # Użyjemy custom command do kompilacji CUDA bez enable_language
        message(STATUS "⚠️  Visual Studio CUDA integration not available - using custom compilation")
        message(STATUS "This is normal for Visual Studio - CUDA will be compiled separately")
        
        # Użyj custom command do kompilacji CUDA
        set(USE_CUDA TRUE)
        set(CUDA_FOUND TRUE)
        add_definitions(-DHAVE_CUDA)
        remove_definitions(-DNO_CUDA)
        message(STATUS "✅ CUDA will be compiled using custom command")
    else()
        message(STATUS "nvcc not found - using CPU fallback")
        add_definitions(-DNO_CUDA)
    endif()
else()
    message(STATUS "GpuScoring.cu not found - using CPU fallback")
    add_definitions(-DNO_CUDA)
endif()

add_executable(bin_packing
    src/main.cpp
    src/Package.h
    src/Package.cpp
    src/Bin.h
    src/Bin.cpp
    src/Move.h
    src/TabuSearch.h
    src/TabuSearch.cpp
    src/GpuData.h
    src/GpuData.cpp
    src/GpuScoring.h
    src/GpuScoring.cpp
    src/Visualize.h
    src/Visualize.cpp
)

# Dodaj plik CUDA jeśli CUDA jest włączone
if(USE_CUDA AND CUDA_FOUND AND NVCC_EXECUTABLE)
    # Workaround dla Visual Studio - użyj custom command
    set(GPU_SCORING_OBJ "${CMAKE_BINARY_DIR}/GpuScoring.obj")
    
    # Architektura GPU - można ustawić przez -DCUDA_ARCH=sm_XX podczas konfiguracji CMake
    # Domyślnie: sm_75 (RTX 20xx, GTX 16xx) - kompatybilne wstecz z sm_60+
    if(NOT DEFINED CUDA_ARCH)
        set(CUDA_ARCH "sm_75")
    endif()
    message(STATUS "CUDA architecture: ${CUDA_ARCH}")
    message(STATUS "  (możesz zmienić przez: cmake -DCUDA_ARCH=sm_XX)")
    
    # Kompiluj CUDA do obiektu
    # Użyj tego samego runtime library co reszta projektu
    # Dla Release użyj /MD, dla Debug /MDd
    if(MSVC)
        # Domyślnie Release (MD)
        set(CUDA_RUNTIME_FLAG "/MD")
        set(CUDA_DEBUG_FLAG "/DNDEBUG")
        # Sprawdź czy to Debug build
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(CUDA_RUNTIME_FLAG "/MDd")
            set(CUDA_DEBUG_FLAG "")
        endif()
    endif()
    
    add_custom_command(
        OUTPUT ${GPU_SCORING_OBJ}
        COMMAND ${NVCC_EXECUTABLE}
            -c ${CMAKE_SOURCE_DIR}/src/GpuScoring.cu
            -o ${GPU_SCORING_OBJ}
            -I${CMAKE_SOURCE_DIR}/src
            -I${CUDA_PATH}/include
            -std=c++17
            -arch=${CUDA_ARCH}
            -Xcompiler "/EHsc /W4 ${CUDA_RUNTIME_FLAG} ${CUDA_DEBUG_FLAG}"
            -DHAVE_CUDA
        DEPENDS ${CMAKE_SOURCE_DIR}/src/GpuScoring.cu
            ${CMAKE_SOURCE_DIR}/src/GpuScoring.h
            ${CMAKE_SOURCE_DIR}/src/GpuData.h
            ${CMAKE_SOURCE_DIR}/src/Move.h
            ${CMAKE_SOURCE_DIR}/src/TabuSearch.h
        COMMENT "Compiling CUDA file: GpuScoring.cu"
    )
    
    # Dodaj obiekt do targetu
    target_sources(bin_packing PRIVATE ${GPU_SCORING_OBJ})
    
    # Linkuj z CUDA runtime
    target_link_directories(bin_packing PRIVATE "${CUDA_PATH}/lib/x64")
    target_link_libraries(bin_packing PRIVATE cudart)
    
    # Dodaj include directories
    target_include_directories(bin_packing PRIVATE "${CUDA_PATH}/include")
    
    message(STATUS "✅ GpuScoring.cu will be compiled with CUDA (custom command)")
else()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/GpuScoring.cu")
        message(WARNING "CUDA file exists but CUDA is not enabled - using CPU fallback")
    endif()
endif()

if (MSVC)
  target_compile_options(bin_packing PRIVATE /W4 /permissive-)
else()
  target_compile_options(bin_packing PRIVATE -Wall -Wextra -Wpedantic -O2)
endif()


