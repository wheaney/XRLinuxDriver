#include "logging.h"
#include "strings.h"

#include <ifaddrs.h>
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

bool get_mac_address_hash(char **mac_address_hash, const char *interface) {
    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return false;

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);

    bool found = false;
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) >= 0) {
        unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;

        bool isZeroMac = true;
        for (int i = 0; i < 6; ++i) {
            if (mac[i] != 0) {
                isZeroMac = false;
                break;
            }
        }

        if (isZeroMac) return false;

        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256((unsigned char*)mac_str, strlen(mac_str), hash);

        *mac_address_hash = calloc(1, SHA256_DIGEST_LENGTH*2 + 1);
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
            sprintf(*mac_address_hash + (i*2), "%02x", hash[i]);

        found = true;
        log_message("Using hardware id %s\n", *mac_address_hash);
    }
    close(fd);

    return found;
}

#define RETRY_ATTEMPTS 6
#define RETRY_DELAY_SEC 5 // with 6 retries: 30 seconds of total attempts

pthread_mutex_t get_hardware_id_lock = PTHREAD_MUTEX_INITIALIZER;
char *get_hardware_id() {
    static char *mac_address_hash = NULL;
    static bool no_mac_address = false;

    pthread_mutex_lock(&get_hardware_id_lock);
    if (!mac_address_hash && !no_mac_address) {
        int attempts = 0;
        bool found = false;
        while (!found && attempts <= RETRY_ATTEMPTS) {
            // check these first so that hardwareIds don't change from the old logic here
            for (int i = 0; i < NET_INTERFACE_COUNT && !found; i++) {
                found = get_mac_address_hash(&mac_address_hash, network_interfaces[i]);
            }

            struct ifaddrs *ifaddr, *ifa;
            if (!found && getifaddrs(&ifaddr) != -1) {
                for (ifa = ifaddr; ifa != NULL && !found; ifa = ifa->ifa_next) {
                    if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET ||
                        in_array(ifa->ifa_name, network_interfaces, NET_INTERFACE_COUNT))
                        continue;

                    found = get_mac_address_hash(&mac_address_hash, ifa->ifa_name);
                }
                freeifaddrs(ifaddr);
            }

            if (!found && ++attempts <= RETRY_ATTEMPTS) {
                log_error("Failed to get hardwareId, retrying in %d seconds\n", RETRY_DELAY_SEC);
                sleep(RETRY_DELAY_SEC);
            }
        }

        no_mac_address = !found;
    }
    pthread_mutex_unlock(&get_hardware_id_lock);

    return mac_address_hash;
}