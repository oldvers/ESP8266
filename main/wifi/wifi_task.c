#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "types.h"

//-------------------------------------------------------------------------------------------------

#define WIFI_SSID                "TestWLAN"
#define WIFI_PASS                "wlanH020785endrix!"
#define EVT_WIFI_STARTED         BIT0
#define EVT_WIFI_CONNECTED       BIT1
#define EVT_WIFI_GOT_IP          BIT2
#define EVT_WIFI_DISCONNECTED    BIT3

//-------------------------------------------------------------------------------------------------

static const char *TAG = "WiFi";
static EventGroupHandle_t gWiFiEvents = NULL;

//-------------------------------------------------------------------------------------------------

#define HNDLR_REG(base,id,handler)                                            \
        do                                                                    \
        {                                                                     \
            esp_err_t result;                                                 \
            result = esp_event_handler_register(base, id, handler, NULL);     \
            ESP_ERROR_CHECK(result);                                          \
        } while (0);

#define HNDLR_UNREG(base,id,handler)                                          \
        do                                                                    \
        {                                                                     \
            esp_err_t result;                                                 \
            result = esp_event_handler_unregister(base, id, handler);         \
            ESP_ERROR_CHECK(result);                                          \
        } while (0);


//-------------------------------------------------------------------------------------------------

static EventBits_t vWiFi_WaitFor(EventBits_t events, TickType_t timeout)
{
    EventBits_t bits = 0;

    /* Waiting until either specified event is set */
    bits = xEventGroupWaitBits
           (
               gWiFiEvents,
               events,
               pdTRUE,
               pdFALSE,
               timeout / portTICK_RATE_MS
           );

    return bits;
}

//-------------------------------------------------------------------------------------------------

static void vWiFi_OnStarted
(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
)
{
    ESP_LOGI(TAG, "Started!");
    xEventGroupSetBits(gWiFiEvents, EVT_WIFI_STARTED);
}

//-------------------------------------------------------------------------------------------------

static void vWiFi_Start(void)
{
    /* Prepare the events loop */
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Prepare the default configuration for WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Create the events group for WiFi task */
    gWiFiEvents = xEventGroupCreate();
    
    /* Register event handler */
    HNDLR_REG(WIFI_EVENT, WIFI_EVENT_STA_START, &vWiFi_OnStarted);

    //ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    /* Prepare the WiFi parameters. Temporary in RAM */
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config =
    {
        .sta =
        {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        },
    };

    ESP_LOGI(TAG, "Starting...");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());
    
    (void)vWiFi_WaitFor(EVT_WIFI_STARTED, portMAX_DELAY);
    
    /* Unregister event handler */
    HNDLR_UNREG(WIFI_EVENT, WIFI_EVENT_STA_START, &vWiFi_OnStarted);
}

//-------------------------------------------------------------------------------------------------

static void vWiFi_OnConnected
(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
)
{
    ESP_LOGI(TAG, "Connected to \"%s\"", WIFI_SSID);
    xEventGroupSetBits(gWiFiEvents, EVT_WIFI_CONNECTED);
}

//-------------------------------------------------------------------------------------------------

static void vWiFi_OnGotIp
(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
)
{
    ip_event_got_ip_t * event = (ip_event_got_ip_t*)event_data;

    ESP_LOGI(TAG, "Got IP : %s", ip4addr_ntoa(&event->ip_info.ip));
    xEventGroupSetBits(gWiFiEvents, EVT_WIFI_GOT_IP);
}    

//-------------------------------------------------------------------------------------------------

static void vWiFi_OnDisconnected
(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
)
{ 
    ESP_LOGE(TAG, "Disconnected!");
    xEventGroupSetBits(gWiFiEvents, EVT_WIFI_DISCONNECTED);
}

//-------------------------------------------------------------------------------------------------

static FW_BOOLEAN vWiFi_Connect(void)
{
    EventBits_t events = 0;
    FW_BOOLEAN result = FW_FALSE;
    
    /* Register event handlers */
    HNDLR_REG(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &vWiFi_OnConnected);
    HNDLR_REG(IP_EVENT, IP_EVENT_STA_GOT_IP, &vWiFi_OnGotIp);
    HNDLR_REG(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &vWiFi_OnDisconnected);
    
    /* Connect */
    ESP_LOGI(TAG, "Connecting to \"%s\"...", WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    /* Wait for connection */
    events = vWiFi_WaitFor(EVT_WIFI_DISCONNECTED | EVT_WIFI_GOT_IP, 10000);
    if (0 != (events & EVT_WIFI_GOT_IP))
    {
        ESP_LOGI(TAG, "Connected successfuly");
        result = FW_TRUE;
    }
    else if (0 != (events & EVT_WIFI_DISCONNECTED))
    {
        ESP_LOGE(TAG, "Reconnection needed");
    }
    else
    {
        ESP_LOGE(TAG, "Something wrong! Reconnection needed");
    }
    
    /* Unregister event handlers */
    HNDLR_UNREG(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &vWiFi_OnConnected);
    HNDLR_UNREG(IP_EVENT, IP_EVENT_STA_GOT_IP, &vWiFi_OnGotIp);
    
    if (FW_FALSE == result)
    {
        HNDLR_UNREG(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &vWiFi_OnDisconnected);
    }

    return result;
}

//-------------------------------------------------------------------------------------------------

static void vWiFi_WaitForDisconnect(void)
{
    (void)vWiFi_WaitFor(EVT_WIFI_DISCONNECTED, portMAX_DELAY);
    
    /* Unregister event handler */
    HNDLR_UNREG(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &vWiFi_OnDisconnected);
}

//-------------------------------------------------------------------------------------------------

void vWiFi_Task(void * pvParams)
{
    vWiFi_Start();

    while (FW_TRUE)
    {
        if (FW_TRUE == vWiFi_Connect())
        {
            vWiFi_WaitForDisconnect();
        }
        vTaskDelay(10000 / portTICK_RATE_MS);
    }
}

//-------------------------------------------------------------------------------------------------

void WIFI_Task_Init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    xTaskCreate(vWiFi_Task, "WiFi", 4096, NULL, 5, NULL);
}

//-------------------------------------------------------------------------------------------------
