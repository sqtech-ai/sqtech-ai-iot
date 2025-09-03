#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <string>
#include <vector>

#include "cJSON.h"

std::string makeConnectInfo(const char *appLicenseId, const char *regionCode, const char *sign,
                            const char *appTime, const char *deviceId, const char *serverToken);

std::string makeRequest(const char *deviceId, const char *request_id, const char *request_text,
                        const char *request_launchApp, const char *request_launchTime, const char *request_action,
                        const std::vector<std::string> &request_resultType, const std::vector<std::string> &request_agentIndexArray,
                        const cJSON *request_params);

int getResponse(const char *result, int &code, std::string &message, std::string &params);

uint64_t get_timestamp_ms();
void get_timestamp_ms_str(char *buffer, size_t buffer_size);
std::string get_device_id();
std::string get_chipmodel_name();
std::string get_connect_topic();
std::string get_publish_topic();
std::string get_sucribe_topic();
std::string get_disconnect_topic();

void bin2hex(const uint8_t *buf, size_t len, char *hex);
int hmac_sha256(const uint8_t *key, size_t key_len,
                const uint8_t *data, size_t data_len,
                uint8_t *digest);

#endif //__PROTOCOL_H__