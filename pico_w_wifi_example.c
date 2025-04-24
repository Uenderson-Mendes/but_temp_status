#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>
#include "hardware/adc.h"

// Function prototypes
void monitor_buttons();

// Definições
#define BUTTON1_PIN 5
#define BUTTON2_PIN 6
#define WIFI_SSID "Gustavo"
#define WIFI_PASS "@cetech17122020k"

// Estado dos botões
char button1_message[50] = "Nenhum evento no botão 1";
char button2_message[50] = "Nenhum evento no botão 2";
char temperature_message[50] = "Temperatura: N/A";

char http_response[1024];

// Função para ler temperatura do sensor interno
float read_temperature_celsius() {
    adc_select_input(4);  // Canal 4 = sensor interno
    uint16_t raw = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float voltage = raw * conversion_factor;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

// Função para criar resposta HTTP
void create_http_response() {
    snprintf(http_response, sizeof(http_response),
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "  <meta charset=\"UTF-8\">"
            "  <title>Monitoramento de Temperatura e Botões</title>"
            "</head>"
            "<body>"
            "  <h1>Monitoramento de Temperatura e Botões</h1>"
            "  <p><a href=\"/update\">Atualizar Estado</a></p>"
            "  <h2>Estado dos Botões:</h2>"
            "  <p>Botão 1: %s</p>"
            "  <p>Botão 2: %s</p>"
            "  <h2>Temperatura:</h2>"
            "  <p>Temperatura atual: %s</p>"
            "</body>"
            "</html>\r\n",
             button1_message, button2_message, temperature_message);
}

// Callback HTTP
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *request = (char *)p->payload;

    // Atualiza leitura dos sensores
    monitor_buttons();
    float temp = read_temperature_celsius();
    snprintf(temperature_message, sizeof(temperature_message), "Temperatura: %.2f °C", temp);

    // Cria resposta e envia
    create_http_response();
    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);
    pbuf_free(p);

    return ERR_OK;
}

// Callback de conexão
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);
    return ERR_OK;
}

// Inicia o servidor HTTP
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) return;

    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) return;

    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);

    printf("Servidor HTTP rodando na porta 80...\n");
}

// Monitoramento dos botões
void monitor_buttons() {
    static bool button1_last = false, button2_last = false;
    bool b1 = !gpio_get(BUTTON1_PIN);
    bool b2 = !gpio_get(BUTTON2_PIN);

    if (b1 != button1_last) {
        button1_last = b1;
        snprintf(button1_message, sizeof(button1_message), b1 ? "Botão 1 pressionado!" : "Botão 1 solto!");
        printf("%s\n", button1_message);
    }

    if (b2 != button2_last) {
        button2_last = b2;
        snprintf(button2_message, sizeof(button2_message), b2 ? "Botão 2 pressionado!" : "Botão 2 solto!");
        printf("%s\n", button2_message);
    }
}

int main() {
    stdio_init_all();
    sleep_ms(10000);
    printf("Iniciando...\n");

    if (cyw43_arch_init()) {
        printf("Erro no Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Erro ao conectar no Wi-Fi\n");
        return 1;
    }

    printf("Conectado.\n");
    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    printf("IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);


    gpio_init(BUTTON1_PIN);
    gpio_set_dir(BUTTON1_PIN, GPIO_IN);
    gpio_pull_up(BUTTON1_PIN);

    gpio_init(BUTTON2_PIN);
    gpio_set_dir(BUTTON2_PIN, GPIO_IN);
    gpio_pull_up(BUTTON2_PIN);

    // Inicializa ADC para temperatura
    adc_init();
    adc_set_temp_sensor_enabled(true);

    // Inicia servidor
    start_http_server();

    while (true) {
        monitor_buttons();
        sleep_ms(1000);  // Atualiza os estados a cada 1 segundo
    }

    return 0;
}
