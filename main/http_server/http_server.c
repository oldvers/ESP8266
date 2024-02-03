/*
 * HTTP server example.
 *
 * This sample code is in the public domain.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>

////#include <espressif/esp_common.h>
////#include "esp8266/esp8266.h"
//#include "uart.h"
//#include "gpio.h"
////#include <ssid_config.h>
#include "httpd.h"
#include "esp_system.h"
#include "wifi_task.h"
#include "led_task.h"

#define LED_PIN 2

enum
{
    SSI_UPTIME,
    SSI_FREE_HEAP,
    SSI_LED_STATE
};

static bool    gConfig         = false;

int32_t ssi_handler(int32_t iIndex, char * pcInsert, int32_t iInsertLen)
{
    switch (iIndex)
    {
        case SSI_UPTIME:
            snprintf(pcInsert, iInsertLen, "%d", xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);
            break;
        case SSI_FREE_HEAP:
            snprintf(pcInsert, iInsertLen, "%d", 35000); // (int) xPortGetFreeHeapSize());
            break;
        case SSI_LED_STATE:
            snprintf(pcInsert, iInsertLen, "Off"); // gpio_get_level(LED_PIN) ? "Off" : "On");
            break;
        default:
            snprintf(pcInsert, iInsertLen, "N/A");
            break;
    }

    /* Tell the server how many characters to insert */
    return (strlen(pcInsert));
}

char * gpio_cgi_handler(int iIndex, int iNumParams, char * pcParam[], char * pcValue[])
{
    for (int i = 0; i < iNumParams; i++)
    {
        if (strcmp(pcParam[i], "on") == 0)
        {
            uint8_t gpio_num = atoi(pcValue[i]);
//          gpio_enable(gpio_num, GPIO_OUTPUT);
//          gpio_write(gpio_num, true);
        }
        else if (strcmp(pcParam[i], "off") == 0)
        {
            uint8_t gpio_num = atoi(pcValue[i]);
//          gpio_enable(gpio_num, GPIO_OUTPUT);
//          gpio_write(gpio_num, false);
        }
        else if (strcmp(pcParam[i], "toggle") == 0)
        {
            uint8_t gpio_num = atoi(pcValue[i]);
//          gpio_enable(gpio_num, GPIO_OUTPUT);
//          gpio_toggle(gpio_num);
        }
    }
    return "/index.ssi";
}

char * complete_cgi_handler(int iIndex, int iNumParams, char * pcParam[], char * pcValue[])
{
    return "/complete.html";
}

char * websocket_cgi_handler(int iIndex, int iNumParams, char * pcParam[], char * pcValue[])
{
    return "/websockets.html";
}

void websocket_task(void * pvParameter)
{
    struct tcp_pcb * pcb = (struct tcp_pcb *) pvParameter;

    for (;;)
    {
        if (pcb == NULL || pcb->state != ESTABLISHED)
        {
            printf("Connection closed, deleting task\n");
            break;
        }

        int uptime = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
        int heap = 35000; //(int) xPortGetFreeHeapSize();
        int led = 0; //!gpio_read(LED_PIN);

        /* Generate response in JSON format */
        char response[64];
        int len = snprintf(response, sizeof (response),
                "{\"uptime\" : \"%d\","
                " \"heap\" : \"%d\","
                " \"led\" : \"%d\"}", uptime, heap, led);
        if (len < sizeof (response))
        {
            websocket_write(pcb, (unsigned char *) response, len, WS_TEXT_MODE);
        }

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

bool websocket_parse_wifi_string(wifi_string_p p_str, uint8_t * p_buf, uint8_t * p_offset)
{
    wifi_string_p p_str_buf = NULL;

    if ((WIFI_STRING_MAX_LEN * 3) < (*p_offset)) return false;
    
    p_str_buf = (wifi_string_p)&p_buf[*p_offset];

    if (WIFI_STRING_MAX_LEN < p_str_buf->length) return false;

    memcpy(p_str, p_str_buf, (p_str_buf->length + 1));
    *p_offset += (p_str_buf->length + 1);

    return true;
}

bool websocket_copy_wifi_string(uint8_t * p_buf, uint8_t * p_offset, wifi_string_p p_str)
{
    if ((WIFI_STRING_MAX_LEN * 3) < (*p_offset)) return false;
    if (WIFI_STRING_MAX_LEN < p_str->length) return false;

    memcpy(&p_buf[*p_offset], p_str, (p_str->length + 1));
    *p_offset += (p_str->length + 1);

    return true;
}

/**
 * This function is called when websocket frame is received.
 *
 * Note: this function is executed on TCP thread and should return as soon
 * as possible.
 */
void websocket_cb(struct tcp_pcb * pcb, uint8_t * data, uint16_t data_len, uint8_t mode)
{
    enum
    {
        MAX_LEN = (4 + 3 * sizeof(wifi_string_t)),
    };
    printf("[websocket_callback]:\n");
    //%.*s\n", (int)data_len, (char *)data);

    uint8_t       response[MAX_LEN] = {0};
    uint16_t      val               = 0;
    uint32_t      rnd               = 0;
    uint8_t       offset            = 1;
    uint8_t       len               = 2;
    wifi_string_t ssid              = {0};
    wifi_string_t pswd              = {0};
    wifi_string_t site              = {0};
    bool          result            = true;

    switch (data[0])
    {
        case 0x01:
            result = WiFi_GetParams(&ssid, &pswd, &site);
            if (true == result)
            {
                offset = 2;
                result &= websocket_copy_wifi_string(response, &offset, &ssid);
                result &= websocket_copy_wifi_string(response, &offset, &pswd);
                result &= websocket_copy_wifi_string(response, &offset, &site);
                if (result)
                {
                    len = offset;
                    val = 0x0100;
                }
            }
            break;
        case 0x02:
            result &= websocket_parse_wifi_string(&ssid, data, &offset);
            result &= websocket_parse_wifi_string(&pswd, data, &offset);
            result &= websocket_parse_wifi_string(&site, data, &offset);
            if (result)
            {
                printf("The config received!\n");
                printf(" - SSID = %s\n", ssid.data);
                printf(" - PSWD = %s\n", pswd.data);
                printf(" - SITE = %s\n", site.data);
            }
            result &= WiFi_SaveParams(&ssid, &pswd, &site);
            if (result)
            {
                val = 0x0200;
            }
            break;
        case 0x03:
            printf("Received Color command %d bytes\n", data_len);

            led_message_t msg = {LED_CMD_INDICATE_COLOR, data[1], data[2], data[3]};
            LED_Task_SendMsg(&msg);

            val = 0x0300;
            break;
        case 'A': // ADC
            /* This should be done on a separate thread in 'real' applications */
            rnd = esp_random();
            val = (rnd >> 22); // sdk_system_adc_read();
            break;
        case 'D': // Disable LED
//          gpio_write(LED_PIN, true);
            val = 0xDEAD;
            break;
        case 'E': // Enable LED
//          gpio_write(LED_PIN, false);
            val = 0xBEEF;
            break;
        default:
            printf("Unknown command\n");
            val = 0;
            break;
    }

    response[1] = (uint8_t) val;
    response[0] = val >> 8;

    websocket_write(pcb, response, len, WS_BIN_MODE);
}

/**
 * This function is called when new websocket is open and
 * creates a new websocket_task if requested URI equals '/stream'.
 */
void websocket_open_cb(struct tcp_pcb * pcb, const char * uri)
{
    printf("WS URI: %s\n", uri);
    if (!strcmp(uri, "/stream"))
    {
        printf("request for streaming\n");
        xTaskCreate(&websocket_task, "websocket_task", 1024, (void *)pcb, 2, NULL);
    }
}

//-------------------------------------------------------------------------------------------------

void httpd_task(void * pvParameters)
{
    tCGI pCGIs[] =
    {
        {"/gpio", (tCGIHandler) gpio_cgi_handler},
        {"/complete", (tCGIHandler) complete_cgi_handler},
        {"/websockets", (tCGIHandler) websocket_cgi_handler},
    };

    const char * pcConfigSSITags[] =
    {
        "uptime", // SSI_UPTIME
        "heap",   // SSI_FREE_HEAP
        "led"     // SSI_LED_STATE
    };

    /* register handlers and start the server */
    http_set_cgi_handlers(pCGIs, sizeof (pCGIs) / sizeof (pCGIs[0]));
    http_set_ssi_handler
    (
        (tSSIHandler) ssi_handler,
        pcConfigSSITags,
        sizeof (pcConfigSSITags) / sizeof (pcConfigSSITags[0])
    );
    websocket_register_callbacks
    (
        (tWsOpenHandler) websocket_open_cb,
        (tWsHandler) websocket_cb
    );
    httpd_init(gConfig);

    for (;;)
    {
        vTaskDelay(25 / portTICK_PERIOD_MS);
    }
}

//-------------------------------------------------------------------------------------------------

void HTTP_Server_Init(bool config)
{
    printf("SDK version: %s\n", esp_get_idf_version());

    gConfig = config;
//    if (gConfig)
//    {
//        led_message_t msg = {LED_CMD_INDICATE_RUN, 0, 0, 0};
//        LED_Task_SendMsg(&msg);
//    }

//    struct sdk_station_config config = {
//        .ssid = "HomeWLAN",
//        .password = "wlanH00785endrix!",
//    };

    /* required to call wifi_set_opmode before station_set_config */
//    sdk_wifi_set_opmode(STATION_MODE);
//    sdk_wifi_station_set_config(&config);
//    sdk_wifi_station_connect();

    /* turn off LED */
//    gpio_enable(LED_PIN, GPIO_OUTPUT);
//    gpio_write(LED_PIN, true);

    /* initialize tasks */
    xTaskCreate(&httpd_task, "HTTP Daemon", 2048, NULL, 2, NULL);
}

//-------------------------------------------------------------------------------------------------
