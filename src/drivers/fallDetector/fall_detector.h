#ifndef FALL_DETECTOR_H
#define FALL_DETECTOR_H

#include <stdbool.h>

typedef struct {
    bool trigger1;
    bool trigger2;
    bool trigger3;
    bool fall;

    int trigger1count;
    int trigger2count;
    int trigger3count;

    bool simulating; // ← impede MPU e OLED durante simulação
} fall_state_t;

void fall_init(fall_state_t* fs);
void fall_update(fall_state_t* fs, int accel[3], int gyro[3]);
void fall_simulate(fall_state_t* fs);

#endif
