#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "lwip/apps/mqtt.h"

// =================================================================================
// --- CONFIGURAÇÕES - !! ATENÇÃO: ALTERE ESTAS INFORMAÇÕES !! ---
// =================================================================================

// --- DADOS DA SUA REDE WI-FI ---
#define WIFI_SSID       "NOME_DA_SUA_REDE_WIFI"
#define WIFI_PASSWORD   "SENHA_DA_SUA_REDE_WIFI"

// --- DADOS DO SEU BROKER LOCAL ---
// !! SUBSTITUA PELO IP REAL DO SEU NOTEBOOK !!
#define MQTT_SERVER_IP  "192.168.xxx.xxx" 
#define MQTT_CLIENT_ID  "BitDoguinho_RP2040"

// --- CREDENCIAIS DO BROKER SE FOI CONFIGURADO COM USUARIO E SENHA ---
#define MQTT_USER       "user02"
#define MQTT_PASSWORD   "147258369"

// --- TÓPICOS MQTT ---
#define MQTT_TOPIC_PUB_TEMP "Temp"
#define MQTT_TOPIC_SUB_LED  "Led"

// --- INTERVALO DE PUBLICAÇÃO ---
#define publication_interval 5000

// --- PINOS DO LED RGB ---
const uint RED_PIN   = 13;
const uint GREEN_PIN = 11;
const uint BLUE_PIN  = 12;

// =================================================================================
// --- LÓGICA DA APLICAÇÃO ---
// =================================================================================

// Função para converter leitura do ADC para Celsius
float adc_to_celsius(uint16_t raw_adc) {
    const float conversion_factor = 3.3f / 4095;
    float voltage = raw_adc * conversion_factor;
    return 27 - (voltage - 0.706f) / 0.001721f;
}

// Função para atualizar os estados dos LEDs
void set_led_color(bool R, bool G, bool B) {
    gpio_put(RED_PIN, R);
    gpio_put(GREEN_PIN, G);
    gpio_put(BLUE_PIN, B);
}

// =================================================================================
// --- LÓGICA MQTT ---
// =================================================================================

typedef struct MQTT_CLIENT_T_ {
    ip_addr_t remote_addr;
    mqtt_client_t *mqtt_client;
} MQTT_CLIENT_T;

// Callback chamado quando uma mensagem é recebida no tópico "Led"
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    char payload[len + 1];
    memcpy(payload, data, len);
    payload[len] = '\0';

    printf("Comando recebido no tópico 'Led': %s\n", payload);

    if (strcmp(payload, "11") == 0) {
        printf("Acendendo LED VERDE\n");
        set_led_color(false, true, false);
    } else if (strcmp(payload, "12") == 0) {
        printf("Acendendo LED AZUL\n");
        set_led_color(false, false, true);
    } else if (strcmp(payload, "13") == 0) {
        printf("Acendendo LED VERMELHO\n");
        set_led_color(true, false, false);
    } else {
        printf("Comando desconhecido. Apagando LEDs.\n");
        set_led_color(false, false, false);
    }
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
    // Apenas para log, não essencial para a lógica.
}

// Callback chamado no status da conexão com o broker
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT: Conectado ao broker com sucesso!\n");
        mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, arg);
        mqtt_subscribe(client, MQTT_TOPIC_SUB_LED, 1, NULL, NULL);
        printf("MQTT: Inscrito no tópico '%s'\n", MQTT_TOPIC_SUB_LED);
    } else {
        printf("MQTT: Falha na conexão, código: %d\n", status);
    }
}

// Função que publica a temperatura
void publish_temperature(MQTT_CLIENT_T* state) {
    adc_select_input(4);
    float temp = adc_to_celsius(adc_read());
    char payload[10];
    snprintf(payload, sizeof(payload), "%.2f", temp);
    printf("Publicando no tópico '%s': %s C\n", MQTT_TOPIC_PUB_TEMP, payload);
    mqtt_publish(state->mqtt_client, MQTT_TOPIC_PUB_TEMP, payload, strlen(payload), 1, 0, NULL, NULL);
}

// =================================================================================
// --- FUNÇÃO MAIN ---
// =================================================================================

int main() {
    stdio_init_all();
    adc_init();
    adc_set_temp_sensor_enabled(true);
    
    gpio_init(RED_PIN);
    gpio_init(GREEN_PIN);
    gpio_init(BLUE_PIN);
    gpio_set_dir(RED_PIN, GPIO_OUT);
    gpio_set_dir(GREEN_PIN, GPIO_OUT);
    gpio_set_dir(BLUE_PIN, GPIO_OUT);
    set_led_color(false, false, false);

    if (cyw43_arch_init()) {
        printf("Falha ao inicializar Wi-Fi\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();
    printf("Conectando a rede '%s'...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha ao conectar na rede.\n");
        return 1;
    }
    printf("Conectado com sucesso!\n");

    MQTT_CLIENT_T* state = calloc(1, sizeof(MQTT_CLIENT_T));
    state->mqtt_client = mqtt_client_new();
    
    struct mqtt_connect_client_info_t ci = {
        .client_id = MQTT_CLIENT_ID,
        .client_user = MQTT_USER,
        .client_pass = MQTT_PASSWORD, 
        .keep_alive = 60
    };

    ip_addr_t broker_ip;
    ipaddr_aton(MQTT_SERVER_IP, &broker_ip);
    mqtt_client_connect(state->mqtt_client, &broker_ip, 1883, mqtt_connection_cb, state, &ci);

    uint32_t last_pub_time = 0;

    while (true) {
        cyw43_arch_poll();
        if (mqtt_client_is_connected(state->mqtt_client)) {
            uint32_t now = to_ms_since_boot(get_absolute_time());
            if (now - last_pub_time > publication_interval) {
                publish_temperature(state);
                last_pub_time = now;
            }
        }
    }
    
    return 0;
}
