cmake_minimum_required(VERSION 3.16)
project(bin_packing_tabu_search_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(bin_packing
    src/main.cpp
    src/Package.h
    src/Package.cpp
    src/Bin.h
    src/Bin.cpp
    src/TabuSearch.h
    src/TabuSearch.cpp
    src/Visualize.h
    src/Visualize.cpp
)

if (MSVC)
  target_compile_options(bin_packing PRIVATE /W4 /permissive-)
else()
  target_compile_options(bin_packing PRIVATE -Wall -Wextra -Wpedantic -O2)
endif()


