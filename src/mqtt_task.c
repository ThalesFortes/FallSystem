#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/altcp_tls.h"

#include "FreeRTOS.h"
#include "task.h"

#include "drivers/leds/leds.h"

#define BUZZER 21


extern volatile bool fall_detected;
extern volatile bool proximity_alert;
extern volatile uint16_t vl53_last_range;
extern volatile bool fall_detected_simul;

#define SIM_PIN 5
#define FALL_PROXIMITY_MM 120

#define FALL_CLEAR_MS 5000

#define NORMAL_REPORT_MS 5000   

#ifndef MQTT_SERVER
#error "MQTT_SERVER não definido"
#endif
#ifndef WIFI_SSID
#error "WIFI_SSID não definido"
#endif
#ifndef WIFI_PASSWORD
#error "WIFI_PASSWORD não definido"
#endif
#ifndef MQTT_USERNAME
#error "MQTT_USERNAME não definido"
#endif
#ifndef MQTT_PASSWORD
#error "MQTT_PASSWORD não definido"
#endif

#define MQTT_TOPIC "pico/fall"

// Estado MQTT
typedef struct {
    ip_addr_t remote_addr;
    mqtt_client_t *mqtt_client;
    bool connected;
} mqtt_state_t;

static mqtt_state_t mqtt_state = {0};

typedef enum {
    FALL_CAUSE_NONE = 0,
    FALL_CAUSE_PROXIMITY,
    FALL_CAUSE_MPU
} fall_cause_t;

static void led_apply_state()
{
    if (fall_detected)
    {
        leds_off_all();
        led_on(LED_VERMELHO);
        gpio_put(BUZZER, 1);
        return;
    }

    if (proximity_alert)
    {
        leds_off_all();
        gpio_put(BUZZER, 1);
        return;
    }

    if (fall_detected_simul)
    {
        leds_off_all();
        led_on(LED_VERDE);
        gpio_put(BUZZER, 0);
        return;
    }

    if (gpio_get(SIM_PIN) == 0)
    {
        leds_off_all();
        led_on(LED_VERDE);
        gpio_put(BUZZER, 0);
        return;
    }

    leds_off_all();
    led_on(LED_AZUL);
    gpio_put(BUZZER, 0);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("[MQTT] Conectado ao broker!\n");
        mqtt_state.connected = true;
    } else {
        printf("[MQTT] Falha conexão (%d)\n", status);
        mqtt_state.connected = false;
    }
}

static void mqtt_pub_cb(void *arg, err_t err)
{
    if (err != ERR_OK) {
        printf("[MQTT] Falha ao publicar (%d)\n", err);
    }
}


static void mqtt_start()
{
    struct mqtt_connect_client_info_t info = {0};

    char client_id[32];
    pico_get_unique_board_id_string(client_id, sizeof(client_id));

    info.client_id  = client_id;
    info.keep_alive = 60;
    info.client_user = MQTT_USERNAME;
    info.client_pass = MQTT_PASSWORD;

    if (mqtt_state.mqtt_client == NULL) {
        mqtt_state.mqtt_client = mqtt_client_new();
        if (!mqtt_state.mqtt_client) {
            printf("[MQTT] Erro ao criar cliente\n");
            return;
        }
    }

    cyw43_arch_lwip_begin();

    err_t err = mqtt_client_connect(
        mqtt_state.mqtt_client,
        &mqtt_state.remote_addr,
        MQTT_PORT,
        mqtt_connection_cb,
        NULL,
        &info
    );

    cyw43_arch_lwip_end();

    if (err != ERR_OK) {
        printf("[MQTT] Erro mqtt_client_connect (%d)\n", err);
    }
}


static fall_cause_t compute_fall_cause(void)
{
    if (proximity_alert)
        return FALL_CAUSE_PROXIMITY;

    if (vl53_last_range != 0 && vl53_last_range <= FALL_PROXIMITY_MM)
        return FALL_CAUSE_PROXIMITY;

    if (fall_detected)
        return FALL_CAUSE_MPU;

    return FALL_CAUSE_NONE;
}

static const char* get_led_state()
{
    if (fall_detected)
        return "vermelho";

    if (fall_detected_simul)
        return "verde";

    return "azul";
}

static bool is_buzzer_on()
{
    return fall_detected || proximity_alert ? true : false;
}


static void mqtt_send_event_normal_with_cause(fall_cause_t cause)
{
    const char *led = get_led_state();
    bool buzzer = is_buzzer_on();

    char motivo[32] = "nenhum";

    if (cause == FALL_CAUSE_PROXIMITY)
        snprintf(motivo, sizeof(motivo), "proximidade");
    else if (cause == FALL_CAUSE_MPU)
        snprintf(motivo, sizeof(motivo), "mpu");

    char payload[256];
    snprintf(payload, sizeof(payload),
        "{ \"queda\": %s, \"motivo\": \"%s\", \"led\": \"%s\", \"buzzer\": %s, \"dist_mm\": %u }",
        (cause != FALL_CAUSE_NONE) ? "true" : "false",
        motivo,
        led,
        buzzer ? "true" : "false",
        vl53_last_range
    );

    cyw43_arch_lwip_begin();
    mqtt_publish(
        mqtt_state.mqtt_client,
        MQTT_TOPIC,
        payload,
        strlen(payload),
        0,
        0,
        mqtt_pub_cb,
        NULL
    );
    cyw43_arch_lwip_end();

    printf("[MQTT] EVENTO ENVIADO: %s\n", payload);
}


static void mqtt_send_event_sim()
{
    const char *led = get_led_state();

    const char *triggers =
        "[\"Trigger 1 enviado\", \"Trigger 2 enviado\", \"Trigger 3 enviado\"]";

    char payload[350];
    snprintf(payload, sizeof(payload),
        "{ "
        "\"queda\": true, "
        "\"motivo\": \"simulacao\", "
        "\"led\": \"%s\", "
        "\"buzzer\": true, "
        "\"dist_mm\": %u, "
        "\"triggers\": %s "
        "}",
        led,
        vl53_last_range,
        triggers
    );

    cyw43_arch_lwip_begin();
    mqtt_publish(
        mqtt_state.mqtt_client,
        MQTT_TOPIC,
        payload,
        strlen(payload),
        0,
        0,
        mqtt_pub_cb,
        NULL
    );
    cyw43_arch_lwip_end();

    printf("[MQTT] EVENTO DE SIMULACAO ENVIADO: %s\n", payload);
}


void mqtt_task(void *p)
{
    printf("[MQTT] Task iniciando...\n");

    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);

    leds_init();
    leds_off_all();

    if (cyw43_arch_init()) {
        printf("[MQTT] Falha ao iniciar WiFi\n");
        vTaskDelete(NULL);
    }
    cyw43_arch_enable_sta_mode();

    printf("[MQTT] SSID = %s\n", WIFI_SSID);
    printf("[MQTT] Broker = %s\n", MQTT_SERVER);

wifi_reconnect:
    printf("[MQTT] Conectando WiFi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID, WIFI_PASSWORD,
            CYW43_AUTH_WPA2_AES_PSK,
            30000)) {

        printf("[MQTT] Falha WiFi → tentando novamente...\n");
        vTaskDelay(pdMS_TO_TICKS(3000));
        goto wifi_reconnect;
    }

    printf("[MQTT] WiFi conectado!\n");

    if (!ip4addr_aton(MQTT_SERVER, &mqtt_state.remote_addr)) {
        printf("[MQTT] Erro ao converter IP do broker!\n");
        vTaskDelete(NULL);
    }

mqtt_reconnect:
    mqtt_start();

    bool last_fall_state = false;
    bool sim_sent = false;
    static uint32_t last_wifi_check = 0;

    TickType_t fall_start_tick = 0;
    bool fall_timer_running = false;

    TickType_t last_normal_report = 0;  

    while (true)
    {
        cyw43_arch_poll();
        vTaskDelay(pdMS_TO_TICKS(5));

        led_apply_state();

        if (xTaskGetTickCount() - last_wifi_check > pdMS_TO_TICKS(3000)) {

            last_wifi_check = xTaskGetTickCount();
            int st = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);

            if (st < 0) {
                printf("[WiFi] Conexão perdida (%d)! Reconectando...\n", st);
                mqtt_state.connected = false;
                goto wifi_reconnect;
            }
        }

        if (!mqtt_state.connected) {
            printf("[MQTT] Desconectado → tentando reconectar...\n");
            vTaskDelay(pdMS_TO_TICKS(2000));
            goto mqtt_reconnect;
        }

        if (fall_detected_simul && !sim_sent) {

            mqtt_send_event_sim();

            sim_sent = true;
            fall_detected_simul = false;
        }

        fall_cause_t current_cause = compute_fall_cause();

        bool is_currently_fall = (current_cause != FALL_CAUSE_NONE);

        if (is_currently_fall)
            sim_sent = false;

        if (current_cause == FALL_CAUSE_MPU && !fall_timer_running) {
            fall_start_tick = xTaskGetTickCount();
            fall_timer_running = true;
            printf("[MQTT] Timer queda MPU iniciado\n");
        }

        if (!is_currently_fall && fall_timer_running) {
            fall_timer_running = false;
            printf("[MQTT] Timer queda MPU cancelado\n");
        }

        if (fall_timer_running) {
            if (xTaskGetTickCount() - fall_start_tick >= pdMS_TO_TICKS(FALL_CLEAR_MS)) {

                fall_detected = false;
                fall_timer_running = false;
                printf("[MQTT] Tempo queda MPU expirou → limpando estado\n");

                mqtt_send_event_normal_with_cause(FALL_CAUSE_NONE);
                last_fall_state = false;
                proximity_alert = false;
            }
        }

        if (!is_currently_fall &&
            (xTaskGetTickCount() - last_normal_report > pdMS_TO_TICKS(NORMAL_REPORT_MS)))
        {
            mqtt_send_event_normal_with_cause(FALL_CAUSE_NONE);
            last_normal_report = xTaskGetTickCount();
        }

        if ( (fall_detected != last_fall_state) ||
             proximity_alert ||
             (current_cause == FALL_CAUSE_MPU && fall_detected) )
        {
            mqtt_send_event_normal_with_cause(current_cause);

            last_fall_state = fall_detected;
            proximity_alert = false;
        }

    }
}
