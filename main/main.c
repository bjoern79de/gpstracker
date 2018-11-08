#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "common.h"
#include "gps.h"
#include "lora.h"

#define TAG "main"

#define RECEIVER false

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}

static void gps_task(void *pvParameter)
{
    ESP_LOGI(TAG, "gps task started");
    if (gps_config() < 0)
    {
        ESP_LOGE(TAG, "error configuring gps");
        while(1)
        {
            delayMs(100);
        }
    }
    while(1)
    {
        gps_stateMachine();
    }
}

typedef struct {
	uint8_t fixType;
	uint8_t	numSats;
	int32_t lon;
	int32_t lat;
    int32_t	groundSpeed;
    int32_t height;
} packet_t;

void app_main(void)
{
    nvs_flash_init();
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    wifi_config_t sta_config =
    {
        .sta = {
            .ssid = "WLAN-0024FEACF701",
            .password = "5irvalu5e",
            .bssid_set = false
        }
    };
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );

    lora_init();
    lora_set_frequency(868e6);
    lora_enable_crc();

    if (!RECEIVER) {
        xTaskCreatePinnedToCore(&gps_task, "gpstask", 2048, NULL, 20, NULL, 0);

        while (true) {
            printf("sats: %i, fix: %i, lat: %d, lon: %d\n", NavPvt.nav.numSV, NavPvt.nav.fixType, NavPvt.nav.latDeg7, NavPvt.nav.lonDeg7);
            packet_t packet = {
                .fixType     = NavPvt.nav.fixType,
                .numSats     = NavPvt.nav.numSV,
                .lon         = NavPvt.nav.lonDeg7,
                .lat         = NavPvt.nav.latDeg7,
                .groundSpeed = NavPvt.nav.groundSpeedmmps
            };
            lora_send_packet((uint8_t*)&packet, sizeof(packet));
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    } else {

        // receive loop
        packet_t * packet = (packet_t*)malloc(sizeof(packet_t));
        while (true) {
            lora_receive();    // put into receive mode
            while (lora_received()) {
                int size = lora_receive_packet((uint8_t*)packet, sizeof(packet_t));
                printf("sats: %i, fix: %i, lat: %d, lon: %d\n", packet->numSats, packet->fixType, packet->lat, packet->lon);
                lora_receive();
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

}