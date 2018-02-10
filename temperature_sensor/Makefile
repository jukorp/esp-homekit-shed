PROGRAM = temperature_sensor

EXTRA_COMPONENTS = \
	extras/dht \
	extras/http-parser \
	extras/dhcpserver \
	extras/rboot-ota \
	$(abspath ../components/wolfssl) \
	$(abspath ../components/cJSON) \
	$(abspath ../components/wifi_config) \
	$(abspath ../components/homekit)

# DHT11 sensor pin
SENSOR_PIN = 14
LED_PIN = 2

FLASH_SIZE = 16

BUILD_VERSION=`date +'"%Y%m%d.%H%M"'`

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS -DSENSOR_PIN=$(SENSOR_PIN) -DLED_PIN=$(LED_PIN) -DBUILD_VERSION=$(BUILD_VERSION)

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)

ota:
	echo verbose > tftp.tftp
	echo binary >> tftp.tftp
	echo put $(FW_FILE) firmware.bin >> tftp.tftp
	echo quit >> tftp.tftp
	$(MAKE) all
	tftp $(ESPIP) < tftp.tftp || true
	rm tftp.tftp