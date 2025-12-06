#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"         // <--- ESSENCIAL para SemaphoreHandle_t, xSemaphoreTake/Give
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "drivers/display/ssd1306.h"
#include "drivers/display/ssd1306_fonts.h"

#define SIM_PIN 5

extern SemaphoreHandle_t i2c_mutex;
extern volatile bool fall_detected_simul = false; 

void sim_task(void *p)
{
    printf("[SIM] Task de simulacao iniciada!\n");

    while (true)
    {
        if (gpio_get(SIM_PIN) == 0)
        {
            printf("\n=== SIMULACAO DE QUEDA ===\n");

            printf("[SIM] Trigger 1 enviado\n");
            vTaskDelay(pdMS_TO_TICKS(200));

            printf("[SIM] Trigger 2 enviado\n");
            vTaskDelay(pdMS_TO_TICKS(200));

            printf("[SIM] Trigger 3 enviado\n");
            vTaskDelay(pdMS_TO_TICKS(200));

            printf("[SIM] QUEDA SIMULADA ATIVADA!\n");
            fall_detected_simul = true;  

            printf("=== FIM DA SIMULACAO ===\n");

            if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(200)) == pdTRUE)
            {
                ssd1306_Fill(Black);
                ssd1306_SetCursor(0, 0);
                ssd1306_WriteString("SIMULACAO", Font_16x26, White);
                ssd1306_UpdateScreen();
                xSemaphoreGive(i2c_mutex);
            }

            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
