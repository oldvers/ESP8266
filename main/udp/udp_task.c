#include <string.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "types.h"

//-------------------------------------------------------------------------------------------------

#define PORT                     (3333)
#define EVT_WIFI_CONNECTED       BIT0
#define EVT_WIFI_DISCONNECTED    BIT1

//-------------------------------------------------------------------------------------------------

static const char *TAG = "UDP";
static EventGroupHandle_t gUdpEvents = NULL;

//-------------------------------------------------------------------------------------------------

static EventBits_t vWiFi_WaitFor(EventBits_t events, TickType_t timeout)
{
    EventBits_t bits = 0;

    /* Waiting until either specified event is set */
    bits = xEventGroupWaitBits
           (
               gUdpEvents,
               events,
               pdTRUE,
               pdFALSE,
               timeout / portTICK_RATE_MS
           );

    return bits;
}

//-------------------------------------------------------------------------------------------------

static void vUDP_Task(void * pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    while (FW_TRUE)
    {
        ESP_LOGI(TAG, "Waiting for WiFi connection...");
        (void)vWiFi_WaitFor(EVT_WIFI_CONNECTED, portMAX_DELAY);

        struct sockaddr_in destAddr;
        destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        int err = bind(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(TAG, "Socket binded");

        while (FW_TRUE)
        {
            ESP_LOGI(TAG, "Waiting for data");

            struct sockaddr_in sourceAddr;

            socklen_t socklen = sizeof(sourceAddr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&sourceAddr, &socklen);

            // Error occured during receiving
            if (len < 0)
            {
                ESP_LOGE(TAG, "Receiving failed: errno %d", errno);
                break;
            }
            // Data received
            else
            {
                // Get the sender's ip address as string
                inet_ntoa_r(((struct sockaddr_in *)&sourceAddr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);

                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG, "%s", rx_buffer);

                int err = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&sourceAddr, sizeof(sourceAddr));
                if (err < 0)
                {
                    ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
                    break;
                }
            }
        }

        if (sock != -1)
        {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

//-------------------------------------------------------------------------------------------------

void UDP_NotifyWiFiIsConnected(void)
{
    xEventGroupSetBits(gUdpEvents, EVT_WIFI_CONNECTED);
}

//-------------------------------------------------------------------------------------------------

void UDP_Task_Init(void)
{
    /* Create the events group for UDP task */
    gUdpEvents = xEventGroupCreate();

    xTaskCreate(vUDP_Task, "UDP", 4096, NULL, 5, NULL);
}
