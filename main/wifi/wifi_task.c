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
#include "udp_task.h"

//-------------------------------------------------------------------------------------------------

#define WIFI_SSID                "HomeWLAN"
#define WIFI_PSWD                "wlanH020785endrix!"
#define WIFI_SITE                "home.com"
#define EVT_WIFI_STARTED         BIT0
#define EVT_WIFI_CONNECTED       BIT1
#define EVT_WIFI_GOT_IP          BIT2
#define EVT_WIFI_DISCONNECTED    BIT3

//-------------------------------------------------------------------------------------------------

typedef struct
{
    uint16_t ssid_len;
    char     ssid[32];
    uint16_t pswd_len;
    char     pswd[32];
    uint16_t site_len;
    char     site[32];
    bool     valid;
} wifi_conn_params_t;

//-------------------------------------------------------------------------------------------------

static const char *       TAG             = "WiFi";
static EventGroupHandle_t gWiFiEvents     = NULL;
static uint32_t           gIpAddr         = 0;
static wifi_conn_params_t gWiFiConnParams = {0};

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

static EventBits_t wifi_WaitFor(EventBits_t events, TickType_t timeout)
{
    EventBits_t bits = 0;

    /* Waiting until either specified event is set */
    bits = xEventGroupWaitBits
           (
               gWiFiEvents,
               events,       /* Bits To Wait For */
               pdTRUE,       /* Clear On Exit */
               pdFALSE,      /* Wait For All Bits */
               timeout / portTICK_RATE_MS
           );

    return bits;
}

//-------------------------------------------------------------------------------------------------

static void wifi_OnStarted
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

static void wifi_OnConnected
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

static void wifi_OnGotIp
(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
)
{
    ip_event_got_ip_t * event = (ip_event_got_ip_t*)event_data;

    gIpAddr = event->ip_info.ip.addr;
    //ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

    ESP_LOGI(TAG, "Got IP : %s", ip4addr_ntoa(&event->ip_info.ip));
    xEventGroupSetBits(gWiFiEvents, EVT_WIFI_GOT_IP);
}    

//-------------------------------------------------------------------------------------------------

static void wifi_OnDisconnected
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

static void wifi_RegisterHandlers(void)
{
    HNDLR_REG
    (
        WIFI_EVENT,
        WIFI_EVENT_STA_START,
        &wifi_OnStarted
    );
    HNDLR_REG
    (
        WIFI_EVENT,
        WIFI_EVENT_STA_CONNECTED,
        &wifi_OnConnected
    );
    HNDLR_REG
    (
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_OnGotIp
    );
    HNDLR_REG
    (
        WIFI_EVENT,
        WIFI_EVENT_STA_DISCONNECTED,
        &wifi_OnDisconnected
    );
}

//-------------------------------------------------------------------------------------------------

static void wifi_Start(void)
{
    /* Prepare the events loop */
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Prepare the default configuration for WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    /* Register event handler */
    wifi_RegisterHandlers();
    //HNDLR_REG(WIFI_EVENT, WIFI_EVENT_STA_START, &vWiFi_OnStarted);

    //ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    /* Prepare the WiFi parameters. Temporary in RAM */
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config =
    {
        .sta =
        {
            .ssid = WIFI_SSID,
            .password = WIFI_PSWD
        },
    };

    ESP_LOGI(TAG, "Starting...");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());
    
    (void)wifi_WaitFor(EVT_WIFI_STARTED, portMAX_DELAY);
    
    /* Unregister event handler */
    //HNDLR_UNREG(WIFI_EVENT, WIFI_EVENT_STA_START, &vWiFi_OnStarted);
}

//-------------------------------------------------------------------------------------------------

static FW_BOOLEAN wifi_Connect(void)
{
    EventBits_t events = 0;
    FW_BOOLEAN result = FW_FALSE;
    
    /* Register event handlers */
    //HNDLR_REG(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &vWiFi_OnConnected);
    //HNDLR_REG(IP_EVENT, IP_EVENT_STA_GOT_IP, &vWiFi_OnGotIp);
    //HNDLR_REG(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &vWiFi_OnDisconnected);
    
    /* Connect */
    ESP_LOGI(TAG, "Connecting to \"%s\"...", WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    /* Wait for connection */
    events = wifi_WaitFor(EVT_WIFI_DISCONNECTED | EVT_WIFI_GOT_IP, 10000);
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
    //HNDLR_UNREG(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &vWiFi_OnConnected);
    //HNDLR_UNREG(IP_EVENT, IP_EVENT_STA_GOT_IP, &vWiFi_OnGotIp);
    
    //if (FW_FALSE == result)
    //{
    //    HNDLR_UNREG(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &vWiFi_OnDisconnected);
    //}

    return result;
}

//-------------------------------------------------------------------------------------------------

static void wifi_WaitForDisconnect(void)
{
    (void)wifi_WaitFor(EVT_WIFI_DISCONNECTED, portMAX_DELAY);
    
    /* Unregister event handler */
    //HNDLR_UNREG(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &vWiFi_OnDisconnected);
}

//-------------------------------------------------------------------------------------------------

static bool wifi_LoadParams(wifi_conn_params_t * p_params)
{
    nvs_handle         h_nvs  = 0;
    esp_err_t          status = ESP_OK;
    wifi_conn_params_t params = {0};
    size_t             length = 0;
    bool               result = false;

    status = nvs_flash_init();
    ESP_ERROR_CHECK(status);

    status = nvs_open("wifi", NVS_READONLY, &h_nvs);
    if (ESP_OK == status)
    {
        do
        {
            memset(&params, 0, sizeof(params));

            length = sizeof(params.ssid);
            status = nvs_get_str(h_nvs, "ssid", params.ssid, &length);
            if (ESP_OK != status) break;
            params.ssid_len = length;

            length = sizeof(params.pswd);
            status = nvs_get_str(h_nvs, "pswd", params.pswd, &length);
            if (ESP_OK != status) break;
            params.pswd_len = length;

            length = sizeof(params.site);
            status = nvs_get_str(h_nvs, "site", params.site, &length);
            if (ESP_OK != status) break;
            params.site_len = length;

            result = true;
        }
        while (0);
    }

    if (true == result)
    {
        memcpy(p_params, &params, sizeof(*p_params));
        p_params->valid = true;

        ESP_LOGI
        (
            TAG, "Restored params:\n  - SSID:%d:%s\n  - PSWD:%d:%s\n  - SITE:%d:%s",
            params.ssid_len, params.ssid,
            params.pswd_len, params.pswd,
            params.site_len, params.site
        );
    }
    else
    {
        ESP_LOGI(TAG, "No stored params");
    }

    nvs_close(h_nvs);

    return result;
}

//-------------------------------------------------------------------------------------------------

static void wifi_SaveParams(wifi_conn_params_t * p_params)
{
    nvs_handle h_nvs  = 0;
    esp_err_t  status = ESP_OK;

    status = nvs_flash_init();
    ESP_ERROR_CHECK(status);

    status = nvs_open("wifi", NVS_READWRITE, &h_nvs);
    ESP_ERROR_CHECK(status);

    status = nvs_set_str(h_nvs, "ssid", p_params->ssid);
    ESP_ERROR_CHECK(status);

    status = nvs_set_str(h_nvs, "pswd", p_params->pswd);
    ESP_ERROR_CHECK(status);

    status = nvs_set_str(h_nvs, "site", p_params->site);
    ESP_ERROR_CHECK(status);

    ESP_LOGI
    (
        TAG, "Stored params:\n  - SSID:%d:%s\n  - PSWD:%d:%s\n  - SITE:%d:%s",
        p_params->ssid_len, p_params->ssid,
        p_params->pswd_len, p_params->pswd,
        p_params->site_len, p_params->site
    );

    nvs_close(h_nvs);
}

//-------------------------------------------------------------------------------------------------

static void wifi_ClearParams(void)
{
    nvs_handle h_nvs  = 0;
    esp_err_t  status = ESP_OK;

    status = nvs_flash_init();
    ESP_ERROR_CHECK(status);

    status = nvs_open("wifi", NVS_READWRITE, &h_nvs);
    ESP_ERROR_CHECK(status);

    status = nvs_erase_key(h_nvs, "ssid");
    ESP_ERROR_CHECK(status);

    status = nvs_erase_key(h_nvs, "pswd");
    ESP_ERROR_CHECK(status);

    status = nvs_erase_key(h_nvs, "site");
    ESP_ERROR_CHECK(status);

    ESP_LOGI(TAG, "Cleared params");

    nvs_close(h_nvs);
}

//-------------------------------------------------------------------------------------------------

extern void user_init(void);

static void wifi_Task(void * pvParams)
{
    wifi_Start();

    while (FW_TRUE)
    {
        if (FW_TRUE == wifi_Connect())
        {
            user_init();
//            UDP_NotifyWiFiIsConnected(gIpAddr);
            wifi_WaitForDisconnect();
        }
//        UDP_NotifyWiFiIsDisconnected();
        vTaskDelay(10000 / portTICK_RATE_MS);
    }
}

//-------------------------------------------------------------------------------------------------

void WIFI_Task_Init(void)
{
    /* Load WiFi parameters */
    if (false == wifi_LoadParams(&gWiFiConnParams))
    {
//        memcpy(gWiFiConnParams.ssid, WIFI_SSID, sizeof(WIFI_SSID));
//        gWiFiConnParams.ssid_len = strlen(WIFI_SSID);
//        memcpy(gWiFiConnParams.pswd, WIFI_PSWD, sizeof(WIFI_PSWD));
//        gWiFiConnParams.pswd_len = strlen(WIFI_PSWD);
//        memcpy(gWiFiConnParams.site, WIFI_SITE, sizeof(WIFI_SITE));
//        gWiFiConnParams.pswd_len = strlen(WIFI_PSWD);
//
//        wifi_ClearParams();
    }

    /* Create the events group for WiFi task */
    gWiFiEvents = xEventGroupCreate();

    //xTaskCreate(wifi_Task, "WiFi", 4096, NULL, 5, NULL);
}

//-------------------------------------------------------------------------------------------------
