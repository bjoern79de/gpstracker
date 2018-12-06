#include "driver/gpio.h"
#include "common.h"
#include "gps.h"

extern "C" {
    #include "lora.h"
    #include "ota_server.h"
}

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer *pServer = NULL;

BLECharacteristic * pFixCharacteristic;
BLECharacteristic * pNumSatCharacteristic;
BLECharacteristic * pLatCharacteristic;
BLECharacteristic * pLonCharacteristic;
BLECharacteristic * pSpeedCharacteristic;
BLECharacteristic * pHeightCharacteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;

#define SERVICE_UUID               "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_FIX    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_NSAT   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_LAT    "6E400004-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_LON    "6E400005-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_SPEED  "6E400006-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_HEIGHT "6E400007-B5A3-F393-E0A9-E50E24DCCA9E"

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

void init_ble() {
  // Create the BLE Device
  BLEDevice::init("UART Service");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pFixCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_FIX,
                            BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
                        );
  pNumSatCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_NSAT,
                            BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
                        );
  pLatCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_LAT,
                            BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
                        );
  pLonCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_LON,
                            BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
                        );
  pSpeedCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_SPEED,
                            BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
                        );
  pHeightCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_HEIGHT,
                            BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
                        );
                      
  pFixCharacteristic->addDescriptor(new BLE2902());
  pNumSatCharacteristic->addDescriptor(new BLE2902());
  pLatCharacteristic->addDescriptor(new BLE2902());
  pLonCharacteristic->addDescriptor(new BLE2902());
  pSpeedCharacteristic->addDescriptor(new BLE2902());
  pHeightCharacteristic->addDescriptor(new BLE2902());

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
    memset(&sta_config, 0, sizeof(sta_config));
    strcpy((char*)sta_config.sta.ssid, WIFI_SSID);
    strcpy((char*)sta_config.sta.password, WIFI_PASSWORD);
    sta_config.sta.bssid_set = false;

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void ota_server_task(void * param)
{
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    ota_server_start();
    vTaskDelete(NULL);
}

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
    xTaskCreate(&ota_server_task, "ota_server_task", 4096, NULL, 5, NULL);

	init_ble();

    lora_init();
    lora_set_frequency(868e6);
    lora_set_tx_power(17);
    
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

                if (deviceConnected) {
                    pFixCharacteristic->setValue((uint8_t*)&packet->fixType, 1);
                    pFixCharacteristic->notify();

                    pNumSatCharacteristic->setValue((uint8_t*)&packet->numSats, 1);
                    pNumSatCharacteristic->notify();

                    pLatCharacteristic->setValue((uint8_t*)&packet->lat, 4);
                    pLatCharacteristic->notify();
                    
                    pLonCharacteristic->setValue((uint8_t*)&packet->lon, 4);
                    pLonCharacteristic->notify();

                    pSpeedCharacteristic->setValue((uint8_t*)&packet->groundSpeed, 4);
                    pSpeedCharacteristic->notify();

                    pHeightCharacteristic->setValue((uint8_t*)&packet->height, 4);
                    pHeightCharacteristic->notify();

                    vTaskDelay(100 / portTICK_PERIOD_MS); // bluetooth stack will go into congestion, if too many packets are sent
                }
                printf("packetSize: %i, sats: %i, fix: %i, lat: %d, lon: %d\n", size, packet->numSats, packet->fixType, packet->lat, packet->lon);
                lora_receive();
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);

            // disconnecting
            if (!deviceConnected && oldDeviceConnected) {
                vTaskDelay(500 / portTICK_PERIOD_MS); // give the bluetooth stack the chance to get things ready
                pServer->startAdvertising(); // restart advertising
                //Serial.println("start advertising");
                oldDeviceConnected = deviceConnected;
            }
            // connecting
            if (deviceConnected && !oldDeviceConnected) {
                // do stuff here on connecting
                oldDeviceConnected = deviceConnected;
            }
        }
    }
}
