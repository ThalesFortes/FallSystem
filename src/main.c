#include "pico/cyw43_arch.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "drivers/simulation/sim_task.h"

#include "drivers/mpu6050/mpu6050.h"
#include "drivers/fallDetector/fall_detector.h"

#include "drivers/display/ssd1306.h"
#include "drivers/display/ssd1306_fonts.h"

#include "drivers/vl53l0x/vl53l0x.h" 
#include "drivers/leds/leds.h"

#include "mqtt_task.h"

#define LED_PIN 12
#define I2C_PORT i2c0
#define I2C_SDA_PIN 0
#define I2C_SCL_PIN 1
#define MPU_I2C_BAUDRATE 400000

#define SIM_PIN 5
#define BUZZER 21
#define BUTTON_PIN 6

extern SemaphoreHandle_t i2c_mutex = NULL;
static SemaphoreHandle_t button_sem = NULL;

#define LOG_INTERVAL_MS 1000
#define PROXIMITY_THRESHOLD_MM 200U
#define PROXIMITY_CONFIRM_COUNT 3

extern volatile bool proximity_alert = false;
extern volatile bool fall_detected = false;
extern volatile uint16_t vl53_last_range = 0;

void button_isr(uint gpio, uint32_t events)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(button_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void mpu_task(void *p)
{
    printf("[MPU] Task iniciada!\n");

    fall_state_t fs;
    fall_init(&fs);

    mpu6500_data_t data;
    TickType_t last_log = xTaskGetTickCount();

    for (;;)
    {
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(200)) == pdTRUE)
        {
            mpu6500_read_raw(I2C_PORT, &data);
            xSemaphoreGive(i2c_mutex);
        }
        else {
            printf("[ERRO] Mutex I2C ocupado (MPU)\n");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        int accel_raw[3] = { data.accel[0], data.accel[1], data.accel[2] };
        int gyro_raw[3]  = { data.gyro[0],  data.gyro[1],  data.gyro[2] };

        float ax = accel_raw[0] / 16384.0f;
        float ay = accel_raw[1] / 16384.0f;
        float az = accel_raw[2] / 16384.0f;

        float gx = gyro_raw[0] / 131.0f;
        float gy = gyro_raw[1] / 131.0f;
        float gz = gyro_raw[2] / 131.0f;

        fall_update(&fs, accel_raw, gyro_raw);

        if (proximity_alert)
        {
            fs.fall = true;
            proximity_alert = false;
            printf("[MPU] QUEDA MARCADA POR PROXIMIDADE!\n");
        }

        if (xSemaphoreTake(button_sem, 0) == pdTRUE)
        {
            fs.fall = true;
            printf("[BUTTON] QUEDA MARCADA POR BOTÃO!\n");
        }

        if (fs.fall)
        {
            fall_detected = true;
            fs.fall = false;
        }

        TickType_t now = xTaskGetTickCount();
        if (now - last_log >= pdMS_TO_TICKS(LOG_INTERVAL_MS))
        {
            last_log = now;
            printf("[MPU] AX=%.2f AY=%.2f AZ=%.2f | GX=%.2f GY=%.2f GZ=%.2f\n",
                   ax, ay, az, gx, gy, gz);
        }

        /* ---- ATUALIZAÇÃO DO OLED (UNIFICADA) ---- */
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(200)) == pdTRUE)
        {
            ssd1306_Fill(Black);
            char buf[32];

            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("Accel(g):", Font_6x8, White);
            sprintf(buf, "%.2f %.2f %.2f", ax, ay, az);
            ssd1306_SetCursor(0, 10);
            ssd1306_WriteString(buf, Font_6x8, White);

            ssd1306_SetCursor(0, 25);
            ssd1306_WriteString("Gyro(d/s):", Font_6x8, White);
            sprintf(buf, "%.2f %.2f %.2f", gx, gy, gz);
            ssd1306_SetCursor(0, 35);
            ssd1306_WriteString(buf, Font_6x8, White);

            sprintf(buf, "D:%umm", vl53_last_range);
            ssd1306_SetCursor(0, 45);
            ssd1306_WriteString(buf, Font_6x8, White);

            if (fall_detected)
            {
                ssd1306_SetCursor(70, 45);
                ssd1306_WriteString("QUEDA!", Font_7x10, White);
            }

            ssd1306_UpdateScreen();
            xSemaphoreGive(i2c_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

void vl53_task(void *p)
{
    printf("[VL53] VL53 task iniciada\n");
    vTaskDelay(pdMS_TO_TICKS(300));

    bool vl53_ok = false;
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500)) == pdTRUE)
    {
        printf("[VL53] Inicializando sensor...\n");
        vl53_ok = vl53l0x_init(I2C_PORT);

        if (vl53_ok)
        {
            vl53l0x_start_continuous(I2C_PORT);
            printf("[VL53] OK!\n");
        }

        xSemaphoreGive(i2c_mutex);
    }

    uint16_t range_mm = 0;
    uint8_t consecutive_close = 0;

    // ---- ANTI-FALSO POSITIVO: AGUARDA SENSOR ESTABILIZAR ----
    TickType_t start_time = xTaskGetTickCount();
    const TickType_t warmup_time = pdMS_TO_TICKS(1000); // 1 segundo

    for (;;)
    {
        if (!vl53_ok)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Se SIM estiver ativo, desativa proximidade
        if (gpio_get(SIM_PIN) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(200)) == pdTRUE)
        {
            bool ok = vl53l0x_read_range_mm(I2C_PORT, &range_mm, 200);
            xSemaphoreGive(i2c_mutex);

            if (!ok)
            {
                printf("[VL53] Falha leitura\n");
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }

            vl53_last_range = range_mm;

            // ---- IGNORAR LEITURAS NOS PRIMEIROS 1000ms ----
            if (xTaskGetTickCount() - start_time < warmup_time)
            {
                // Zeramos o contador pq no início o sensor dá 20~30mm
                consecutive_close = 0;
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }

            // ---- FILTRAR LEITURAS IMPOSSÍVEIS (<50mm) ----
            if (range_mm < 50)
            {
                // Ruído comum do VL53 logo após boot
                // Não contar como proximidade
                consecutive_close = 0;
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }

            // ---- LÓGICA DE PROXIMIDADE ----
            if (range_mm <= PROXIMITY_THRESHOLD_MM)
            {
                consecutive_close++;

                if (consecutive_close >= PROXIMITY_CONFIRM_COUNT)
                {
                    proximity_alert = true;
                    printf("[VL53] ALERTA PROXIMIDADE! (%u mm)\n", range_mm);
                    consecutive_close = 0;
                }
            }
            else
            {
                consecutive_close = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void led_task(void *p)
{
    const uint32_t LED_DELAY_NORMAL = 300;
    const uint32_t LED_DELAY_FALL = 500;

    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);
    gpio_put(BUZZER, 0);

    leds_init();  

    while (1)
    {
        if (fall_detected)
        {
            leds_off_all();
            led_on(LED_VERMELHO);
            gpio_put(BUZZER, 1);  
            vTaskDelay(pdMS_TO_TICKS(LED_DELAY_FALL));
            gpio_put(BUZZER, 0);  
        }
        else if (gpio_get(SIM_PIN) == 0)
        {
            leds_off_all();
            led_on(LED_VERDE);
            gpio_put(BUZZER, 0);  
            vTaskDelay(pdMS_TO_TICKS(LED_DELAY_FALL));
        }
        else
        {
            leds_off_all();
            led_on(LED_AZUL);
            gpio_put(BUZZER, 0);  
            vTaskDelay(pdMS_TO_TICKS(LED_DELAY_NORMAL));
            leds_off_all();
            vTaskDelay(pdMS_TO_TICKS(LED_DELAY_NORMAL));
        }
    }
}

int main()
{
    stdio_init_all();
    printf("\n\n===== BOOT =====\n");

    i2c_init(I2C_PORT, MPU_I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    gpio_init(SIM_PIN);
    gpio_set_dir(SIM_PIN, GPIO_IN);
    gpio_pull_up(SIM_PIN);

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    i2c_mutex = xSemaphoreCreateMutex();
    if (!i2c_mutex) { while (1); }

    button_sem = xSemaphoreCreateBinary();
    if (!button_sem) { while(1); }

    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &button_isr);

    // Init MPU + OLED
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500)) == pdTRUE)
    {
        mpu6500_init(I2C_PORT);
        ssd1306_Init();
        ssd1306_Fill(Black);
        ssd1306_SetCursor(0,0);
        ssd1306_WriteString("Sistema Iniciado", Font_6x8, White);
        ssd1306_UpdateScreen();
        xSemaphoreGive(i2c_mutex);
    }

    xTaskCreate(mpu_task,  "MPU",  2048, NULL, 2, NULL);
    xTaskCreate(vl53_task, "VL53", 1024, NULL, 2, NULL);
    xTaskCreate(sim_task,  "SIM",  2048, NULL, 2, NULL);
    xTaskCreate(led_task,  "LED",  256,  NULL, 1, NULL);
    xTaskCreate(mqtt_task, "MQTT", 4096, NULL, 2, NULL);

    vTaskStartScheduler();

    while (1);
}
