#include "curl.h"
#include "files.h"
#include "logging.h"
#include "memory.h"
#include "plugins/device_license.h"
#include "runtime_context.h"
#include "strings.h"
#include "system.h"

#include <curl/curl.h>
#include <json-c/json.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#define SECONDS_PER_DAY 86400

const char* DEVICE_LICENSE_FILE_NAME = "%.8s_license.json";
const char* DEVICE_LICENSE_TEMP_FILE_NAME = "license.tmp.json";

// Declare mutexes early so they can be used in helper functions
pthread_mutex_t refresh_license_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t requested_features_lock = PTHREAD_MUTEX_INITIALIZER;

// Helper function to parse comma-separated features string
int parse_features_string(const char* features_str, char*** features) {
    if (!features_str || strlen(features_str) == 0) {
        *features = NULL;
        return 0;
    }

    char* str_copy = strdup(features_str);
    int count = 0;
    *features = NULL;

    char* token = strtok(str_copy, ",");
    while (token != NULL) {
        // Trim whitespace
        while (*token == ' ') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && *end == ' ') end--;
        *(end + 1) = '\0';

        if (strlen(token) > 0) {
            char** temp = realloc(*features, (count + 1) * sizeof(char*));
            if (!temp) {
                // Clean up on allocation failure
                for (int i = 0; i < count; i++) {
                    free((*features)[i]);
                }
                free(*features);
                free(str_copy);
                *features = NULL;
                return 0;
            }
            *features = temp;
            (*features)[count] = strdup(token);
            if (!(*features)[count]) {
                // Clean up on strdup failure
                for (int i = 0; i < count; i++) {
                    free((*features)[i]);
                }
                free(*features);
                free(str_copy);
                *features = NULL;
                return 0;
            }
            count++;
        }
        token = strtok(NULL, ",");
    }

    free(str_copy);
    return count;
}

// Helper function to check if all requested features are already granted
bool all_features_granted(char** requested_features, int requested_count) {
    if (requested_count == 0) return true;
    
    pthread_mutex_lock(&refresh_license_lock);
    bool result = true;
    
    if (!state()->granted_features || state()->granted_features_count == 0) {
        result = false;
    } else {
        for (int i = 0; i < requested_count; i++) {
            bool found = false;
            for (int j = 0; j < state()->granted_features_count; j++) {
                if (strcmp(requested_features[i], state()->granted_features[j]) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                result = false;
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&refresh_license_lock);
    return result;
}

// Helper function to deep copy feature array
char** deep_copy_features(char** features, int count) {
    if (count == 0 || !features) return NULL;
    
    char** copy = calloc(count, sizeof(char*));
    if (!copy) return NULL;
    
    for (int i = 0; i < count; i++) {
        if (!features[i]) {
            // Clean up on NULL feature
            for (int j = 0; j < i; j++) {
                free(copy[j]);
            }
            free(copy);
            return NULL;
        }
        copy[i] = strdup(features[i]);
        if (!copy[i]) {
            // Clean up on failure
            for (int j = 0; j < i; j++) {
                free(copy[j]);
            }
            free(copy);
            return NULL;
        }
    }
    return copy;
}

// Helper function to free feature array
void free_features(char** features, int count) {
    if (!features) return;
    for (int i = 0; i < count; i++) {
        free(features[i]);
    }
    free(features);
}

#ifdef DEVICE_LICENSE_PUBLIC_KEY
    char* postbody(char* hardwareId, char** features, int features_count) {
        json_object *root = json_object_new_object();
        json_object_object_add(root, "hardwareId", json_object_new_string(hardwareId));
        json_object *featuresArray = json_object_new_array();
        if (features && features_count > 0) {
            for (int i = 0; i < features_count; i++) {
                if (features[i]) {
                    json_object_array_add(featuresArray, json_object_new_string(features[i]));
                }
            }
        }
        json_object_object_add(root, "features", featuresArray);
        const char* json_str = json_object_to_json_string(root);
        char* result = strdup(json_str);
        json_object_put(root);
        return result;
    }

    bool is_valid_license_signature(const char* license, const char* signature) {
        if (!license || !signature) {
            log_error("License or signature is NULL.\n");
            return false;
        }

        BIO *bio = BIO_new_mem_buf((void*)DEVICE_LICENSE_PUBLIC_KEY, -1);
        if (!bio) {
            log_error("Error creating BIO for public key.\n");
            ERR_print_errors_fp(stderr);
            return false;
        }

        EVP_PKEY *publicKey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
        BIO_free(bio);
        if (!publicKey) {
            log_error("Error reading the public key.\n");
            ERR_print_errors_fp(stderr);
            return false;
        }

        EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
        if (!mdctx) {
            log_error("Failed to create the EVP_MD_CTX structure.\n");
            ERR_print_errors_fp(stderr);
            return false;
        }

        if (1 != EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, publicKey)) {
            log_error("Failed to initialize the digest context for verification.\n");
            ERR_print_errors_fp(stderr);
            return false;
        }

        if (1 != EVP_DigestVerifyUpdate(mdctx, license, strlen((char*)license))) {
            log_error("Failed to update the digest context.\n");
            ERR_print_errors_fp(stderr);
            return false;
        }

        // Convert the signature from hex string to binary
        int sig_len = strlen(signature) / 2;
        unsigned char* binary_sig = calloc(1, sig_len);
        for (int i = 0; i < sig_len; i++) {
            unsigned int temp;
            sscanf(signature + 2*i, "%02x", &temp);
            binary_sig[i] = temp;
        }

        int result = EVP_DigestVerifyFinal(mdctx, binary_sig, sig_len);
        free(binary_sig);
        binary_sig = NULL;

        if (result == 0) {
            log_error("Signature is not valid.\n");
        } else if (result != 1) {
            log_error("Error occurred while checking the signature.\n");
            ERR_print_errors_fp(stderr);
        }

        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(publicKey);

        return result == 1;
    }


    int get_license_features(FILE* file, char*** features) {
        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);
        char* buffer = calloc(1, length);
        if (buffer) {
            fread(buffer, 1, length, file);
        }

        json_object *root = json_tokener_parse(buffer);
        free(buffer);
        buffer = NULL;

        // check for the "message" property, indicating an error from the server
        json_object *message;
        json_object_object_get_ex(root, "message", &message);
        if (message) {
            log_error("Error from server: %s\n", json_object_get_string(message));
            json_object_put(root);
            return 0;
        }

        json_object *license;
        json_object_object_get_ex(root, "license", &license);
        json_object *signature;
        json_object_object_get_ex(root, "signature", &signature);

        int features_count = 0;
        bool valid_license = false;
        if (is_valid_license_signature(json_object_get_string(license), json_object_get_string(signature))) {
            json_object *license_root = json_tokener_parse(json_object_get_string(license));
            json_object *hardwareId;
            json_object_object_get_ex(license_root, "hardwareId", &hardwareId);
            if (strcmp(json_object_get_string(hardwareId), get_hardware_id()) == 0) {
                valid_license = true;
                free_and_clear(&state()->device_license);
                state()->device_license = strdup(json_object_get_string(license));

                json_object *features_root;
                json_object_object_get_ex(license_root, "features", &features_root);
                if (!features_root) {
                    json_object_put(license_root);
                    json_object_put(root);
                    return 0;
                }

                int license_features = json_object_object_length(features_root);
                if (license_features == 0) {
                    json_object_put(license_root);
                    json_object_put(root);
                    return 0;
                }

                json_object_object_foreach(features_root, featureName, val) {
                    json_object *featureObject;
                    json_object_object_get_ex(features_root, featureName, &featureObject);
                    json_object *featureStatusObject;
                    json_object_object_get_ex(featureObject, "status", &featureStatusObject);
                    const char* featureStatus = json_object_get_string(featureStatusObject);
                    json_object *featureEndDate;
                    json_object_object_get_ex(featureObject, "endDate", &featureEndDate);

                    // if status is "on" or "trial" and the current system time is not past the endDate, add to the features array
                    bool enabled = false;
                    if (strcmp(featureStatus, "on") == 0 || strcmp(featureStatus, "trial") == 0) {
                        struct timeval tv;
                        gettimeofday(&tv, NULL);
                        if (!featureEndDate || json_object_get_int(featureEndDate) > tv.tv_sec) {
                            *features = realloc(*features, (features_count + 1) * sizeof(char*));
                            (*features)[features_count++] = strdup(featureName);
                            enabled = true;
                        }
                    }
                    log_message("Feature %s %s.\n", featureName, enabled ? "granted" : "denied");
                }
            } else {
                if (config() && config()->debug_license) {
                    log_debug("License hardwareId mismatch;\n\t\treceived: %s\n\t\texpected: %s\n",
                        json_object_get_string(hardwareId), get_hardware_id());
                }
            }
            json_object_put(license_root);
        }
        json_object_put(root);

        if (!valid_license) {
            return -1;
        }

        return features_count;
    }
#endif



void refresh_license(bool force, char** requested_features, int requested_features_count) {
    int features_count = 0;
    char** features = NULL;

    #ifdef DEVICE_LICENSE_PUBLIC_KEY
        char* hw_id = get_hardware_id();
        if (hw_id) {
            pthread_mutex_lock(&refresh_license_lock);

            // prefix the license file name with the hardware id we can still have this one saved in 
            // the case where it changes in the future, which can happen due to network interface ordering or
            // startup sequence timing
            char* file_name = malloc(strlen(hw_id) + strlen(DEVICE_LICENSE_FILE_NAME));
            sprintf(file_name, DEVICE_LICENSE_FILE_NAME, hw_id);
            const char* device_license_path = get_state_file_path(file_name);
            free(file_name);
            const char* device_license_path_tmp = get_state_file_path(DEVICE_LICENSE_TEMP_FILE_NAME);

            int attempt = 0;
            bool valid_license = false;
            bool debug_license = config() && config()->debug_license;
            while (!valid_license && attempt < 2) {
                if (debug_license) log_debug("Attempt %d to refresh license\n", attempt);
                char* file_path = strdup(device_license_path);
                FILE *file = force ? NULL : fopen(file_path, "r");
                if (file) {
                    if (debug_license) log_debug("License file already exists\n");
                    // remove the file if it hasn't been touched in over a day
                    struct stat attr;
                    stat(device_license_path, &attr);
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    if (tv.tv_sec - attr.st_mtime > SECONDS_PER_DAY) {
                        // do this to force a refresh
                        fclose(file);
                        file = NULL;
                        if (debug_license) log_debug("License file is 1 day old, attempting to refresh it\n");
                    }
                }

                if (file == NULL) {
                    free(file_path);
                    file_path = strdup(device_license_path_tmp);

                    CURL *curl;
                    CURLcode res;
                    struct curl_slist *headers = NULL;

                    curl_init();
                    curl = curl_easy_init();
                    if(curl) {
                        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
                        file = get_or_create_file(file_path, 0777, "w", NULL);
                        if (file == NULL) {
                            log_error("Error opening file (%s): %d\n", file_path, errno);
                            break;
                        }

                        curl_easy_setopt(curl, CURLOPT_URL, "https://eu.driver-backend.xronlinux.com/licenses/v1");
                        char* postbody_string = postbody(get_hardware_id(), requested_features, requested_features_count);
                        if (debug_license) log_debug("License curl with postbody %s\n", postbody_string);
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postbody_string);

                        headers = curl_slist_append(headers, "Content-Type: application/json");
                        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

                        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
                        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

                        long http_code = 0;
                        CURLcode res = curl_easy_perform(curl);

                        bool failed = false;
                        if(res != CURLE_OK) {
                            failed = true;
                            log_error("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                        } else {
                            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                            if(http_code != 200) {
                                failed = true;
                                log_error("Unexpected HTTP response: %ld\n", http_code);
                                res = CURLE_HTTP_RETURNED_ERROR;
                            }
                        }

                        fclose(file);
                        if(failed) {
                            remove(file_path);
                            free(file_path);
                            file_path = strdup(device_license_path);
                        }

                        free(postbody_string);
                        curl_easy_cleanup(curl);
                    }

                    // Reopen the file
                    file = fopen(file_path, "r");
                    if (file == NULL) {
                        free(file_path);
                        log_error("Error opening file (%s): %d\n", file_path, errno);
                        break;
                    }
                }

                features_count = get_license_features(file, &features);
                fclose(file);
                if (features_count != -1) {
                    valid_license = true;
                    if (strcmp(file_path, device_license_path_tmp) == 0) {
                        rename(file_path, device_license_path);
                    }
                } else {
                    features_count = 0;
                    remove(file_path);
                }
                free(file_path);
                attempt++;
            }
            pthread_mutex_unlock(&refresh_license_lock);

            free((void*)device_license_path);
            free((void*)device_license_path_tmp);
        } else {
            log_error("No hardwareId found, not retrieving license\n");
        }
    #endif

    pthread_mutex_lock(&refresh_license_lock);
    free_features(state()->granted_features, state()->granted_features_count);
    state()->granted_features = features;
    state()->granted_features_count = features_count;
    pthread_mutex_unlock(&refresh_license_lock);
}

void device_license_start_func() {
    refresh_license(false, NULL, 0);
}

static char** last_requested_features = NULL;
static int last_requested_features_count = 0;

void device_license_handle_control_flag_line_func(char* key, char* value) {
    if (strcmp(key, "refresh_device_license") == 0) {
        if (strcmp(value, "true") == 0) refresh_license(true, NULL, 0);
    } else if (strcmp(key, "request_features") == 0) {
        pthread_mutex_lock(&requested_features_lock);
        
        // Parse the comma-separated features string
        char** requested_features = NULL;
        int requested_count = parse_features_string(value, &requested_features);

        // Check if these features are different from the last request
        bool features_changed = false;
        if (requested_count != last_requested_features_count) {
            features_changed = true;
        } else {
            for (int i = 0; i < requested_count; i++) {
                bool found = false;
                for (int j = 0; j < last_requested_features_count; j++) {
                    if (strcmp(requested_features[i], last_requested_features[j]) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    features_changed = true;
                    break;
                }
            }
        }

        // If features changed and not all requested features are already granted, refresh the license
        bool should_refresh = features_changed && !all_features_granted(requested_features, requested_count);
        
        // Make a deep copy for refresh_license if needed
        char** features_copy = NULL;
        int features_copy_count = 0;
        if (should_refresh) {
            features_copy = deep_copy_features(requested_features, requested_count);
            if (features_copy) {
                features_copy_count = requested_count;
            } else {
                // Failed to allocate memory for deep copy, don't update last_requested_features
                // to avoid inconsistent state
                pthread_mutex_unlock(&requested_features_lock);
                free_features(requested_features, requested_count);
                return;
            }
        }
        
        // Update last requested features
        free_features(last_requested_features, last_requested_features_count);
        last_requested_features = requested_features;
        last_requested_features_count = requested_count;
        
        pthread_mutex_unlock(&requested_features_lock);
        
        if (should_refresh) {
            refresh_license(false, features_copy, features_copy_count);
            free_features(features_copy, features_copy_count);
        }
    }
}

void device_license_handle_device_connect_func() {
    refresh_license(false, NULL, 0);
}

const plugin_type device_license_plugin = {
    .id = "device_license",
    .start = device_license_start_func,
    .handle_control_flag_line = device_license_handle_control_flag_line_func,
    .handle_device_connect = device_license_handle_device_connect_func
};