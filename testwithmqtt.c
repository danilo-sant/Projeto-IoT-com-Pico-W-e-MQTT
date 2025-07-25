#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "lwip/apps/mqtt.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h" // Necessário para o Mutex

// =================================================================================
// --- CONFIGURAÇÕES ---
// =================================================================================
#define WIFI_SSID "Laica-IoT"
#define WIFI_PASSWORD "Laica321"
#define MQTT_SERVER_IP "192.168.***.***"
#define MQTT_USER "*********"
#define MQTT_PASSWORD "**********"
#define MQTT_TOPIC_PUB_TEMP "ha/bitdog/temp"
#define MQTT_TOPIC_SUB_LED  "ha/bitdog/led/set"
#define MQTT_TOPIC_STATUS   "ha/bitdog/status"
#define PUBLISH_INTERVAL_MS 5000
#define RECONNECT_INTERVAL_MS 5000

const uint RED_PIN   = 13;
const uint GREEN_PIN = 11;
const uint BLUE_PIN  = 12;

// --- Estrutura de Estado MQTT ---
typedef struct MQTT_CLIENT_T_ {
    ip_addr_t remote_addr;
    mqtt_client_t *mqtt_client;
    char client_id[24]; 
} MQTT_CLIENT_T;

// --- Variáveis Globais para FreeRTOS ---
static MQTT_CLIENT_T state; // Tornada global para acesso das tarefas
static SemaphoreHandle_t mqtt_mutex; // Mutex para proteger o acesso à 'state'

// =================================================================================
// --- LÓGICA DA APLICAÇÃO (sem alterações) ---
// =================================================================================
float adc_to_celsius(uint16_t raw_adc) {
    const float conversion_factor = 3.3f / 4095;
    float voltage = raw_adc * conversion_factor;
    return 27 - (voltage - 0.706f) / 0.001721f;
}

void set_led_color(bool R, bool G, bool B) {
    gpio_put(RED_PIN, R);
    gpio_put(GREEN_PIN, G);
    gpio_put(BLUE_PIN, B);
}

// =================================================================================
// --- FUNÇÕES MQTT ---
// =================================================================================
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT: Successfully connected!\n");
        mqtt_publish(client, MQTT_TOPIC_STATUS, "online", strlen("online"), 1, 1, NULL, NULL);
        mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, arg);
        mqtt_subscribe(client, MQTT_TOPIC_SUB_LED, 1, NULL, NULL);
        printf("MQTT: Subscribed to '%s'\n", MQTT_TOPIC_SUB_LED);
    } else {
        printf("MQTT: Connection failed, code: %d\n", status);
    }
}

void mqtt_try_connect(MQTT_CLIENT_T *s) {
    printf("Attempting to connect to MQTT broker...\n");
    struct mqtt_connect_client_info_t ci = {
        .client_id = s->client_id, 
        .client_user = MQTT_USER,
        .client_pass = MQTT_PASSWORD, 
        .keep_alive = 60,
        .will_topic = MQTT_TOPIC_STATUS,
        .will_msg = (const u8_t*)"offline",
        .will_qos = 1,
        .will_retain = 1
    };
    err_t err = mqtt_client_connect(s->mqtt_client, &s->remote_addr, 1883, mqtt_connection_cb, s, &ci);
    if (err != ERR_OK) {
        printf("MQTT: Error starting connection: %d\n", err);
    }
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    char payload[len + 1]; memcpy(payload, data, len); payload[len] = '\0';
    printf("Command received on topic '%s': %s\n", MQTT_TOPIC_SUB_LED, payload);

    if (strcmp(payload, "11") == 0) { set_led_color(false, true, false); }
    else if (strcmp(payload, "12") == 0) { set_led_color(false, false, true); }
    else if (strcmp(payload, "13") == 0) { set_led_color(true, false, false); }
    else { set_led_color(false, false, false); }
}
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {}

void publish_temperature(MQTT_CLIENT_T* s) {
    adc_select_input(4);
    float temp = adc_to_celsius(adc_read());
    char payload[10]; snprintf(payload, sizeof(payload), "%.2f", temp);
    printf("Publishing to '%s': %s C\n", MQTT_TOPIC_PUB_TEMP, payload);
    mqtt_publish(s->mqtt_client, MQTT_TOPIC_PUB_TEMP, payload, strlen(payload), 1, 0, NULL, NULL);
}

// =================================================================================
// --- TAREFAS (TASKS) DO FREERTOS ---
// =================================================================================

// Tarefa para gerir a conexão Wi-Fi e MQTT
void mqtt_task(void *pvParameters) {
    uint32_t last_reconnect_attempt = 0;

    // Conecta ao Wi-Fi uma única vez
    cyw43_arch_enable_sta_mode();
    printf("Connecting to network '%s'...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("FATAL: Failed to connect to network.\n");
        // Se falhar, a tarefa para aqui.
        vTaskDelete(NULL); 
    }
    printf("Successfully connected to Wi-Fi!\n");
    cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);

    // Loop principal da tarefa de rede
    for (;;) {
        // Usa o Mutex para garantir acesso seguro à 'state'
        if (xSemaphoreTake(mqtt_mutex, (TickType_t)10) == pdTRUE) {
            if (!mqtt_client_is_connected(state.mqtt_client)) {
                uint32_t now = to_ms_since_boot(get_absolute_time());
                if (now - last_reconnect_attempt > RECONNECT_INTERVAL_MS) {
                    last_reconnect_attempt = now;
                    mqtt_try_connect(&state);
                }
            }
            xSemaphoreGive(mqtt_mutex);
        }

        // Esta função é o coração da rede. Ela deve ser chamada o mais rápido possível.
        cyw43_arch_poll();
        vTaskDelay(pdMS_TO_TICKS(1)); // Pequeno delay para ceder o processador
    }
}

// Tarefa para publicar a temperatura
void temp_publish_task(void *pvParameters) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));

        // Usa o Mutex para garantir acesso seguro à 'state'
        if (xSemaphoreTake(mqtt_mutex, (TickType_t)10) == pdTRUE) {
            if (mqtt_client_is_connected(state.mqtt_client)) {
                publish_temperature(&state);
            } else {
                printf("Temp Task: MQTT not connected, skipping publish.\n");
            }
            xSemaphoreGive(mqtt_mutex);
        }
    }
}

// =================================================================================
// --- FUNÇÃO MAIN ---
// =================================================================================
int main() {
    stdio_init_all();
    
    // Inicialização do hardware
    adc_init(); adc_set_temp_sensor_enabled(true);
    gpio_init(RED_PIN); gpio_init(GREEN_PIN); gpio_init(BLUE_PIN);
    gpio_set_dir(RED_PIN, GPIO_OUT); gpio_set_dir(GREEN_PIN, GPIO_OUT); gpio_set_dir(BLUE_PIN, GPIO_OUT);
    set_led_color(false, false, false);
    if (cyw43_arch_init()) { printf("FATAL: Failed to initialize Wi-Fi chip\n"); return 1; }

    // Inicialização do cliente MQTT (global)
    state.mqtt_client = mqtt_client_new();
    ipaddr_aton(MQTT_SERVER_IP, &state.remote_addr);
    char base_id[] = "BitDog_Danilo_";
    uint8_t mac[6]; cyw43_hal_get_mac(0, mac);
    snprintf(state.client_id, sizeof(state.client_id), "%s%02X%02X%02X", base_id, mac[3], mac[4], mac[5]);
    printf("Using unique Client ID: %s\n", state.client_id);
    
    // Cria o Mutex
    mqtt_mutex = xSemaphoreCreateMutex();
    if (mqtt_mutex == NULL) {
        printf("FATAL: Failed to create MQTT mutex\n");
        return 1;
    }

    // Cria as tarefas
    // A tarefa de rede tem prioridade mais alta
    xTaskCreate(mqtt_task, "MQTT_Task", 1024, NULL, tskIDLE_PRIORITY + 2, NULL);
    // A tarefa de aplicação tem prioridade normal
    xTaskCreate(temp_publish_task, "Temp_Publish_Task", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);

    // Inicia o escalonador do FreeRTOS
    // O código abaixo desta linha não será executado.
    vTaskStartScheduler();

    while(true){}; // Loop infinito de segurança
}
