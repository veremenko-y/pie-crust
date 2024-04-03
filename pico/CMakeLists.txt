cmake_minimum_required(VERSION 3.12)

set(PROJECT_NAME piecrust)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_BUILD_TYPE Debug)
set(PICO_COPY_TO_RAM 1)
set(PICO_BOARD pico_w)

add_compile_definitions(CYW43_CHIPSET_FIRMWARE_INCLUDE_FILE="flash_firmware.h")
include(pico_sdk_import.cmake)
project(${PROJECT_NAME} C CXX ASM)
pico_sdk_init()
#set(FLASH "__attribute__((aligned(4))) __in_flash(\"wifi_firmware\")")
#add_definitions(-DCYW43_RESOURCE_ATTRIBUTE="${FLASH}")
# Important, must be configured before pico_sdk_init
# and before add_executable
# add_compile_options(-Wall
#         -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
#         -Wno-unused-function # we have some for the docs that aren't called
#         -Wno-maybe-uninitialized
#         -g
#         -fverbose-asm
#         )
add_executable(${PROJECT_NAME})
pico_add_extra_outputs(${PROJECT_NAME})
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/bus.pio)
pico_set_float_implementation(${PROJECT_NAME} none)
pico_set_double_implementation(${PROJECT_NAME} none)

target_sources(${PROJECT_NAME} PRIVATE
        main.cxx
        binary.s
        )

# file(GLOB firmware "${CMAKE_CURRENT_SOURCE_DIR}/firmware_files/*")
# message(status Copy firmware)
# foreach(f ${firmware})
#         message(status "From ${f}")
#         get_filename_component(outputFileName ${f} NAME)
#         message(status "To ${CMAKE_CURRENT_BINARY_DIR}/${outputFileName}")
#         configure_file(${f} ${CMAKE_CURRENT_BINARY_DIR}/${outputFileName} COPYONLY)
# endforeach()

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        )

target_link_libraries(${PROJECT_NAME}
        pico_stdlib
        pico_cyw43_arch_lwip_poll
        pico_multicore
        hardware_gpio
        hardware_pio
        hardware_rtc
        )
