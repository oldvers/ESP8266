#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_clk.h"
#include "esp_log.h"

#include "east_task.h"
#include "wifi_task.h"
#include "udp_task.h"
#include "udp_dns_server.h"
#include "led_task.h"
#include "time_task.h"

//-------------------------------------------------------------------------------------------------

#define MAIN_LOG  1

#if (1 == MAIN_LOG)
static const char * gTAG = "MAIN";
#    define MAIN_LOGI(...)  ESP_LOGI(gTAG, __VA_ARGS__)
#    define MAIN_LOGE(...)  ESP_LOGE(gTAG, __VA_ARGS__)
#    define MAIN_LOGV(...)  ESP_LOGV(gTAG, __VA_ARGS__)
#else
#    define MAIN_LOGI(...)
#    define MAIN_LOGE(...)
#    define MAIN_LOGV(...)
#endif

//-------------------------------------------------------------------------------------------------

static void print_info(void)
{
    esp_chip_info_t chip_info = {0};
    char *          string    = NULL;
    uint32_t        value     = 0;
    uint8_t         mac[6]    = {0};
    esp_err_t       error     = ESP_OK;

    /* Retrieve the chip info */
    esp_chip_info(&chip_info);

    /* Retrieve the default MAC address */
    error = esp_efuse_mac_get_default(mac);
    ESP_ERROR_CHECK(error);

    MAIN_LOGI("*");
    MAIN_LOGI("-------------------------------------------------");
    MAIN_LOGI("ESP8266 chip with %d CPU cores and WiFi", chip_info.cores);
    MAIN_LOGI("Silicon revision %d", chip_info.revision);
    if (chip_info.features & CHIP_FEATURE_EMB_FLASH)
    {
        string = "embedded";
    }
    else
    {
        string = "external";
    }
    value = spi_flash_get_chip_size() / (1024 * 1024);
    MAIN_LOGI("Flash %d MB : %s", value, string);
    value = esp_clk_cpu_freq();
    MAIN_LOGI("CPU Frequency %d Hz", value);
    MAIN_LOGI("MAC %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

//-------------------------------------------------------------------------------------------------

void app_main()
{
    /* Print chip information */
    print_info();

    /* Initialize the tasks */
    //EAST_Task_Init();
    LED_Task_Init();
    WiFi_Task_Init();
    Time_Task_Init();

    time_message_t msg = {TIME_CMD_SUN_ENABLE};
    Time_Task_SendMsg(&msg);
}

//-------------------------------------------------------------------------------------------------
