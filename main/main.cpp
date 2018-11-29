#include "driver/gpio.h"
#include "common.h"
#include "gps.h"

extern "C" {
    #include "lora.h"
    #include "ota_server.h"
}

#include "ble.h"
#include "util/log.h"
#include "util/ble_server.h"
#include "util/fields.h"
#include "util/ble_uart_server.h"

#define TAG "main"

#define RECEIVER true

#define WIFI_SSID             "WLAN-0024FEACF701"
#define WIFI_PASSWORD         "5irvalu5e"

bool mLogActive = false;
bool mAppConnected = false;
bool mLogActiveSaved = false;

uint32_t mLastTimeReceivedData = 0;
uint32_t mLastTimeFeedback = 0; 
uint32_t mTimeSeconds = 0;

static EventGroupHandle_t wifi_event_group;

static const int CONNECTED_BIT = BIT0;

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

void appInitialize(bool resetTimerSeconds) {

	// Restore logging ?

	if (mLogActiveSaved && !mLogActive) {
		mLogActive = true; // Restore state
	}

	logD ("Initializing ..."); 

	///// Initialize the variables

	// Initialize times 

	mLastTimeReceivedData = 0;
	mLastTimeFeedback = 0;

	// TODO: see it! Please put here custom global variables or code for init

	// Debugging

	logD ("Initialized");
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

void processBleMessage (const string& message) {

	// This is to process ASCII (text) messagens - not binary ones

	string response = ""; // Return response to mobile app

	// --- Process the received line 

	// Check the message

	if (message.length () < 2 ) {

		//error("Message length must have 2 or more characters");
		return; 

	} 

	// Process fields of the message 

	Fields fields(message, ":");

	// Code of the message 

	uint8_t code = 0;

	if (!fields.isNum (1)) { // Not numerical
		//error ("Non-numeric message code"); 
		return; 
	} 

	code = fields.getInt (1);

	if (code == 0) { // Invalid code
		//error ("Invalid message code"); 
		return; 
	} 

	//logV("Code -> %u Message -> %s", code, mUtil.strExpand (message).c_str());

	// Considers the message received as feedback also 

	mLastTimeFeedback = mTimeSeconds;

	// Process the message 

#ifdef HAVE_BATTERY
	bool sendEnergy = false; // Send response with the energy situation too? 
#endif

	switch (code) { // Note: the '{' and '}' in cases, is to allow to create local variables, else give an error cross definition ... 

	// --- Initial messages 

	case 1: // Start 
		{
			// Initial message sent by the mobile application, to indicate start of the connection

			if (mLogActive) { 
				//debugInitial();
			}

			// Reinicialize the app - include timer of seconds

			appInitialize(true);

			// Indicates connection initiated by the application 

			mAppConnected = true;

			// Inform to mobile app, if this device is battery powered and sensors 
			// Note: this is important to App works with differents versions or models of device

#ifdef HAVE_BATTERY

			// Yes, is a battery powered device

			string haveBattery = "Y";
			
#ifdef PIN_SENSOR_CHARGING

			// Yes, have a sensor of charging
			string sensorCharging = "Y";

#else
			// No have a sensor of charging
			string sensorCharging = "N";
#endif
			// Send energy status (also if this project not have battery, to mobile app know it)

			sendEnergy = true;

#else

			// No, no is a battery powered device

			string haveBattery = "N";
			string sensorCharging = "N";

#endif
			// Debug

			bool turnOnDebug = false;

#ifdef HAVE_BATTERY

			// Turn on the debugging (if the USB is connected)

			if (mGpioVEXT && !mLogActive) {
				turnOnDebug = true; 
			} 
#else

			// Turn on debugging

			turnOnDebug = !mLogActive;

#endif

			// Turn on the debugging (if the USB cable is connected)

			if (turnOnDebug) {

				mLogActive = true; 
				//debugInitial();
			} 

			// Reset the time in main_Task

			//notifyMainTask(MAIN_TASK_ACTION_RESET_TIMER);

			// Returns status of device, this firware version and if is a battery powered device

			response = "01:";
			response.append("version");
			response.append(1u, ':');
			response.append(haveBattery);
			response.append(1u, ':');
			response.append(sensorCharging);
			
		}
		break; 
	
#ifdef HAVE_BATTERY
	case 10: // Status of energy: battery or external (USB or power supply)
		{
			sendEnergy = true;
		}
		break; 
#endif
	
	case 11: // Request of ESP32 informations
		{
			// Example of passing fields class to routine process

			//sendInfo(fields);

		}
		break;

	// TODO: see it! Please put here custom messages

	case 70: // Echo (for test purpose)
		{
			response = message;
		}
		break; 

	case 71: // Logging - activate or desactivate debug logging - save state to use after
		{
			switch (fields.getChar(2)) // Process options
			{
				case 'Y': // Yes

					mLogActiveSaved = mLogActive; // Save state
					mLogActive = true; // Activate it

					logV("Logging activated now");
					break;

				case 'N': // No

					logV("Logging deactivated now");

					mLogActiveSaved = mLogActive; // Save state
					mLogActive = false; // Deactivate it
					break;

				case 'R': // Restore

					mLogActive = mLogActiveSaved; // Restore state
					logV("Logging state restored now");
					break;
			}
		}
		break; 

	case 80: // Feedback 
		{
			// Message sent by the application periodically, for connection verification

			logV("Feedback recebido");

			// Response it (put here any information that needs)

			response = "80:"; 
		}
		break; 

	case 98: // Reinicialize the app
		{
			logI ("Reinitialize");

			// End code placed at the end of this routine to send OK before 
		}
		break; 

	case 99: // Enter in standby
		{
			logI ("Entering in standby");

			// End code placed at the end of this routine to send OK before 
		}
		break; 

	default: 
		{
			string errorMsg = "Code of message invalid: ";
			//errorMsg.append(mUtil.intToStr(code)); 
			//error (errorMsg.c_str()); 
			return;
		}
	}

	// return 

	if (response.size() > 0) {

		// Return -> Send message response

		if (bleConnected ()) { 
			bleSendData(response);
		} 
	} 

#ifdef HAVE_BATTERY
	// Return energy situation too? 

	if (sendEnergy) { 

		checkEnergyVoltage (true);

	} 
#endif

	// Mark the mTimeSeconds of the receipt 

	mLastTimeReceivedData = mTimeSeconds;

	// Here is processed messages that have actions to do after response sended

	switch (code) {

	case 98: // Restart the Esp32

		// Wait 500 ms, to give mobile app time to quit 

		if (mAppConnected) {
			//delay (500); 
		}

		//restartESP32();
		break; 

	case 99: // Standby - enter in deep sleep

#ifdef HAVE_STANDBY

		// Wait 500 ms, to give mobile app time to quit 

		if (mAppConnected) {
			//delay (500); 
		}

		// Soft Off - enter in standby by main task to not crash or hang on finalize BLE
		// Notify main_Task to enter standby
			
		//notifyMainTask(MAIN_TASK_ACTION_STANDBY_MSG);

#else

		// No have standby - restart

		//restartESP32();

#endif

		break; 

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
        bleInitialize();
        // receive loop
        packet_t * packet = (packet_t*)malloc(sizeof(packet_t));
        while (true) {
            lora_receive();    // put into receive mode
            while (lora_received()) {
                int size = lora_receive_packet((uint8_t*)packet, sizeof(packet_t));
                printf("packetSize: %i, sats: %i, fix: %i, lat: %d, lon: %d\n", size, packet->numSats, packet->fixType, packet->lat, packet->lon);
                lora_receive();

                if (bleConnected()) {
                    char data[20];
                    sprintf(data, "G,%i,%i,%i,%i", packet->numSats, packet->fixType, packet->lat, packet->lon);
                    bleSendData(data);
                }
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

}
