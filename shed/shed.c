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
#include "button.h"


// void led_write(bool on) {
//     gpio_write(LED_PIN, on ? 0 : 1);
// }
void temperature_sensor_identify(homekit_value_t _value) {
    printf("Temperature sensor identify\n");
    // for (int i=0; i<3; i++) {
    //     for (int j=0; j<2; j++) {
    //         led_write(true);
    //         vTaskDelay(100 / portTICK_PERIOD_MS);
    //         led_write(false);
    //         vTaskDelay(100 / portTICK_PERIOD_MS);
    //     }

    //     vTaskDelay(250 / portTICK_PERIOD_MS);
    // }
}

homekit_characteristic_t temperature1 = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t humidity1    = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);

homekit_characteristic_t temperature2 = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t humidity2    = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);

homekit_characteristic_t temperature3 = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t humidity3    = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);

// 9.2 fan
// 9.4 lightbulb
// 9.8 switch
// 9.9 thermostat
// 9.15 humidity sensor
// 9.18 motion sensor
// 9.22 temperature sensor 
// 9.25 battery serrvice

void read_temperature_sensor(int pin, homekit_characteristic_t temperature, homekit_characteristic_t humidity) {
    float humidity_value, temperature_value;

    bool success = dht_read_float_data(
        DHT_TYPE_DHT22, pin,
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
}

void temperature_sensor_task(void *_args) {
    gpio_set_pullup(SENSOR1_PIN, false, false);
    gpio_set_pullup(SENSOR2_PIN, false, false);
    gpio_set_pullup(SENSOR3_PIN, false, false);

    gpio_enable(EXTERIOR_FLOOD, GPIO_OUTPUT);
    gpio_enable(INTERIOR_LIGHTS, GPIO_OUTPUT);
    gpio_enable(PHONE_CHARGER, GPIO_OUTPUT);
    gpio_enable(EASTWEST_FAN, GPIO_OUTPUT);
    gpio_enable(WESTEAST_FAN, GPIO_OUTPUT);

    while (1) {
        read_temperature_sensor(SENSOR1_PIN, temperature1, humidity1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        read_temperature_sensor(SENSOR2_PIN, temperature2, humidity2);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        read_temperature_sensor(SENSOR3_PIN, temperature3, humidity3);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void temperature_sensor_init() {
    xTaskCreate(temperature_sensor_task, "Temperature Sensor", 512, NULL, 2, NULL);
    // gpio_enable(LED_PIN, GPIO_OUTPUT);
    // led_write(true);
    // vTaskDelay(250 / portTICK_PERIOD_MS);
    // led_write(false);
}

bool interior_lights_on = false;
homekit_value_t interior_lights_get() {
    return HOMEKIT_BOOL(interior_lights_on);
}

void interior_lights_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid value format: %d\n", value.format);
        return;
    }

    interior_lights_on = value.bool_value;
    gpio_write(INTERIOR_LIGHTS, interior_lights_on ? 1 : 0);
}

bool exterior_flood_on = false;
homekit_value_t exterior_flood_get() {
    return HOMEKIT_BOOL(exterior_flood_on);
}

void exterior_flood_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid value format: %d\n", value.format);
        return;
    }

    exterior_flood_on = value.bool_value;
    gpio_write(EXTERIOR_FLOOD, exterior_flood_on ? 1 : 0);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Shed Control");
homekit_characteristic_t serialNumber = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "XXXXXXXXXX");

homekit_characteristic_t thermostat_current_state = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATING_COOLING_STATE, 0);
homekit_characteristic_t thermostat_target_state = HOMEKIT_CHARACTERISTIC_(TARGET_HEATING_COOLING_STATE, 0);
homekit_characteristic_t thermostat_target_temperature = HOMEKIT_CHARACTERISTIC_(TARGET_TEMPERATURE, 18.0f);
/* 
    //  Required Characteristics:
    - CURRENT_HEATING_COOLING_STATE
    - TARGET_HEATING_COOLING_STATE
    - CURRENT_TEMPERATURE
    - TARGET_TEMPERATURE
    - TEMPERATURE_DISPLAY_UNITS
 */

homekit_characteristic_t button = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);
homekit_characteristic_t motion = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, false);

homekit_characteristic_t battery_level  = HOMEKIT_CHARACTERISTIC_(BATTERY_LEVEL, 100);
homekit_characteristic_t charging_state  = HOMEKIT_CHARACTERISTIC_(CHARGING_STATE, 0);
homekit_characteristic_t status_low_battery  = HOMEKIT_CHARACTERISTIC_(STATUS_LOW_BATTERY, 0);


void button_callback(uint8_t gpio, button_event_t event) {
    switch (event) {
        case button_event_single_press:
            printf("single press\n");
            homekit_characteristic_notify(&button, HOMEKIT_UINT8(0));
            break;
        case button_event_double_press:
            printf("double press\n");
            homekit_characteristic_notify(&button, HOMEKIT_UINT8(1));
            break;
        case button_event_long_press:
            printf("long press\n");
            homekit_characteristic_notify(&button, HOMEKIT_UINT8(2));
            break;
        default:
            printf("unknown button event: %d\n", event);
    }
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_garage, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Alex"),
            &serialNumber,
            HOMEKIT_CHARACTERISTIC(MODEL, "Shed2,1"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, BUILD_VERSION),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, temperature_sensor_identify),
            NULL
        }),
        HOMEKIT_SERVICE(BATTERY_SERVICE, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Battery"),
            &battery_level,
            &charging_state,
            &status_low_battery,
            NULL
        }),
        HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "West Exterior Temperature Sensor"),
            &temperature1,
            NULL
        }),
        HOMEKIT_SERVICE(HUMIDITY_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "West Exterior Humidity Sensor"),
            &humidity1,
            NULL
        }),

        HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "East Exterior Temperature Sensor"),
            &temperature2,
            NULL
        }),
        HOMEKIT_SERVICE(HUMIDITY_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "East Exterior Humidity Sensor"),
            &humidity2,
            NULL
        }),

        HOMEKIT_SERVICE(HUMIDITY_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Interior Humidity Sensor"),
            &humidity3,
            NULL
        }),

// THERMOSTAT
        HOMEKIT_SERVICE(THERMOSTAT,  .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Interior Thermostat"),
            &temperature3,
            HOMEKIT_CHARACTERISTIC(TEMPERATURE_DISPLAY_UNITS, 0),
            &thermostat_target_temperature,
            &thermostat_current_state,
            &thermostat_target_state,
            NULL
        }),

// INTERIOR LIGHTS
        HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Light Switch"),
            &button,
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Interior Lights"),
            HOMEKIT_CHARACTERISTIC(ON, false,                 
                .getter=interior_lights_get,
                .setter=interior_lights_set
            ),
            NULL
        }),

// EXTERIOR LIGHTS
        HOMEKIT_SERVICE(MOTION_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Motion Sensor"),
            &motion,
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Exterior Flood Light"),
            HOMEKIT_CHARACTERISTIC(ON, false,
                .getter=exterior_flood_get,
                .setter=exterior_flood_set
            ),
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
    / * Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code 
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

    int name_len = snprintf(NULL, 0, "Shed %02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Shed %02X%02X%02X",
             macaddr[3], macaddr[4], macaddr[5]);

    int serial_len = snprintf(NULL, 0, "%02X%02X%02X%02X%02X%02X",
                            macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
    char *serial_value = malloc(serial_len+1);
    snprintf(serial_value, serial_len+1, "%02X%02X%02X%02X%02X%02X",
             macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

    name.value = HOMEKIT_STRING(name_value);
    serialNumber.value = HOMEKIT_STRING(serial_value);

    wifi_config_init("Shed", NULL, on_wifi_ready);
    printf("~~~ Name: %s\n", name_value);
    printf("~~~ Serial Number: %s\n", serial_value);
    printf("~~~ Firmware build version: %s\n", BUILD_VERSION);

    if (button_create(BUTTON_PIN, button_callback)) {
        printf("Failed to initialize button\n");
    }
}

