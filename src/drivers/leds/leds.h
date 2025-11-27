#ifndef LEDS_H
#define LEDS_H

#include <stdint.h>


typedef enum {
    LED_VERMELHO = 13,
    LED_VERDE    = 11,
    LED_AZUL     = 12
} led_t;


void leds_init(void);

void led_on(led_t led);

void led_off(led_t led);

void leds_off_all(void);

#endif