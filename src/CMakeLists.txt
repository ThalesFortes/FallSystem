### Primeiro: garantir que a placa é o Pico W
set(PICO_BOARD pico_w)

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

set(VL53L0X
    ${CMAKE_CURRENT_LIST_DIR}/drivers/vl53l0x/vl53l0x.c
    ${CMAKE_CURRENT_LIST_DIR}/drivers/vl53l0x/vl53l0x.h
)

set(LEDS
    ${CMAKE_CURRENT_LIST_DIR}/drivers/leds/leds.c
    ${CMAKE_CURRENT_LIST_DIR}/drivers/leds/leds.h
)


add_executable(${ProjectName}
    main.c
    mqtt_task.c
    mqtt_task.h
    ${DISPLAY_SOURCES}
    ${MPU6050}
    ${FALL_DETECTOR}
    ${SIMULATION}
    ${VL53L0X}
    ${LEDS}
)

pico_enable_stdio_uart(${ProjectName} 0)
pico_enable_stdio_usb(${ProjectName} 1)

add_compile_definitions(
    WIFI_SSID="minhaWifi"
    WIFI_PASSWORD="senha123"
    MQTT_SERVER="192.168.1.10"
    MQTT_PORT=1883
    MQTT_USERNAME="user"
    MQTT_PASSWORD="pass"
)

target_include_directories(${ProjectName} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/drivers/display
    ${CMAKE_CURRENT_LIST_DIR}/drivers/mpu6050
    ${CMAKE_CURRENT_LIST_DIR}/drivers/fallDetector
    ${CMAKE_CURRENT_LIST_DIR}/drivers/simulation

    ### Includes corretos para CYW43 + LWIP + MQTT
    ${PICO_SDK_PATH}/src/rp2_common/cyw43_arch/include
    ${PICO_SDK_PATH}/lib/lwip/src/include
    ${PICO_SDK_PATH}/lib/lwip/src/include/lwip
    ${PICO_SDK_PATH}/lib/lwip/src/include/arch
    ${PICO_SDK_PATH}/lib/lwip/src/include/apps
    ${PICO_SDK_PATH}/lib/lwip/src/include/compat
)

target_link_libraries(${ProjectName}
    pico_stdlib
    hardware_i2c     
    hardware_gpio
    hardware_adc
    hardware_pwm  

    ### FreeRTOS (obrigatório para SMP)
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4

    ### Wi-Fi + MQTT + TCP/IP
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
    pico_unique_id
)

pico_add_extra_outputs(${ProjectName})
