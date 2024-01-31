#include <net/if.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#define NET_INTERFACE_COUNT 2
const char *network_interfaces[NET_INTERFACE_COUNT] = {"eth0", "wlan0"};

pthread_mutex_t get_hardware_id_lock = PTHREAD_MUTEX_INITIALIZER;
char *get_hardware_id() {
    static char *mac_address_hash = NULL;
    static bool no_mac_address = false;

    pthread_mutex_lock(&get_hardware_id_lock);
    if (!mac_address_hash && !no_mac_address) {
        int fd;
        struct ifreq ifr;
        unsigned char hash[SHA256_DIGEST_LENGTH];
        char mac_str[18];
        mac_address_hash = malloc(SHA256_DIGEST_LENGTH*2 + 1); // Space for SHA256 hash

        bool found = false;
        for (int i = 0; i < NET_INTERFACE_COUNT && !found; i++) {
            fd = socket(AF_INET, SOCK_DGRAM, 0);

            ifr.ifr_addr.sa_family = AF_INET;
            strncpy(ifr.ifr_name , network_interfaces[i] , IFNAMSIZ-1);

            if (ioctl(fd, SIOCGIFHWADDR, &ifr) >= 0) {
                unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;

                sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

                SHA256((unsigned char*)mac_str, strlen(mac_str), hash);

                for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
                    sprintf(mac_address_hash + (i*2), "%02x", hash[i]);

                found = true;
                printf("Using hardware id %s\n", mac_address_hash);
            }
            close(fd);
        }

        if (!found) {
            no_mac_address = true;
        }
    }
    pthread_mutex_unlock(&get_hardware_id_lock);

    return mac_address_hash;
}