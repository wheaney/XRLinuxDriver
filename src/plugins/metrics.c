#include "config.h"
#include "device.h"
#include "plugins.h"
#include "runtime_context.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>
#include <openssl/sha.h>

// Using mac address so it's always the same for the same hardware, but globally unique.
// Hashing it for privacy.
char *get_mac_address() {
    static char *mac_address = NULL;

    if (!mac_address) {
        int fd;
        struct ifreq ifr;
        char *iface = "eth0";
        unsigned char *mac;
        unsigned char hash[SHA256_DIGEST_LENGTH];
        char *mac_str = malloc(18 * sizeof(char));
        mac_address = malloc(SHA256_DIGEST_LENGTH*2 + 1); // Space for SHA256 hash

        fd = socket(AF_INET, SOCK_DGRAM, 0);

        ifr.ifr_addr.sa_family = AF_INET;
        strncpy(ifr.ifr_name , iface , IFNAMSIZ-1);

        ioctl(fd, SIOCGIFHWADDR, &ifr);

        close(fd);

        mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;

        sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        SHA256((unsigned char*)mac_str, strlen(mac_str), hash);

        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
            sprintf(mac_address + (i*2), "%02x", hash[i]);

        free(mac_str);
    }

    return mac_address;
}

const char *UA_MEASUREMENT_ID = "G-Z94MXP18T6";
const char *UA_CLIENT_ID="ARLinuxDriver";
void log_metric(char *event_name) {
    #ifdef UA_API_SECRET
        CURL *curl;
        CURLcode res;

        struct curl_slist *headers = NULL;

        curl_global_init(CURL_GLOBAL_DEFAULT);

        curl = curl_easy_init();
        if(curl) {
            headers = curl_slist_append(headers, "Content-Type: application/json");

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            char url[1024];
            snprintf(url, 1024, "https://www.google-analytics.com/mp/collect?api_secret=%s&measurement_id=%s", UA_API_SECRET, UA_MEASUREMENT_ID);
            curl_easy_setopt(curl, CURLOPT_URL, url);

            char post_data[1024];
            snprintf(post_data, 1024, "{\"client_id\": \"%s\", \"user_id\": \"%s\", \"events\": [{\"name\": \"%s\"}]}", UA_CLIENT_ID, get_mac_address(), event_name);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

            /* Perform the request, res will get the return code */
            res = curl_easy_perform(curl);

            /* always cleanup */
            curl_easy_cleanup(curl);
        }

        curl_global_cleanup();
    #endif
};

// Distill all possible modes into one enum. It's spread across multiple configs, so it's not a perfect representation.
enum metrics_output_mode {
    OUTPUT_MODE_DISABLED,
    OUTPUT_MODE_SIDEVIEW,
    OUTPUT_MODE_VIRTUAL_DISPLAY,
    OUTPUT_MODE_MOUSE,
    OUTPUT_MODE_JOYSTICK
};

char *metrics_output_mode_to_event_name[5] = {
    "output_mode_disabled",
    "output_mode_sideview",
    "output_mode_virtual_display",
    "output_mode_mouse",
    "output_mode_joystick"
};

enum metrics_output_mode *current_output_mode = NULL;
bool config_disabled = true;
char *config_output_mode = NULL;
char *config_external_mode = NULL;
char *current_device = NULL;
bool sbs_enabled = false;

void metrics_handle_config_line_func(void* config, char* key, char* value) {
    if (!config_output_mode) config_output_mode = strdup(mouse_output_mode);
    if (!config_external_mode) config_external_mode = strdup("none");
    if (equal(key, "output_mode")) {
        string_config(key, value, &config_output_mode);
    } else if (equal(key, "external_mode")) {
        string_config(key, value, &config_external_mode);
    } else if (equal(key, "disabled")) {
        boolean_config(key, value, &config_disabled);
    }
};

void metrics_set_config_func(void* config) {
    enum metrics_output_mode new_output_mode = OUTPUT_MODE_DISABLED;
    if (!config_disabled) {
        if (!config_output_mode || equal(config_output_mode, mouse_output_mode)) {
            new_output_mode = OUTPUT_MODE_MOUSE;
        } else if (equal(config_output_mode, joystick_output_mode)) {
            new_output_mode = OUTPUT_MODE_JOYSTICK;
        } else if (equal(config_external_mode, "virtual_display")) {
            new_output_mode = OUTPUT_MODE_VIRTUAL_DISPLAY;
        } else if (equal(config_external_mode, "sideview")) {
            new_output_mode = OUTPUT_MODE_SIDEVIEW;
        }
    }

    if (!current_output_mode || *current_output_mode != new_output_mode) {
        log_metric(metrics_output_mode_to_event_name[new_output_mode]);
        if (!current_output_mode) current_output_mode = malloc(sizeof(enum metrics_output_mode));
        *current_output_mode = new_output_mode;
    }
};


void metrics_handle_state_func() {
    if (!sbs_enabled && context.state->sbs_mode_enabled) {
        log_metric("sbs_enabled");
        sbs_enabled = context.state->sbs_mode_enabled;
    }
};

void metrics_handle_device_connect_func() {
    char new_device[1024];
    snprintf(new_device, 1024, "device_%x_%x", context.device->hid_vendor_id, context.device->hid_product_id);

    if (!current_device || strcmp(new_device, current_device) != 0) {
        log_metric(new_device);
        free_and_clear(&current_device);
        current_device = strdup(new_device);
    }
};

const plugin_type metrics_plugin = {
    .id = "metrics",
    .handle_config_line = metrics_handle_config_line_func,
    .set_config = metrics_set_config_func,
    .handle_state = metrics_handle_state_func,
    .handle_device_connect = metrics_handle_device_connect_func
};