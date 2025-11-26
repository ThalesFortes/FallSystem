# Lista dos arquivos do display
set(DISPLAY_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/drivers/display/ssd1306.c
    ${CMAKE_CURRENT_LIST_DIR}/drivers/display/ssd1306_conf.h
    ${CMAKE_CURRENT_LIST_DIR}/drivers/display/ssd1306_fonts.c
    ${CMAKE_CURRENT_LIST_DIR}/drivers/display/ssd1306_bitmaps.c
)

set(MPU6050
    ${CMAKE_CURRENT_LIST_DIR}/drivers/mpu6050/mpu6050.c
    ${CMAKE_CURRENT_LIST_DIR}/drivers/mpu6050/mpu6050.h
)

set(FALL_DETECTOR
    ${CMAKE_CURRENT_LIST_DIR}/drivers/fallDetector/fall_detector.c
    ${CMAKE_CURRENT_LIST_DIR}/drivers/fallDetector/fall_detector.h
)

set(SIMULATION
    ${CMAKE_CURRENT_LIST_DIR}/drivers/simulation/sim_task.c
    ${CMAKE_CURRENT_LIST_DIR}/drivers/simulation/sim_task.h
)

set(BH1750
    ${CMAKE_CURRENT_LIST_DIR}/drivers/bh1750/bh1750.c
    ${CMAKE_CURRENT_LIST_DIR}/drivers/bh1750/bh1750.h
)

add_executable(${ProjectName}
    main.c
    ${DISPLAY_SOURCES}
    ${MPU6050}
    ${FALL_DETECTOR}
    ${SIMULATION}
    ${BH1750}
)

pico_enable_stdio_uart(${ProjectName} 0)
pico_enable_stdio_usb(${ProjectName} 1)

# Adicionar includes da pasta display
target_include_directories(${ProjectName} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/drivers/display
    ${CMAKE_CURRENT_LIST_DIR}/drivers/mpu6050
    ${CMAKE_CURRENT_LIST_DIR}/drivers/fallDetector
    ${CMAKE_CURRENT_LIST_DIR}/drivers/simulation
)

target_link_libraries(${ProjectName}
    pico_stdlib
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    hardware_i2c     
    hardware_gpio
    hardware_adc
    hardware_pwm  
)

pico_add_extra_outputs(${ProjectName})
