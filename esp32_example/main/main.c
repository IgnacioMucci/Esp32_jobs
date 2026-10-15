/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <stdint.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <poma.h>
#include "esp_sleep.h"
#include <string.h>
#include <string_check.h>

#define PORT CONFIG_EXAMPLE_PORT

#define NVS_NAMESPACE "poma"
#define NVS_KEY_PERIOD "period"


static const char *TAG = "example";

int GlobalVar = 0;
uint64_t time_deepsleep = 0;
Topic *topicHead;

void setterGlobalVar(int sockfd, char *argument)
{
    if (argument != NULL)
        GlobalVar = atoi(argument);
    write(sockfd, "done\n", 6);
}

void getterGlobalVar(int sockfd, char *argument)
{
    char response[10];
    sprintf(response, "%d\n", GlobalVar);
    write(sockfd, response, strlen(response));
}

// Reads the value stored;if it doesnt exits uses the default
static esp_err_t period_load(uint64_t *out, uint64_t default_val)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {  //namespace still doesnt exists
        *out = default_val;
        return err;
    }
    err = nvs_get_u64(h, NVS_KEY_PERIOD, out);
    if (err != ESP_OK) {    // Didnt find the value
        *out = default_val;
    }
    nvs_close(h);
    return err;
}

// Changes the value if the value is diferent
static esp_err_t period_save(uint64_t value)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    uint64_t stored;
    if (nvs_get_u64(h, NVS_KEY_PERIOD, &stored) == ESP_OK && stored == value) { // IF the value that im trying to store is the same that was stored
        nvs_close(h);
        return ESP_OK;                // I dont change it
    }
    err = nvs_set_u64(h, NVS_KEY_PERIOD, value); // changes the value
    if (err == ESP_OK) err = nvs_commit(h); // Puts it in the flash
    nvs_close(h);
    return err;
}

void setterPeriod(int sockfd, char *argument)
{   
    char response[25];
    uint32_t seconds;

   if (check_String(argument, &seconds, response) == ESP_OK) {
     time_deepsleep = seconds * 1000000ULL; // changes from secons to us
     period_save(time_deepsleep);
     sprintf(response, "done\n");   
    }    
    write(sockfd, response, strlen(response));
}

void getterPeriod(int sockfd, char *argument)
{
    char response[25];
    uint64_t period_seconds = time_deepsleep / 1000000ULL; // Chaging from us to s
    sprintf(response, "%llu\n", (unsigned long long)period_seconds);
    write(sockfd, response, strlen(response));
}



void setterDeepSleep(int sockfd, char *argument)
{   
    char response[25];
    uint32_t seconds; 
        if( argument != NULL && strcmp(argument, "0") == 0){
        uint64_t time_us;    
        period_load(&time_us, 0); // Going to nvs to search for the value
        sprintf(response, "Sleeping\n");    
        write(sockfd, response, strlen(response));
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_sleep_enable_timer_wakeup(time_us);// Configure the time to sleep
        esp_deep_sleep_start();// start sleeping
        return;
        } 
        if (check_String(argument, &seconds, response) != ESP_OK) {
        write(sockfd, response, strlen(response));
        return;
    }
    sprintf(response, "Sleeping\n");
    write(sockfd, response, strlen(response));
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);  // s -> us
    esp_deep_sleep_start(); 
}

static void do_processQuery(const int sock)
{
    int len;
    char rx_buffer[128];

    do
    {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0)
        {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        }
        else if (len == 1)
        {
            ESP_LOGW(TAG, "Connection closed");
        }
        else
        {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

            processMessage(sock, rx_buffer, topicHead);
        }
    } while (len > 0);
}

static void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_in6 dest_addr;

    if (addr_family == AF_INET)
    {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
    }
    else if (addr_family == AF_INET6)
    {
        bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(PORT);
        ip_protocol = IPPROTO_IPV6;
    }

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
#if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
    // Note that by default IPV6 binds to both protocols, it is must be disabled
    // if both protocols used at the same time (used in CI)
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0)
    {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1)
    {

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Convert ip address to string
        if (source_addr.sin6_family == PF_INET)
        {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
        }
        else if (source_addr.sin6_family == PF_INET6)
        {
            inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        // do_retransmit(sock);
        do_processQuery(sock);

        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    period_load(&time_deepsleep,0);
    topicHead = createTopic("GlobalVar", getterGlobalVar, setterGlobalVar);
    addTopic(topicHead, createTopic("Period", getterPeriod, setterPeriod));
    addTopic(topicHead, createTopic("DeepSleep", NULL, setterDeepSleep));

#ifdef CONFIG_EXAMPLE_IPV4
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void *)AF_INET, 5, NULL);
#endif
#ifdef CONFIG_EXAMPLE_IPV6
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void *)AF_INET6, 5, NULL);
#endif
}
