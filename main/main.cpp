extern "C" {
    void app_main(void);
}

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "webhookIF.hpp"

#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "testbench";

extern const uint8_t root_cert_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t root_cert_end[] asm("_binary_server_root_cert_pem_end");

static const char *webhook_url = "https://discord.com/api/webhooks/719008915194642443/Ubp9z0s0CxoC2z5gd6FboaOPB1iDpqPE9mJU51oyrQUqDFRmtHgSl-WXwGc-xsp4Ncbq";

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 3) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGE(TAG, "wifi disconnected");
    }
}

void app_main(void)
{
    char errRes[128];
    mbedtls_strerror(-0x2700, errRes, 128);
    ESP_LOGI(TAG, "mbed_tls err %s", errRes);
    mbedtls_strerror(-0x50, errRes, 128); // unknown error code
    ESP_LOGI(TAG, "mbed_tls err %s", errRes);
    webhookIF discord(webhook_url, root_cert_start, root_cert_end);

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = { 0 };
    strcpy((char *)wifi_config.sta.ssid, "TELUSWiFi3437");
    strcpy((char *)wifi_config.sta.password, "x3ufKAV9Xg");
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect");
        return;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }


    char msg1[32] = "msg1";
    char msg2[32] = "msg2";
    char msg3[32] = "msg3";
    char msg4[32] = "msg4";
    int numMessages = 0;
    uint64_t t_start = esp_timer_get_time();
    uint64_t t_finish;
    while(1){
        esp_err_t err = discord.send_message_print_response(msg4, strlen(msg4));
        t_finish = esp_timer_get_time();
        if(err != ESP_OK){
            ESP_LOGE(TAG, "send message failed %d", err);
            break;
        }
        numMessages++;
        ESP_LOGI(TAG, "ms/message %llu", (t_finish - t_start) / 1000 / numMessages);
        //ESP_LOGI(TAG, "free heap size %d", xPortGetFreeHeapSize());
        //vTaskDelay(60000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "Number of successful messages: %d", numMessages);
}