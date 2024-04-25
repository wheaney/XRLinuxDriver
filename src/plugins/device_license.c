#include "curl.h"
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
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#define SECONDS_PER_DAY 86400

const char* DEVICE_LICENSE_FILE_PATH = "/var/lib/xr_driver/device_license";
const char* DEVICE_LICENSE_TEMP_FILE_PATH = "/var/lib/xr_driver/device_license.tmp";

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
            fprintf(stderr, "License or signature is NULL.\n");
            return false;
        }

        BIO *bio = BIO_new_mem_buf((void*)DEVICE_LICENSE_PUBLIC_KEY, -1);
        if (!bio) {
            fprintf(stderr, "Error creating BIO for public key.\n");
            ERR_print_errors_fp(stderr);
            return false;
        }

        EVP_PKEY *publicKey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
        BIO_free(bio);
        if (!publicKey) {
            fprintf(stderr, "Error reading the public key.\n");
            ERR_print_errors_fp(stderr);
            return false;
        }

        EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
        if (!mdctx) {
            fprintf(stderr, "Failed to create the EVP_MD_CTX structure.\n");
            ERR_print_errors_fp(stderr);
            return false;
        }

        if (1 != EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, publicKey)) {
            fprintf(stderr, "Failed to initialize the digest context for verification.\n");
            ERR_print_errors_fp(stderr);
            return false;
        }

        if (1 != EVP_DigestVerifyUpdate(mdctx, license, strlen((char*)license))) {
            fprintf(stderr, "Failed to update the digest context.\n");
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
            fprintf(stderr, "Signature is not valid.\n");
        } else if (result != 1) {
            fprintf(stderr, "Error occurred while checking the signature.\n");
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
            fprintf(stderr, "Error from server: %s\n", json_object_get_string(message));
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
                context.state->device_license = strdup(json_object_get_string(license));

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
                    printf("Feature %s %s.\n", featureName, enabled ? "granted" : "denied");
                }
            } else {
                if (context.config && context.config->debug_license) {
                    printf("\tdebug: License hardwareId mismatch;\n\t\treceived: %s\n\t\texpected: %s\n",
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
    free_and_clear(&context.state->device_license);

    #ifdef DEVICE_LICENSE_PUBLIC_KEY
        if (get_hardware_id()) {
            pthread_mutex_lock(&refresh_license_lock);
            struct stat st = {0};

            if (stat("/var/lib/xr_driver", &st) == -1) {
                mkdir("/var/lib/xr_driver", 0700);
            }

            int attempt = 0;
            bool valid_license = false;
            while (!valid_license && attempt < 2) {
                if (context.config && context.config->debug_license) printf("\tdebug: Attempt %d to refresh license\n", attempt);
                char* file_path = strdup(DEVICE_LICENSE_FILE_PATH);
                FILE *file = force ? NULL : fopen(file_path, "r");
                if (file) {
                    if (context.config && context.config->debug_license) printf("\tdebug: License file already exists\n");
                    // remove the file if it hasn't been touched in over a day
                    struct stat attr;
                    stat(DEVICE_LICENSE_FILE_PATH, &attr);
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    if (tv.tv_sec - attr.st_mtime > SECONDS_PER_DAY) {
                        // do this to force a refresh
                        fclose(file);
                        file = NULL;
                        if (context.config && context.config->debug_license) printf("\tdebug: License file is 1 day old, attempting to refresh it\n");
                    }
                }

                if (file == NULL) {
                    free(file_path);
                    file_path = strdup(DEVICE_LICENSE_TEMP_FILE_PATH);

                    CURL *curl;
                    CURLcode res;
                    struct curl_slist *headers = NULL;

                    curl_init();
                    curl = curl_easy_init();
                    if(curl) {
                        file = fopen(file_path, "w");
                        if (file == NULL) {
                            fprintf(stderr, "Error opening file\n");
                            break;
                        }

                        curl_easy_setopt(curl, CURLOPT_URL, "https://test.na.driver-backend.xronlinux.com/licenses/v1");
                        char* postbody_string = postbody(get_hardware_id(), context.state->registered_features, context.state->registered_features_count);
                        if (context.config && context.config->debug_license) printf("\tdebug: License curl with postbody %s\n", postbody_string);
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
                            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                        } else {
                            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                            if(http_code != 200) {
                                failed = true;
                                fprintf(stderr, "Unexpected HTTP response: %ld\n", http_code);
                                res = CURLE_HTTP_RETURNED_ERROR;
                            }
                        }

                        fclose(file);
                        if(failed) {
                            remove(file_path);
                            free(file_path);
                            file_path = strdup(DEVICE_LICENSE_FILE_PATH);
                        }

                        free(postbody_string);
                        curl_easy_cleanup(curl);
                    }

                    // Reopen the file
                    file = fopen(file_path, "r");
                    if (file == NULL) {
                        free(file_path);
                        fprintf(stderr, "Error opening file\n");
                        break;
                    }
                }

                features_count = get_license_features(file, &features);
                fclose(file);
                if (features_count != -1) {
                    valid_license = true;
                    if (strcmp(file_path, DEVICE_LICENSE_TEMP_FILE_PATH) == 0) {
                        rename(file_path, DEVICE_LICENSE_FILE_PATH);
                    }
                } else {
                    features_count = 0;
                    remove(file_path);
                }
                free(file_path);
                attempt++;
            }
            pthread_mutex_unlock(&refresh_license_lock);
        }
    #endif

    if (context.state->granted_features && context.state->granted_features_count > 0) {
        for (int i = 0; i < context.state->granted_features_count; i++) {
            free(context.state->granted_features[i]);
        }
        free(context.state->granted_features);
    }
    context.state->granted_features = features;
    context.state->granted_features_count = features_count;
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