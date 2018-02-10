#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <dht/dht.h>
#include "ota-tftp.h"
#include "rboot-api.h"


#ifndef SENSOR_PIN
#error SENSOR_PIN is not specified
#endif

#ifndef LED_PIN
#error LED_PIN is not specified
#endif


void led_write(bool on) {
    gpio_write(LED_PIN, on ? 0 : 1);
}
void temperature_sensor_identify(homekit_value_t _value) {
    printf("Temperature sensor identify\n");
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            led_write(true);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            led_write(false);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}

homekit_characteristic_t temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t humidity    = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);


void temperature_sensor_task(void *_args) {
    gpio_set_pullup(SENSOR_PIN, false, false);

    float humidity_value, temperature_value;
    while (1) {
        bool success = dht_read_float_data(
            DHT_TYPE_DHT22, SENSOR_PIN,
            &humidity_value, &temperature_value
        );
        if (success) {
            temperature.value.float_value = temperature_value;
            humidity.value.float_value = humidity_value;

            homekit_characteristic_notify(&temperature, HOMEKIT_FLOAT(temperature_value));
            homekit_characteristic_notify(&humidity, HOMEKIT_FLOAT(humidity_value));

            printf("Current temperature: ");
            printf("%.2f",temperature_value);
            printf("\n");
        } else {
            printf("Couldnt read data from sensor\n");
        }

        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}

void temperature_sensor_init() {
    xTaskCreate(temperature_sensor_task, "Temperature Sensor", 256, NULL, 2, NULL);
    gpio_enable(LED_PIN, GPIO_OUTPUT);
    led_write(true);
    vTaskDelay(250 / portTICK_PERIOD_MS);
    led_write(false);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Temperature Sensor");
homekit_characteristic_t serialNumber = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "XXXXXXXXXX");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Alex"),
            &serialNumber,
            HOMEKIT_CHARACTERISTIC(MODEL, "TemperatureSensor1,1"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, BUILD_VERSION),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, temperature_sensor_identify),
            NULL
        }),
        HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Temperature Sensor"),
            &temperature,
            NULL
        }),
        HOMEKIT_SERVICE(HUMIDITY_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Humidity Sensor"),
            &humidity,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "506-81-392"
};

void on_wifi_ready() {
    homekit_server_init(&config);
    temperature_sensor_init();
    ota_tftp_init_server(TFTP_PORT);
}

#define WEB_SERVER "https://xxxxxxxxxxxx"
#define WEB_URL "/temperatures/post.php"

void post_data() {
    /*const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;

    printf("Running DNS lookup for %s...\r\n", WEB_SERVER);
    int err = getaddrinfo(WEB_SERVER, "3000", &hints, &res);

    if(err != 0 || res == NULL) {
        printf("DNS lookup failed err=%d res=%p\r\n", err, res);
        if(res)
            freeaddrinfo(res);
        vTaskDelay(1000 / portTICK_RATE_MS);
        failures++;
        continue;
    }
    /* Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code 
    struct in_addr *addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    printf("DNS lookup succeeded. IP=%s\r\n", inet_ntoa(*addr));

    int s = socket(res->ai_family, res->ai_socktype, 0);
    if(s < 0) {
        printf("... Failed to allocate socket.\r\n");
        freeaddrinfo(res);
        vTaskDelay(1000 / portTICK_RATE_MS);
        failures++;
        continue;
    }

    printf("... allocated socket\r\n");

    if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
        close(s);
        freeaddrinfo(res);
        printf("... socket connect failed.\r\n");
        vTaskDelay(4000 / portTICK_RATE_MS);
        failures++;
        continue;
    }

    printf("... connected\r\n");
    freeaddrinfo(res);
    request[0] = "\0";
    snprintf(details, 80, "temp=%.3f&humiditiy=%.3f&device=ABCD", temperature, humidity);

    snprintf(request, 300, "POST / HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-open-rtos/0.1 esp8266\r\nConnection: close\r\nContent-Type: application/json; charset=UTF-8\r\nContent-Length: %d\r\n\r\n%s\r\n", WEB_URL, strlen(details), details);
    printf(request);
    if (write(s, request, strlen(request)) < 0) {
        printf("... socket send failed\r\n");
        close(s);
        vTaskDelay(4000 / portTICK_RATE_MS);
        failures++;
        continue;
    }
    printf("... socket send success\r\n");

    static char recv_buf[200];
    int r;
    do {
        printf("receiving...");
        bzero(recv_buf, 200);
        r = read(s, recv_buf, 199);
        if(r > 0) {
            printf("%s", recv_buf);
        }
    } while(r > 0);

    printf("... done reading from socket. Last read return=%d errno=%d\r\n", r, errno);
    if(r != 0)
        failures++;
    else
        successes++;
    close(s); */
}

void user_init(void) {
    uart_set_baud(0, 115200);

    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);

    int name_len = snprintf(NULL, 0, "Temperature Sensor %02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Temperature Sensor %02X%02X%02X",
             macaddr[3], macaddr[4], macaddr[5]);

    int serial_len = snprintf(NULL, 0, "%02X%02X%02X%02X%02X%02X",
                            macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
    char *serial_value = malloc(serial_len+1);
    snprintf(serial_value, serial_len+1, "%02X%02X%02X%02X%02X%02X",
             macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

    name.value = HOMEKIT_STRING(name_value);
    serialNumber.value = HOMEKIT_STRING(serial_value);

    wifi_config_init("Temperature Sensor", NULL, on_wifi_ready);
    printf("~~~ Name: %s\n", name_value);
    printf("~~~ Serial Number: %s\n", serial_value);
    printf("~~~ Firmware build version: %s\n", BUILD_VERSION);
}

