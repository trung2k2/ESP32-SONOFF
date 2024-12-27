#ifndef __APP_HTTP_SERVER_H
#define __APP_HTTP_SERVER_H


typedef void (*http_post_callback_t)(char* data, int len);
typedef void (*http_get_callback_t)(void);

void start_webserver(void);
void stop_webserver(void);
void sensor_task(void *pvParameters);
void http_set_callback_switch(void *cb);
void http_set_callback_dht11(void *cb);
void http_set_callback_slider(void *cb);
void http_set_callback_wifiinfo(void *cb);
#endif