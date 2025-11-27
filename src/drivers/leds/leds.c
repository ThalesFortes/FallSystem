#include "pico/stdlib.h"
#include "leds.h"

void leds_init(void) {
    gpio_init(LED_VERMELHO); gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_init(LED_VERDE);    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_init(LED_AZUL);     gpio_set_dir(LED_AZUL, GPIO_OUT);
    leds_off_all();
}

void led_on(led_t led) {
    gpio_put(led, 1);
}

void led_off(led_t led) {
    gpio_put(led, 0);
}

void leds_off_all(void) {
    gpio_put(LED_VERMELHO, 0);
    gpio_put(LED_VERDE, 0);
    gpio_put(LED_AZUL, 0);
}