#include "curl.h"
#include "files.h"
#include "logging.h"
#include "plugins/device_license.h"
#include "runtime_context.h"
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

const char* DEVICE_LICENSE_FILE_NAME = "license.json";
const char* DEVICE_LICENSE_TEMP_FILE_NAME = "license.tmp.json";

#ifdef DEVICE_LICENSE_PUBLIC_KEY
    char* postbody(char* hardwareId, char** features, int features_count) {
        json_object *root = json_object_new_object();
        json_object_object_add(root, "hardwareId", json_object_new_string(hardwareId));
        json_object *featuresArray = json_object_new_array();
        for (int i = 0; i < features_count; i++) {
            json_object_array_add(featuresArray, json_object_new_string(features[i]));
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



pthread_mutex_t refresh_license_lock = PTHREAD_MUTEX_INITIALIZER;
void refresh_license(bool force) {
    int features_count = 0;
    char** features = NULL;

    #ifdef DEVICE_LICENSE_PUBLIC_KEY
        if (get_hardware_id()) {
            pthread_mutex_lock(&refresh_license_lock);
            const char* device_license_path = get_state_file_path(DEVICE_LICENSE_FILE_NAME);
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
                        char* postbody_string = postbody(get_hardware_id(), state()->registered_features, state()->registered_features_count);
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

                // TODO - pass the requested features list and verify all features are present in the license, otherwise refresh it
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

    if (state()->granted_features && state()->granted_features_count > 0) {
        for (int i = 0; i < state()->granted_features_count; i++) {
            free(state()->granted_features[i]);
        }
        free(state()->granted_features);
    }
    state()->granted_features = features;
    state()->granted_features_count = features_count;
}

void device_license_start_func() {
    refresh_license(false);
}

void device_license_handle_control_flag_line_func(char* key, char* value) {
    if (strcmp(key, "refresh_device_license") == 0) {
        if (strcmp(value, "true") == 0) refresh_license(true);
    }
}

void device_license_handle_device_connect_func() {
    refresh_license(false);
}

const plugin_type device_license_plugin = {
    .id = "device_license",
    .start = device_license_start_func,
    .handle_control_flag_line = device_license_handle_control_flag_line_func,
    .handle_device_connect = device_license_handle_device_connect_func
};