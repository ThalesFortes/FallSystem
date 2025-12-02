#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/altcp_tls.h"

#include "FreeRTOS.h"
#include "task.h"

// Variáveis globais vindas do main.c
extern volatile bool fall_detected;
extern volatile bool proximity_alert;
extern volatile uint16_t vl53_last_range;
#define SIM_PIN 5

// ---- MQTT CONFIG (DEFINIDO NO CMAKE) ----
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

static mqtt_state_t mqtt_state;


// --------------------------- CALLBACKS MQTT ---------------------------
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


// --------------------------- INICIAR MQTT ---------------------------
static void mqtt_start()
{
    struct mqtt_connect_client_info_t info = {0};

    char client_id[20];
    pico_get_unique_board_id_string(client_id, sizeof(client_id));

    info.client_id = client_id;
    info.keep_alive = 60;
    info.client_user = MQTT_USERNAME;
    info.client_pass = MQTT_PASSWORD;

    mqtt_state.mqtt_client = mqtt_client_new();
    if (!mqtt_state.mqtt_client) {
        printf("[MQTT] Erro ao criar cliente\n");
        return;
    }

    err_t err = mqtt_client_connect(
        mqtt_state.mqtt_client,
        &mqtt_state.remote_addr,
        MQTT_PORT,
        mqtt_connection_cb,
        NULL,
        &info
    );

    if (err != ERR_OK) {
        printf("[MQTT] Erro mqtt_client_connect (%d)\n", err);
    }
}


// --------------------------- MAPEAMENTO LED E BUZZER ---------------------------
static const char* get_led_state()
{
    if (fall_detected)
        return "vermelho";

    if (gpio_get(SIM_PIN) == 0)
        return "verde";

    return "azul";
}

static bool is_buzzer_on()
{
    return fall_detected ? true : false;
}


// --------------------------- ENVIAR MENSAGEM MQTT ---------------------------
static void mqtt_send_event()
{
    const char *led = get_led_state();
    bool buzzer = is_buzzer_on();

    char motivo[20] = "nenhum";

    if (fall_detected && proximity_alert)
        sprintf(motivo, "proximidade");
    else if (fall_detected && vl53_last_range <= 200)
        sprintf(motivo, "proximidade");
    else if (fall_detected)
        sprintf(motivo, "mpu");

    char payload[200];
    snprintf(payload, sizeof(payload),
        "{ \"queda\": %s, \"motivo\": \"%s\", \"led\": \"%s\", \"buzzer\": %s, \"dist_mm\": %u }",
        fall_detected ? "true" : "false",
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
        0,      // QoS
        0,      // no retain
        mqtt_pub_cb,
        NULL
    );
    cyw43_arch_lwip_end();

    printf("[MQTT] EVENTO ENVIADO: %s\n", payload);
}


// --------------------------- TAREFA MQTT ---------------------------
void mqtt_task(void *p)
{
    printf("[MQTT] Task iniciando...\n");

    // ---- WiFi ----
    if (cyw43_arch_init()) {
        printf("[MQTT] Falha ao iniciar WiFi\n");
        vTaskDelete(NULL);
    }
    cyw43_arch_enable_sta_mode();

    printf("[MQTT] Conectando ao WiFi: %s\n", WIFI_SSID);

    if (cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID, WIFI_PASSWORD,
            CYW43_AUTH_WPA2_AES_PSK,
            30000)) {

        printf("[MQTT] Falha conexão WiFi\n");
        vTaskDelete(NULL);
    }

    printf("[MQTT] WiFi conectado!\n");

    if (!ip4addr_aton(MQTT_SERVER, &mqtt_state.remote_addr)) {
        printf("[MQTT] Erro ao converter IP do broker!\n");
        vTaskDelete(NULL);
    }

    mqtt_start();

    bool last_fall_state = false;

    while (1)
    {
        if (!mqtt_state.connected) {
            vTaskDelay(pdMS_TO_TICKS(3000));
            mqtt_start();
            continue;
        }

        // SOMENTE ENVIA AO HAVER ALTERAÇÃO NO ESTADO
        if (fall_detected != last_fall_state || proximity_alert) {
            mqtt_send_event();
            last_fall_state = fall_detected;
            proximity_alert = false;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
