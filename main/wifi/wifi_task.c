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

#define WIFI_SSID                    "HomeWLAN"
#define WIFI_PSWD                    "wlanH020785endrix!"
#define WIFI_SITE                    "home.com"
#define EVT_WIFI_ST_STARTED          BIT0
#define EVT_WIFI_ST_CONNECTED        BIT1
#define EVT_WIFI_ST_GOT_IP           BIT2
#define EVT_WIFI_ST_DISCONNECTED     BIT3
#define EVT_WIFI_AP_ST_CONNECTED     BIT4
#define EVT_WIFI_AP_ST_DISCONNECTED  BIT5

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

static void wifi_ST_OnStarted
(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
)
{
    ESP_LOGI(TAG, "ST Started!");
    xEventGroupSetBits(gWiFiEvents, EVT_WIFI_ST_STARTED);
}

//-------------------------------------------------------------------------------------------------

static void wifi_ST_OnConnected
(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
)
{
    ESP_LOGI(TAG, "ST Connected to \"%s\"", WIFI_SSID);
    xEventGroupSetBits(gWiFiEvents, EVT_WIFI_ST_CONNECTED);
}

//-------------------------------------------------------------------------------------------------

static void wifi_ST_OnGotIp
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

    ESP_LOGI(TAG, "ST Got IP : %s", ip4addr_ntoa(&event->ip_info.ip));
    xEventGroupSetBits(gWiFiEvents, EVT_WIFI_ST_GOT_IP);
}    

//-------------------------------------------------------------------------------------------------

static void wifi_ST_OnDisconnected
(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
)
{ 
    ESP_LOGE(TAG, "ST Disconnected!");
    xEventGroupSetBits(gWiFiEvents, EVT_WIFI_ST_DISCONNECTED);
}

//-------------------------------------------------------------------------------------------------

static void wifi_AP_OnStConnected
(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
)
{
    wifi_event_ap_staconnected_t * event = (wifi_event_ap_staconnected_t *)event_data;

    ESP_LOGE(TAG, "AP - ST Connected! "MACSTR", AID=%d", MAC2STR(event->mac), event->aid);
    xEventGroupSetBits(gWiFiEvents, EVT_WIFI_AP_ST_CONNECTED);
}

//-------------------------------------------------------------------------------------------------

static void wifi_AP_OnStDisconnected
(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
)
{
    wifi_event_ap_stadisconnected_t * event = (wifi_event_ap_stadisconnected_t *)event_data;

    ESP_LOGE(TAG, "AP - ST Disconnected! "MACSTR", AID=%d", MAC2STR(event->mac), event->aid);
    xEventGroupSetBits(gWiFiEvents, EVT_WIFI_AP_ST_DISCONNECTED);
}

//-------------------------------------------------------------------------------------------------

static void wifi_RegisterHandlers(void)
{
    HNDLR_REG
    (
        WIFI_EVENT,
        WIFI_EVENT_STA_START,
        &wifi_ST_OnStarted
    );
    HNDLR_REG
    (
        WIFI_EVENT,
        WIFI_EVENT_STA_CONNECTED,
        &wifi_ST_OnConnected
    );
    HNDLR_REG
    (
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_ST_OnGotIp
    );
    HNDLR_REG
    (
        WIFI_EVENT,
        WIFI_EVENT_STA_DISCONNECTED,
        &wifi_ST_OnDisconnected
    );
    HNDLR_REG
    (
        WIFI_EVENT,
        WIFI_EVENT_AP_STACONNECTED,
        &wifi_AP_OnStConnected
    );
    HNDLR_REG
    (
        WIFI_EVENT,
        WIFI_EVENT_AP_STADISCONNECTED,
        &wifi_AP_OnStDisconnected
    );
}

//-------------------------------------------------------------------------------------------------

static void wifi_Start(void)
{
    wifi_config_t wifi_config = {0};
    uint8_t       mac[6]      = {0};
    char          ap_ssid[32] = {0};
    int           length      = 0;

    /* Prepare the events loop */
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Prepare the default configuration for WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    /* Register event handler */
    wifi_RegisterHandlers();

    /* Prepare the WiFi parameters. Temporary in RAM */
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    /* Station mode */
    if (true == gWiFiConnParams.valid)
    {
        //wifi_config_t wifi_config =
        //{
        //    .sta =
        //    {
        //        .ssid = WIFI_SSID,
        //        .password = WIFI_PSWD
        //    },
        //};
        memcpy(wifi_config.sta.ssid, gWiFiConnParams.ssid, gWiFiConnParams.ssid_len);
        memcpy(wifi_config.sta.password, gWiFiConnParams.pswd, gWiFiConnParams.pswd_len);

        ESP_LOGI(TAG, "ST Starting...");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        (void)wifi_WaitFor(EVT_WIFI_ST_STARTED, portMAX_DELAY);

        ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, "testing"));
    }
    else
    /* Access Point mode */
    {
        //wifi_config_t wifi_config =
        //{
        //    .ap =
        //    {
        //        .ssid = EXAMPLE_ESP_WIFI_SSID,
        //        .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
        //        .password = EXAMPLE_ESP_WIFI_PASS,
        //        .max_connection = EXAMPLE_MAX_STA_CONN,
        //        .authmode = WIFI_AUTH_WPA_WPA2_PSK
        //    },
        //};
        ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
        length = sprintf(ap_ssid, "WIFI_%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        memcpy(wifi_config.ap.ssid, ap_ssid, length);
        wifi_config.ap.ssid_len = length;
        strcpy((char *)wifi_config.ap.password, "0123456789");
        wifi_config.ap.max_connection = 1;
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

        ESP_LOGI(TAG, "AP \"%s\" Starting...", ap_ssid);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, "testing"));
    }
}

//-------------------------------------------------------------------------------------------------

static FW_BOOLEAN wifi_Connect(void)
{
    EventBits_t events = 0;
    FW_BOOLEAN result = FW_FALSE;

    /* Station mode */
    if (true == gWiFiConnParams.valid)
    {
        /* Connect */
        ESP_LOGI(TAG, "Connecting to \"%s\"...", WIFI_SSID);
        ESP_ERROR_CHECK(esp_wifi_connect());
    
        /* Wait for connection */
        events = wifi_WaitFor(EVT_WIFI_ST_DISCONNECTED | EVT_WIFI_ST_GOT_IP, 10000);
        if (0 != (events & EVT_WIFI_ST_GOT_IP))
        {
            ESP_LOGI(TAG, "Connected successfuly");
            result = FW_TRUE;
        }
        else if (0 != (events & EVT_WIFI_ST_DISCONNECTED))
        {
            ESP_LOGE(TAG, "Reconnection needed");
        }
        else
        {
            ESP_LOGE(TAG, "Something wrong! Reconnection needed");
        }
    }
    else
    /* Access Point mode */
    {
        /* Wait for connection */
        events = wifi_WaitFor(EVT_WIFI_AP_ST_CONNECTED, 10000);
        if (0 != (events & EVT_WIFI_AP_ST_CONNECTED))
        {
            ESP_LOGI(TAG, "Connected successfuly");
            result = FW_TRUE;
        }
        else
        {
            ESP_LOGE(TAG, "Something wrong! Reconnection needed");
        }
    }

    return result;
}

//-------------------------------------------------------------------------------------------------

static void wifi_WaitForDisconnect(void)
{
    /* Station mode */
    if (true == gWiFiConnParams.valid)
    {
        (void)wifi_WaitFor(EVT_WIFI_ST_DISCONNECTED, portMAX_DELAY);
    }
    else
    /* Access Point mode */
    {
        (void)wifi_WaitFor(EVT_WIFI_AP_ST_DISCONNECTED, portMAX_DELAY);
    }
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
//            user_init();
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

    xTaskCreate(wifi_Task, "WiFi", 4096, NULL, 5, NULL);
}

//-------------------------------------------------------------------------------------------------
