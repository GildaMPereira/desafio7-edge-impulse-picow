cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(desafio_7_ml_pico_w C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

add_executable(app
    main.cpp
)

target_include_directories(app PRIVATE
    .
    edge-impulse
)

target_compile_definitions(app PRIVATE
    EI_CLASSIFIER_TFLITE_ENABLE_CMSIS_NN=0
)

target_link_libraries(app
    pico_stdlib
    hardware_i2c
)

# Edge Impulse C++/MCU library. The generated EI CMake expects a target named "app".
add_subdirectory(edge-impulse)

pico_enable_stdio_usb(app 0)
pico_enable_stdio_uart(app 1)

pico_add_extra_outputs(app)
