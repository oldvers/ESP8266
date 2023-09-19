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
#include "lwip/netdb.h"

#include "types.h"
#include "led_strip.h"

//-------------------------------------------------------------------------------------------------

#define PORT                     3333
#define EVT_WIFI_CONNECTED       BIT0

//-------------------------------------------------------------------------------------------------

typedef struct
{
    uint16_t xmark;
    uint16_t mark;
    uint32_t ip;
    uint16_t port;
    uint16_t cmark;
} udp_packet_pingpong_t;

typedef struct
{
    uint8_t type;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} udp_packet_set_color_t;

//-------------------------------------------------------------------------------------------------

static const char *       TAG              = "UDP";
static EventGroupHandle_t gUdpEvents       = NULL;
static uint32_t           gIpAddr          = 0;
static uint8_t            gUdpBuffer[1024] = {0};
static uint8_t            gLedStrip[16*3]  = {0};

//-------------------------------------------------------------------------------------------------

static EventBits_t udp_WaitFor(EventBits_t events, TickType_t timeout)
{
    EventBits_t bits = 0;

    /* Waiting until either specified event is set */
    bits = xEventGroupWaitBits
           (
               gUdpEvents,
               events,       /* Bits To Wait For */
               pdFALSE,      /* Clear On Exit */
               pdFALSE,      /* Wait For All Bits */
               timeout / portTICK_RATE_MS
           );

    return bits;
}

//-------------------------------------------------------------------------------------------------

static uint32_t udp_Connect(void)
{
    struct sockaddr_in cltAddr;
    struct sockaddr_in svrAddr;
    struct timeval timeouts;
    socklen_t socklen;
    udp_packet_pingpong_t packet = {0};
    char addr_str[16];
    int addr_family;
    int ip_protocol;
    int len = 0;
    int res = 0;

    /* Create the socket */
    cltAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    cltAddr.sin_family = AF_INET;
    cltAddr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(cltAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

    ESP_LOGI(TAG, "Creating socket, IP: %s, %08X", addr_str, ((struct sockaddr_in *)&cltAddr)->sin_addr.s_addr);

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return res;
    }
    ESP_LOGI(TAG, "Socket created");

    /* Set timeouts */
    timeouts.tv_sec = 1;
    timeouts.tv_usec = 0;
    int err = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeouts, sizeof(timeouts));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Unable to set socket timeouts: errno %d", errno);
        closesocket(sock);
        return res;
    }

    err = bind(sock, (struct sockaddr *)&cltAddr, sizeof(cltAddr));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    }
    ESP_LOGI(TAG, "Socket binded");

    while (FW_TRUE)
    {
        packet.mark = 0x1234;
        packet.xmark = (packet.mark ^ 0xFFFF);
        packet.cmark = packet.mark;
        packet.ip = gIpAddr;
        packet.port = PORT;
        len = sizeof(packet);

        svrAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        svrAddr.sin_family = AF_INET;
        svrAddr.sin_port = htons(PORT);

        int err = sendto(sock, (uint8_t *)&packet, len, 0, (struct sockaddr *)&svrAddr, sizeof(svrAddr));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Sent %d bytes", len);

        socklen = sizeof(svrAddr);
        len = recvfrom(sock, (uint8_t *)&packet, sizeof(packet), 0, (struct sockaddr *)&svrAddr, &socklen);
        ESP_LOGI(TAG, "Rcvd %d bytes", len);
        if ((sizeof(packet) == len) && (0x4321 == packet.mark))
        {
            if ((packet.mark == (packet.xmark ^ 0xFFFF)) && (packet.mark == packet.cmark))
            {
                // Get the sender's ip address as string
                inet_ntoa_r(((struct sockaddr_in *)&svrAddr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);

                ESP_LOGI(TAG, "Received %d bytes from %s, %08X", len, addr_str, ((struct sockaddr_in *)&svrAddr)->sin_addr.s_addr);

                res = ((struct sockaddr_in *)&svrAddr)->sin_addr.s_addr;

                break;
            }
        }
    }

    if (sock != -1)
    {
        ESP_LOGI(TAG, "Shutting down socket...");
        closesocket(sock);
    }

    return res;
}

//-------------------------------------------------------------------------------------------------

static void vUDP_Task(void * pvParameters)
{
    struct sockaddr_in svrAddr;
    struct sockaddr_in cltAddr;
    char addr_str[128];
    int addr_family;
    int ip_protocol;
    uint32_t ip;
//    TickType_t xLastWakeTime;
//    const TickType_t xPeriod = 20;
//    int len = 0;
    struct timeval timeouts;
//    socklen_t socklen;

    while (FW_TRUE)
    {
        ESP_LOGI(TAG, "Waiting for WiFi connection...");
        (void)udp_WaitFor(EVT_WIFI_CONNECTED, portMAX_DELAY);

        ip = udp_Connect();
        if (0 != ip)
        {
            ESP_LOGI(TAG, "Server IP found: %08X", ip);
        }

        //struct sockaddr_in destAddr;
        //destAddr.sin_addr.s_addr = ip;
        //destAddr.sin_family = AF_INET;
        //destAddr.sin_port = htons(PORT);
        //addr_family = AF_INET;
        //ip_protocol = IPPROTO_IP;
        //inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

        /* Create the socket */
        svrAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        svrAddr.sin_family = AF_INET;
        svrAddr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(svrAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

        ESP_LOGI(TAG, "Creating socket, IP: %s, %08X", addr_str, ((struct sockaddr_in *)&svrAddr)->sin_addr.s_addr);

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");


        /* Set timeouts */
        timeouts.tv_sec = 1;
        timeouts.tv_usec = 0;
        int err = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeouts, sizeof(timeouts));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Unable to set socket timeouts: errno %d", errno);
            closesocket(sock);
            break;
        }

        err = bind(sock, (struct sockaddr *)&svrAddr, sizeof(svrAddr));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket binded");


//        xLastWakeTime = xTaskGetTickCount();
        while (FW_TRUE)
        {
//            len = udp_GetSensorValues();
//            ESP_LOGI(TAG, "Len = %d", len);
//            if (0 < len)
//            {
//                int err = sendto(sock, gUdpBuffer, len, 0, (struct sockaddr *)&destAddr, sizeof(destAddr));
//                if (err < 0)
//                {
//                    ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
//                    break;
//                }
//            }

//            socklen = sizeof(destAddr);
//            len = recvfrom(sock, gUdpBuffer, sizeof(gUdpBuffer), 0, (struct sockaddr *)&destAddr, &socklen);
//            if (0 < len)
//            {
//                ESP_LOGI(TAG, "Rcvd %d bytes: %08X %08X", len, gUdpBuffer[0], gUdpBuffer[1]);
//            }




            //ESP_LOGI(TAG, "Waiting for data");

            socklen_t socklen = sizeof(cltAddr);
            int len = recvfrom(sock, gUdpBuffer, sizeof(gUdpBuffer) - 1, 0, (struct sockaddr *)&cltAddr, &socklen);

            // Error occured during receiving
            //if (len < 0)
            //{
            //    ESP_LOGE(TAG, "Receiving failed: errno %d", errno);
                //break;
            //}
            // Data received
            //else

            udp_packet_set_color_t * packet = (udp_packet_set_color_t *)gUdpBuffer;
            if ((sizeof(udp_packet_set_color_t) == len) && (0 == packet->type))
            {
                // Get the sender's ip address as string
                inet_ntoa_r(((struct sockaddr_in *)&cltAddr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);

                ESP_LOGI(TAG, "Received %d bytes from %s", len, addr_str);

                for (uint8_t idx = 0; idx < sizeof(gLedStrip); idx += 3)
                {
                    gLedStrip[idx + 0] = packet->r;
                    gLedStrip[idx + 1] = packet->g;
                    gLedStrip[idx + 2] = packet->b;
                }
                LED_Strip_Update();
            }

            //if (0 < len)
            //{
            //    // Get the sender's ip address as string
            //    inet_ntoa_r(((struct sockaddr_in *)&cltAddr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
//
            //    //gUdpBuffer[len] = 0; // Null-terminate whatever we received and treat like a string...
            //    ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
            //    //ESP_LOGI(TAG, "%s", rx_buffer);
            //    ESP_LOGI(TAG, "Rcvd %d bytes: %02X %02X", len, gUdpBuffer[0], gUdpBuffer[1]);
//
            //    //int err = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&sourceAddr, sizeof(sourceAddr));
            //    //if (err < 0)
            //    //{
            //    //    ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
            //    //    break;
            //    //}
            //}




//            vTaskDelayUntil(&xLastWakeTime, xPeriod);
        }

        if (sock != -1)
        {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            closesocket(sock);
        }
    }
    vTaskDelete(NULL);
}

//-------------------------------------------------------------------------------------------------

void UDP_NotifyWiFiIsConnected(uint32_t ip)
{
    gIpAddr = ip;

    xEventGroupSetBits(gUdpEvents, EVT_WIFI_CONNECTED);
}

//-------------------------------------------------------------------------------------------------

void UDP_NotifyWiFiIsDisconnected(void)
{
    xEventGroupClearBits(gUdpEvents, EVT_WIFI_CONNECTED);

    gIpAddr = 0;
}

//-------------------------------------------------------------------------------------------------

void UDP_Task_Init(void)
{
    /* Create the events group for UDP task */
    gUdpEvents = xEventGroupCreate();

    LED_Strip_Init(gLedStrip, sizeof(gLedStrip));

    xTaskCreate(vUDP_Task, "UDP", 4096, NULL, 5, NULL);
}

//-------------------------------------------------------------------------------------------------

