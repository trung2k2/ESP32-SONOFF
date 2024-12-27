#include "app_config.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/etharp.h"
#include "lwip/err.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/ip4.h"
#include "netif/ethernet.h"
#include "esp_netif.h"
#include "lwip/sys.h"
#include "esp_netif_net_stack.h"
#include "driver/gpio.h"
#include "lwip/ip_addr.h"
#include "esp_smartconfig.h"
#include "esp_mac.h"
#include "app_http_server.c"


//provision_type_t provisition_type = PROVISION_SMARTCONFIG; //nếu dùng smartconfig
provision_type_t provisition_type = PROVISION_ACCESSPOINT; //nếu dùng access point
static EventGroupHandle_t s_wifi_event_group;

static const int WIFI_CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const int HTTP_CONFIG_DONE = BIT2;

        char ssid[33] = { 0 };
        char password[65] = { 0 };

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        //xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;

        uint8_t rvd_data[33] = { 0 };

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }


        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
            ESP_LOGI(TAG, "RVD_DATA:");
            for (int i=0; i<33; i++) {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }

        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }

}

bool is_provisioned(void) //hàm check thông tin wifi xem có chưa
{
    bool provision = false;
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap(); //dăng ký event
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );

    wifi_config_t wifi_config;
    esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
    if(wifi_config.sta.ssid[0] != 0x00) //đã có cau hình
    {
        provision = true;
    }
    return provision;
}

void ap_start(void)
{
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "daubuoi2",
            .ssid_len = strlen("daubuoi2"),
            .channel = 1,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK //chuan bao mat
            },
        };
    if (wifi_config.ap.password[0] == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);

}

void wifi_data_callback(char *data, int len)
{
    //char ssid[30] = "";
    //char pass[30] = "";
    printf("->data: %s\n", data);
    char *pt = strtok(data, "@");
    if(pt)
    strcpy(ssid, pt);
    pt = strtok(NULL, "@");
    if(pt)
    strcpy(password, pt);
    printf("->ssid: %s\n", ssid);
    printf("->pass: %s\n", password);
    xEventGroupSetBits(s_wifi_event_group, HTTP_CONFIG_DONE);
    ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);
}

void app_config(void) 
{
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    s_wifi_event_group = xEventGroupCreate();
    bool provisioned = is_provisioned();
    if(!provisioned) //chua co thi bật mode provision lên, chạy một trong hai chế độ
    {
        if(provisition_type == PROVISION_SMARTCONFIG)
        {
            ESP_ERROR_CHECK(esp_wifi_start() );
            ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
            smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
            ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );    
            xEventGroupWaitBits(s_wifi_event_group, ESPTOUCH_DONE_BIT, false, true, portMAX_DELAY);//đợi set bit max thời gian
            esp_smartconfig_stop();       

        }
        else if(provisition_type == PROVISION_ACCESSPOINT)
        {
            ap_start();
            start_webserver();
            http_set_callback_wifiinfo(wifi_data_callback);
            xEventGroupWaitBits(s_wifi_event_group, HTTP_CONFIG_DONE, false, true, portMAX_DELAY);
            // cenvert station mode and connect to router
            stop_webserver();
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            wifi_config_t wifi_config;
            bzero(&wifi_config, sizeof(wifi_config_t));
            memcpy(wifi_config.sta.ssid, ssid, sizeof(ssid));
            memcpy(wifi_config.sta.password, password, sizeof(password));   

            ESP_ERROR_CHECK(esp_wifi_init(&cfg));
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
            ESP_ERROR_CHECK(esp_wifi_start());
            start_webserver();
        }
    
    }
    else{ //neu co thi start
        ESP_ERROR_CHECK(esp_wifi_start() );
    }
    xEventGroupWaitBits(s_wifi_event_group, ESPTOUCH_DONE_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "wifi connected");
}