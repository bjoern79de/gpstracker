#include "driver/gpio.h"
#include "common.h"
#include "gps.h"

extern "C" {
    #include "lora.h"
    //#include "ota_server.h"
}

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define TAG "main"

#define RECEIVER true

#define WIFI_SSID             "WLAN-0024FEACF701"
#define WIFI_PASSWORD         "5irvalu5e"

static EventGroupHandle_t wifi_event_group;

static const int CONNECTED_BIT = BIT0;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        //Serial.println("*********");
        //Serial.print("Received Value: ");
        //for (int i = 0; i < rxValue.length(); i++)
          //Serial.print(rxValue[i]);

        //Serial.println();
        //Serial.println("*********");
      }
    }
};

void init_ble() {
  // Create the BLE Device
  BLEDevice::init("UART Service");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
										CHARACTERISTIC_UUID_TX,
										BLECharacteristic::PROPERTY_NOTIFY
									);
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_RX,
											BLECharacteristic::PROPERTY_WRITE
										);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  //Serial.println("Waiting a client connection to notify...");
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    wifi_config_t sta_config;

    strcpy((char*)sta_config.sta.ssid, WIFI_SSID);
    strcpy((char*)sta_config.sta.password, WIFI_PASSWORD);
    sta_config.sta.bssid_set = false;

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

/*
static void ota_server_task(void * param)
{
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    ota_server_start();
    vTaskDelete(NULL);
}
*/

static void gps_task(void *pvParameter)
{
    ESP_LOGI(TAG, "gps task started");
    if (gps_config() < 0)
    {
        ESP_LOGE(TAG, "error configuring gps");
        while(1)
        {
            //delayMs(100);
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

extern "C" {
	void app_main();
}

void app_main(void)
{
    nvs_flash_init();

    initialise_wifi();
    //xTaskCreate(&ota_server_task, "ota_server_task", 4096, NULL, 5, NULL);

	init_ble();

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
                .groundSpeed = NavPvt.nav.groundSpeedmmps,
                .height      = NavPvt.nav.heightMSLmm
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
                printf("packetSize: %i, sats: %i, fix: %i, lat: %d, lon: %d\n", size, packet->numSats, packet->fixType, packet->lat, packet->lon);
                lora_receive();
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

}
