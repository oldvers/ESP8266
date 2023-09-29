/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_clk.h"

#include "east_task.h"
#include "wifi_task.h"
#include "udp_task.h"
#include "udp_dns_server.h"

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

    printf("\n\n\n--- ESP8266 chip with %d CPU cores and WiFi ---\n", chip_info.cores);
    printf("  - Silicon revision %d\n", chip_info.revision);
    if (chip_info.features & CHIP_FEATURE_EMB_FLASH)
    {
        string = "embedded";
    }
    else
    {
        string = "external";
    }
    value = spi_flash_get_chip_size() / (1024 * 1024);
    printf("  - Flash %d MB : %s\n", value, string);
    value = esp_clk_cpu_freq() / (1024 * 1024);
    printf("  - CPU Frequency %d MHz\n", value);
    printf("  - MAC %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    fflush(stdout);
}

void app_main()
{
    /* Print chip information */
    print_info();

    /* Initialize the tasks */
    //EAST_Task_Init();
    WIFI_Task_Init();
}
