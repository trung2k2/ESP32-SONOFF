#include "app_http_server.h"

/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#if !CONFIG_IDF_TARGET_LINUX
#include <esp_wifi.h>
#include "esp_event.h"
#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include <esp_http_server.h>
#include "esp_netif.h"
#include "esp_tls.h"
#endif  // !CONFIG_IDF_TARGET_LINUX

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN  (64)
static httpd_handle_t server = NULL;
/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

static const char *TAG = "HTTP_SERVER";



extern const uint8_t anh_start[] asm("_binary_anhmau_jpeg_start");
extern const uint8_t anh_end[] asm("_binary_anhmau_jpeg_end");
// extern const uint8_t index_html_start[] asm("_binary_index_html_start");
// extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t index2_html_start[] asm("_binary_index2_html_start");
extern const uint8_t index2_html_end[] asm("_binary_index2_html_end");
extern const uint8_t index3_html_start[] asm("_binary_index3_html_start");
extern const uint8_t index3_html_end[] asm("_binary_index3_html_end");

static http_post_callback_t http_post_switch_callback = NULL;
static http_get_callback_t http_get_dht11_callback = NULL;
static http_post_callback_t http_post_slider_callback = NULL;
static http_post_callback_t http_post_wifi_info_callback = NULL;

// static bool check_login(httpd_req_t *req) { //kiem tra dang nhap cookie
//     char session_token[32] = {0};
//     size_t token_len = httpd_req_get_hdr_value_len(req, "Cookie") + 1;

//     // Kiểm tra nếu có cookie hay không
//     if (token_len > 1) {
//         httpd_req_get_hdr_value_str(req, "Cookie", session_token, token_len);
    
//     // Kiểm tra xem cookie có chứa session_token không
//     if (strstr(session_token, "session_token=123456") != NULL) {
//         return true; // Đã đăng nhập
//     }
//     }
//     return false; // Chưa đăng nhập
// }

// bool is_logged_in = false;

static esp_err_t sign_post_handler(httpd_req_t *req)
{
    int login_successful = 0;
    char buf[100];
    int ret, remaining = req->content_len;

    // Nhận dữ liệu POST
    if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) < 0) {
        // Xử lý lỗi khi nhận dữ liệu
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);  // Trả về lỗi 408 nếu timeout
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0'; // Kết thúc chuỗi dữ liệu

    printf("DATA: %s\n", buf);  // In ra dữ liệu đã nhận

    // Kiểm tra thông tin đăng nhập từ dữ liệu POST
    const char correct_username[] = "admin";
    const char correct_password[] = "123456";

    // Giả sử dữ liệu được gửi là dạng `username=admin&password=123456`
    char *username = strstr(buf, "username=");
    char *password = strstr(buf, "password=");

    if (username && password) {
        username += 9; // Bỏ qua "username=" để lấy giá trị
        password += 9; // Bỏ qua "password=" để lấy giá trị

        // Xử lý username và password từ `buf`
        char *username_end = strchr(username, '&');
        if (username_end) *username_end = '\0'; // Kết thúc chuỗi username

        // Kiểm tra nếu username và password khớp
        if (strcmp(username, correct_username) == 0 && strcmp(password, correct_password) == 0) {
            login_successful = 1;
        }
    }

    if (login_successful) {
        // Gửi token trong cookie
        httpd_resp_set_hdr(req, "Set-Cookie", "session_token=123456; Path=/; HttpOnly");

        // Trả về phản hồi đăng nhập thành công
        const char *resp = "{\"success\": true}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));

        // Đánh dấu kết thúc
        httpd_resp_send_chunk(req, NULL, 0);
    } else {
        // Trả về phản hồi đăng nhập thất bại
        const char *resp = "{\"success\": false}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));

        // Đánh dấu kết thúc
        httpd_resp_send_chunk(req, NULL, 0);
    }

    return ESP_OK;
}


static const httpd_uri_t sign_data= {
    .uri       = "/login",
    .method    = HTTP_POST,
    .handler   = sign_post_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

/* An HTTP POST handler */
static esp_err_t data_post_handler(httpd_req_t *req)
{
    char buf[100];
        /* Send back the same data */
    httpd_req_recv(req, buf, req->content_len);
    printf("DATA: %s\n", buf);
    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t post_data= {
    .uri       = "/data",
    .method    = HTTP_POST,
    .handler   = data_post_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

static esp_err_t sw1_post_handler(httpd_req_t *req)
{
    char buf[100];
        /* Send back the same data */
    httpd_req_recv(req, buf, req->content_len);
    http_post_switch_callback(buf, req->content_len);
    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t sw1_post_data= {
    .uri       = "/switch1",
    .method    = HTTP_POST,
    .handler   = sw1_post_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

static esp_err_t wifi_post_handler(httpd_req_t *req)
{
    char buf[100];
        /* Send back the same data */
    httpd_req_recv(req, buf, req->content_len);
    printf("->data: %s\n", buf);
    http_post_wifi_info_callback(buf, req->content_len);
    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t wifi_post_data= {
    .uri       = "/wifiinfo",
    .method    = HTTP_POST,
    .handler   = wifi_post_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

static esp_err_t slider_post_handler(httpd_req_t *req)
{
    char buf[100];
        /* Send back the same data */
    httpd_req_recv(req, buf, req->content_len);
    http_post_slider_callback(buf, req->content_len);
    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t slider_post_data= {
    .uri       = "/slider",
    .method    = HTTP_POST,
    .handler   = slider_post_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

/* An HTTP GET handler */
// static esp_err_t hello_get_handler(httpd_req_t *req)
// {
//     httpd_resp_set_type(req, "text/html");
//     httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start); //send lại phản hồi khi client request vào dht11
//     return ESP_OK;
// }

// static const httpd_uri_t get_dht11= {
//     .uri       = "/trangchu",
//     .method    = HTTP_GET,
//     .handler   = hello_get_handler,
//     /* Let's pass response string in user
//      * context to demonstrate it's usage */
//     .user_ctx  = NULL
// };


static esp_err_t sign_in_get_handler(httpd_req_t *req)
{
    //const char* resp_str = (const char*) "Send response request";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index2_html_start, index2_html_end - index2_html_start); //send lại phản hồi khi client request vào dht11
    return ESP_OK;
}

static const httpd_uri_t get_sign_in= {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = sign_in_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

static esp_err_t setting_get_handler(httpd_req_t *req)
{
    //const char* resp_str = (const char*) "Send response request";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index3_html_start, index3_html_end - index3_html_start); //send lại phản hồi khi client request vào dht11
    return ESP_OK;
}

static const httpd_uri_t get_setting= {
    .uri       = "/caidat",
    .method    = HTTP_GET,
    .handler   = setting_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

esp_err_t image_get_handler(httpd_req_t *req) {
    // Gửi dữ liệu hình ảnh từ RAM
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_send(req, (const char *)anh_start, anh_end - anh_start);
    return ESP_OK;
}
static const httpd_uri_t get_image= {
    .uri       = "/anh",
    .method    = HTTP_GET,
    .handler   = image_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};
// Khởi tạo biến toàn cục cho temperature và humidity

// Hàm để tăng temperature và humidity mỗi giây
// Handler để gửi phản hồi DHT11
static float temperature = 27;
static int humidity = 20;
// Hàm để tăng temperature và humidity mỗi giây
void update_sensor_data() {
    temperature += 1;
    humidity += 1;
    if(temperature>=50)
    temperature=0;
    if(humidity>=45)
    humidity=0;
}

// Task để cập nhật giá trị mỗi giây
void sensor_task(void *pvParameters) {
    while (1) {
        update_sensor_data();
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay 1 giây
    }
}

static esp_err_t dht11_get_handler(httpd_req_t *req)
{
    //char resp_str[100];
    // Tạo chuỗi JSON với các giá trị cập nhật
    //snprintf(resp_str, sizeof(resp_str), "{\"temperature\": \"%.1f\", \"humidity\": \"%d\"}", temperature, humidity);      
    // Send the JSON response when the client requests the DHT11 data
    //httpd_resp_send(req, resp_str, strlen(resp_str));
    http_get_dht11_callback();
    return ESP_OK;
}

// Hàm này bạn có thể gọi mỗi giây để cập nhật dữ liệu

static const httpd_uri_t get_data_dht11= {
    .uri       = "/getdatadht11",
    .method    = HTTP_GET,
    .handler   = dht11_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

/* An HTTP POST handler */
// static esp_err_t echo_post_handler(httpd_req_t *req)
// {
//     char buf[100];
//     int ret, remaining = req->content_len;

//     while (remaining > 0) {
//         /* Read the data for the request */
//         if ((ret = httpd_req_recv(req, buf,
//                         MIN(remaining, sizeof(buf)))) <= 0) {
//             if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
//                 /* Retry receiving if timeout occurred */
//                 continue;
//             }
//             return ESP_FAIL;
//         }

//         /* Send back the same data */
//         httpd_resp_send_chunk(req, buf, ret);
//         remaining -= ret;

//         /* Log data received */
//         ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
//         ESP_LOGI(TAG, "%.*s", ret, buf);
//         ESP_LOGI(TAG, "====================================");
//     }

//     // End response
//     httpd_resp_send_chunk(req, NULL, 0);
//     return ESP_OK;
// }

// static const httpd_uri_t echo = {
//     .uri       = "/echo",
//     .method    = HTTP_POST,
//     .handler   = echo_post_handler,
//     .user_ctx  = NULL
// };

/* This handler allows the custom error handling functionality to be
 * tested from client side. For that, when a PUT request 0 is sent to
 * URI /ctrl, the /hello and /echo URIs are unregistered and following
 * custom error handler http_404_error_handler() is registered.
 * Afterwards, when /hello or /echo is requested, this custom error
 * handler is invoked which, after sending an error message to client,
 * either closes the underlying socket (when requested URI is /echo)
 * or keeps it open (when requested URI is /hello). This allows the
 * client to infer if the custom error handler is functioning as expected
 * by observing the socket state.
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/trang-chu", req->uri) == 0||strcmp("/", req->uri) == 0||strcmp("/caidat", req->uri) == 0) { //-> truy xuất vào struct vì hàm nhập vào khai báo *req là con trỏ, nếu không có * thì dùng . 
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } 
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

/* An HTTP PUT handler. This demonstrates realtime
 * registration and deregistration of URI handlers
 */


void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
#if CONFIG_IDF_TARGET_LINUX
    // Setting port as 8001 when building for Linux. Port 80 can be used only by a priviliged user in linux.
    // So when a unpriviliged user tries to run the application, it throws bind error and the server is not started.
    // Port 8001 can be used by an unpriviliged user as well. So the application will not throw bind error and the
    // server will be started.
    config.server_port = 8001;
#endif // !CONFIG_IDF_TARGET_LINUX
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &post_data);
        httpd_register_uri_handler(server, &sw1_post_data);
        // httpd_register_uri_handler(server, &get_dht11);
        httpd_register_uri_handler(server, &get_sign_in);
        httpd_register_uri_handler(server, &get_setting);
        httpd_register_uri_handler(server, &get_image);
        httpd_register_uri_handler(server, &wifi_post_data);
        httpd_register_uri_handler(server, &get_data_dht11);
        httpd_register_uri_handler(server, &sign_data);
        
        httpd_register_uri_handler(server, &slider_post_data);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);

    }
    else{
    ESP_LOGI(TAG, "Error starting server!");
    }
}


void stop_webserver(void)
{
    // Stop the httpd server
    httpd_stop(server);
}

void http_set_callback_switch(void *cb)
{
    http_post_switch_callback = cb;
}

void http_set_callback_slider(void *cb)
{
    http_post_slider_callback = cb;
}

void http_set_callback_dht11(void *cb)
{
    http_get_dht11_callback = cb;
}

void http_set_callback_wifiinfo(void *cb)
{
    http_post_wifi_info_callback = cb;
}



