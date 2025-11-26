#include "fall_detector.h"
#include <math.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "../mpu6050/mpu6050.h"

static float ax, ay, az, gx, gy, gz;
static int angleChange;

void fall_init(fall_state_t* fs) {
    fs->fall = false;
    fs->trigger1 = false;
    fs->trigger2 = false;
    fs->trigger3 = false;
    fs->trigger1count = 0;
    fs->trigger2count = 0;
    fs->trigger3count = 0;
}

void fall_update(fall_state_t* fs, int accel[3], int gyro[3]) {

    // NORMALIZAÇÃO (ajuste fino opcional)
    ax = accel[0] / 16384.0f;
    ay = accel[1] / 16384.0f;
    az = accel[2] / 16384.0f;

    gx = gyro[0] / 131.0f;
    gy = gyro[1] / 131.0f;
    gz = gyro[2] / 131.0f;

    // --- 1) Magnitude da aceleração ---
    float Raw_Amp = sqrt(ax*ax + ay*ay + az*az);
    int Amp = Raw_Amp * 10;

    // ---------- TRIGGER 1 (QUEDA LIVRE) ----------
    if (Amp <= 2 && !fs->trigger2) {
        fs->trigger1 = true;
        fs->trigger1count = 0;
        printf("TRIGGER 1 ATIVADO\n");
    }

    if (fs->trigger1) {
        fs->trigger1count++;
        if (Amp >= 12) {
            fs->trigger2 = true;
            fs->trigger1 = false;
            fs->trigger1count = 0;
            printf("TRIGGER 2 ATIVADO\n");
        }
    }

    // ---------- TRIGGER 2 (IMPACTO) ----------
    if (fs->trigger2) {
        fs->trigger2count++;

        angleChange = sqrt(gx*gx + gy*gy + gz*gz);

        if (angleChange >= 30 && angleChange <= 400) {
            fs->trigger3 = true;
            fs->trigger2 = false;
            fs->trigger2count = 0;
            printf("TRIGGER 3 ATIVADO\n");
        }
    }

    // ---------- TRIGGER 3 (IMÓVEL APÓS A QUEDA) ----------
    if (fs->trigger3) {
        fs->trigger3count++;

        if (fs->trigger3count >= 10) {

            angleChange = sqrt(gx*gx + gy*gy + gz*gz);

            if (angleChange <= 10) {
                printf("QUEDA DETECTADA!\n");
                fs->fall = true;

                // Se quiser enviar MQTT:
                // send_event("fall_detect");

            } else {
                printf("TRIGGER 3 CANCELADO\n");
            }

            // Reset final
            fs->trigger3 = false;
            fs->trigger3count = 0;
        }
    }

    // --- Resets automáticos ---
    if (fs->trigger2count >= 6) {
        fs->trigger2 = false;
        fs->trigger2count = 0;
        printf("TRIGGER 2 DESATIVADO\n");
    }
    if (fs->trigger1count >= 6) {
        fs->trigger1 = false;
        fs->trigger1count = 0;
        printf("TRIGGER 1 DESATIVADO\n");
    }
}
