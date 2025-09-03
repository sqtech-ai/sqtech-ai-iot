#include "protocol.h"

#include <esp_log.h>
#include <mbedtls/md.h>
#include "../main/system_info.h"

#include "type.h"
#include "hook.h"

static const char *TAG = "Protocol";

std::string makeConnectInfo(const char *appLicenseId, const char *regionCode, const char *sign,
                            const char *appTime, const char *deviceId, const char *serverToken)
{
  // 参数校验
  if (appLicenseId == nullptr || regionCode == nullptr || 
      sign == nullptr || deviceId == nullptr || serverToken == nullptr)
  {
    ESP_LOGE(TAG, "Invalid arguments");
    return "";
  }

  cJSON *root = cJSON_CreateObject();
  if (!root)
  {
    ESP_LOGE(TAG, "Failed to create JSON object");
    return "";
  }

  cJSON_AddStringToObject(root, "appLicenseId", appLicenseId);
  cJSON_AddStringToObject(root, "regionCode", regionCode);
  cJSON_AddStringToObject(root, "sign", sign);
  cJSON_AddStringToObject(root, "appTime", appTime);
  cJSON_AddStringToObject(root, "deviceId", deviceId);
  cJSON_AddStringToObject(root, "serverToken", serverToken);
  cJSON_AddStringToObject(root, "deviceType", get_chipmodel_name().c_str());
  cJSON_AddStringToObject(root, "servicePackageCode", SERVICE_PACKAGE_CODE);

  char *json_string = cJSON_PrintUnformatted(root);
  std::string result(json_string);
  cJSON_Delete(root);
  free(json_string);

  return result;
}

std::string makeRequest(const char *deviceId, const char *request_id, const char *request_text,
                        const char *request_launchApp, const char *request_launchTime, const char *request_action,
                        const std::vector<std::string> &request_resultType, const std::vector<std::string> &request_agentIndexArray,
                        const cJSON *request_params)
{
  // 参数校验
  if (deviceId == nullptr || request_id == nullptr ||
      request_text == nullptr || request_resultType.empty())
  {
    ESP_LOGE(TAG, "Invalid arguments");
    return "";
  }

  // 创建根对象
  auto root = cJSON_CreateObject();
  if (!root)
  {
    ESP_LOGE(TAG, "Failed to create root JSON");
    return "";
  }
  cJSON_AddStringToObject(root, "deviceId", deviceId);

  // 创建 request 对象
  auto request = cJSON_CreateObject();
  if (!request)
  {
    cJSON_Delete(root);
    ESP_LOGE(TAG, "Failed to create request JSON");
    return "";
  }
  cJSON_AddStringToObject(request, "id", request_id);
  cJSON_AddStringToObject(request, "text", request_text);

  // 可选字段
  if (request_launchApp != nullptr)
  {
    cJSON_AddStringToObject(request, "launchApp", request_launchApp);
  }
  if (request_launchTime != nullptr)
  {
    cJSON_AddStringToObject(request, "launchTime", request_launchTime);
  }
  if (request_action != nullptr)
  {
    cJSON_AddStringToObject(request, "action", request_action);
  }

  // 处理数组字段
  auto resultType = cJSON_CreateArray();
  if (!resultType)
  {
    cJSON_Delete(root);
    cJSON_Delete(request);
    ESP_LOGE(TAG, "Failed to create resultType array");
    return "";
  }
  for (const auto &type : request_resultType)
  {
    cJSON_AddItemToArray(resultType, cJSON_CreateString(type.c_str()));
  }
  cJSON_AddItemToObject(request, "resultType", resultType); // request 接管所有权

  // 处理可选数组字段
  if (!request_agentIndexArray.empty())
  {
    auto agentIndexArray = cJSON_CreateArray();
    if (agentIndexArray)
    {
      for (const auto &index : request_agentIndexArray)
      {
        cJSON_AddItemToArray(agentIndexArray, cJSON_CreateString(index.c_str()));
      }
      cJSON_AddItemToObject(request, "agentIndexArray", agentIndexArray);
    }
  }

  // 处理嵌套的 request_params
  if (request_params != nullptr)
  {
    cJSON_AddItemToObject(request, "params", cJSON_Duplicate(request_params, 1));
  }

  // 组装最终 JSON
  cJSON_AddItemToObject(root, "request", request);
  char *json_str = cJSON_PrintUnformatted(root);
  if (!json_str)
  {
    cJSON_Delete(root);
    ESP_LOGE(TAG, "Failed to print JSON");
    return "";
  }

  std::string result(json_str);
  cJSON_free(json_str);
  cJSON_Delete(root); // 会递归释放所有子节点（包括 request）
  return result;
}

int getResponse(const char *result, int &code, std::string &message, std::string &params)
{
  // 参数校验
  if (result == nullptr)
  {
    ESP_LOGE(TAG, "Invalid arguments: result is null");
    return -1;
  }

  // 解析 JSON
  cJSON *root = cJSON_Parse(result);
  if (root == nullptr)
  {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != nullptr)
    {
      ESP_LOGE(TAG, "JSON parse error before: %s", error_ptr);
    }
    return -1;
  }

  // 提取 code
  cJSON *code_item = cJSON_GetObjectItem(root, "code");
  if (code_item != nullptr && cJSON_IsNumber(code_item))
  {
    code = code_item->valueint;
  }
  else
  {
    ESP_LOGE(TAG, "Failed to parse code");
    cJSON_Delete(root);
    return -1;
  }

  // 提取 message
  cJSON *message_item = cJSON_GetObjectItem(root, "message");
  if (message_item != nullptr && cJSON_IsString(message_item))
  {
    message = message_item->valuestring;
  }
  else
  {
    ESP_LOGE(TAG, "Failed to parse message");
    cJSON_Delete(root);
    return -1;
  }

  // 提取 result.extendParam
  cJSON *data = cJSON_GetObjectItem(root, "result");
  if (data != nullptr)
  {
    cJSON *param_item = cJSON_GetObjectItem(data, "extendParam");
    if (param_item != nullptr)
    {
      char *json_str = cJSON_PrintUnformatted(param_item);
      if (json_str != nullptr)
      {
        params = json_str;
        cJSON_free(json_str);
      }
    }
    else
    {
      ESP_LOGE(TAG, "Failed to parse extendParam");
      cJSON_Delete(root);
      return 0;
    }
  }
  else
  {
    ESP_LOGE(TAG, "Failed to parse result");
    cJSON_Delete(root);
    return 0;
  }

  // 释放 cJSON 对象
  cJSON_Delete(root);
  return 0; // 成功返回 0
}

uint64_t get_timestamp_ms()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void get_timestamp_ms_str(char *buffer, size_t buffer_size)
{
  uint64_t milliseconds = get_timestamp_ms();

  std::string str = std::to_string(milliseconds);
  strncpy(buffer, str.c_str(), buffer_size - 1);
  buffer[buffer_size - 1] = '\0'; // 确保终止符
}

std::string get_device_id()
{
  return SystemInfo::GetMacAddress();
}

std::string get_chipmodel_name()
{
  return SystemInfo::GetChipModelName();
}

std::string get_connect_topic()
{
  std::string topic = "connect/online";
  // std::cout << "topic: " << topic << std::endl;
  return topic;
}

std::string get_publish_topic()
{
  std::string topic = "request/";
  topic += APP_LICENSE_ID;
  topic += "/";
  topic += get_device_id();
  // std::cout << "topic: " << topic << std::endl;
  return topic;
}

std::string get_sucribe_topic()
{
  std::string topic = "response/";
  topic += APP_LICENSE_ID;
  topic += "/";
  topic += get_device_id();
  // std::cout << "topic: " << topic << std::endl;
  return topic;
}

std::string get_disconnect_topic()
{
  std::string topic = "disconnect/";
  topic += get_device_id();
  // std::cout << "topic: " << topic << std::endl;
  return topic;
}

// 将二进制数据转为十六进制字符串
void bin2hex(const uint8_t *buf, size_t len, char *hex)
{
  for (size_t i = 0; i < len; i++)
  {
    sprintf(hex + 2 * i, "%02x", buf[i]);
  }
  hex[2 * len] = '\0'; // 确保字符串以 null 结尾
}

int hmac_sha256(const uint8_t *key, size_t key_len,
                const uint8_t *data, size_t data_len,
                uint8_t *digest)
{
  mbedtls_md_context_t ctx;
  const mbedtls_md_info_t *md_info;

  // 初始化上下文
  mbedtls_md_init(&ctx);

  // 获取 SHA-256 算法信息
  md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md_info == NULL)
  {
    return -1; // 不支持的算法
  }

  // 设置 HMAC 上下文
  if (mbedtls_md_setup(&ctx, md_info, 1) != 0)
  { // 1 表示启用 HMAC
    mbedtls_md_free(&ctx);
    return -2;
  }

  // 计算 HMAC
  if (mbedtls_md_hmac_starts(&ctx, key, key_len) != 0)
  {
    mbedtls_md_free(&ctx);
    return -3;
  }
  if (mbedtls_md_hmac_update(&ctx, data, data_len) != 0)
  {
    mbedtls_md_free(&ctx);
    return -4;
  }
  if (mbedtls_md_hmac_finish(&ctx, digest) != 0)
  {
    mbedtls_md_free(&ctx);
    return -5;
  }

  // 释放资源
  mbedtls_md_free(&ctx);
  return 0;
}