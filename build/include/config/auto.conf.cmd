deps_config := \
	/Users/bjoern/esp/esp-idf/components/app_trace/Kconfig \
	/Users/bjoern/esp/esp-idf/components/aws_iot/Kconfig \
	/Users/bjoern/esp/esp-idf/components/bt/Kconfig \
	/Users/bjoern/esp/esp-idf/components/driver/Kconfig \
	/Users/bjoern/esp/esp-idf/components/esp32/Kconfig \
	/Users/bjoern/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/Users/bjoern/esp/esp-idf/components/esp_event/Kconfig \
	/Users/bjoern/esp/esp-idf/components/esp_http_client/Kconfig \
	/Users/bjoern/esp/esp-idf/components/esp_http_server/Kconfig \
	/Users/bjoern/esp/esp-idf/components/ethernet/Kconfig \
	/Users/bjoern/esp/esp-idf/components/fatfs/Kconfig \
	/Users/bjoern/esp/esp-idf/components/freemodbus/Kconfig \
	/Users/bjoern/esp/esp-idf/components/freertos/Kconfig \
	/Users/bjoern/esp/esp-idf/components/heap/Kconfig \
	/Users/bjoern/esp/esp-idf/components/libsodium/Kconfig \
	/Users/bjoern/esp/esp-idf/components/log/Kconfig \
	/Users/bjoern/gpstracker/components/lora/Kconfig \
	/Users/bjoern/esp/esp-idf/components/lwip/Kconfig \
	/Users/bjoern/esp/esp-idf/components/mbedtls/Kconfig \
	/Users/bjoern/esp/esp-idf/components/mdns/Kconfig \
	/Users/bjoern/esp/esp-idf/components/mqtt/Kconfig \
	/Users/bjoern/esp/esp-idf/components/nvs_flash/Kconfig \
	/Users/bjoern/esp/esp-idf/components/openssl/Kconfig \
	/Users/bjoern/esp/esp-idf/components/pthread/Kconfig \
	/Users/bjoern/esp/esp-idf/components/spi_flash/Kconfig \
	/Users/bjoern/esp/esp-idf/components/spiffs/Kconfig \
	/Users/bjoern/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/Users/bjoern/esp/esp-idf/components/unity/Kconfig \
	/Users/bjoern/esp/esp-idf/components/vfs/Kconfig \
	/Users/bjoern/esp/esp-idf/components/wear_levelling/Kconfig \
	/Users/bjoern/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/Users/bjoern/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/Users/bjoern/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/Users/bjoern/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_TARGET)" "esp32"
include/config/auto.conf: FORCE
endif
ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
