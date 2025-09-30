#include "hook.h"

#include <stdio.h>
#include <stdarg.h>

#include "board.h"
#include "display/display.h"

#include "main/aiservice/type.h"
#include "main/aiservice/protocol.h"

int printf_hook(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  int ret = vprintf(fmt, args);
  va_end(args);
  return ret;
}

int display_hook(const char *role, const char *content)
{
  auto display = Board::GetInstance().GetDisplay();
  display->SetChatMessage(role, content);

  return 0;
}

int sendToService(WssMqttv5 *mqtt, const char *text)
{
  if (mqtt == nullptr || text == nullptr)
  {
    return -1;
  }

  std::string action = "playAudio"; //?
  static uint64_t request_id = get_timestamp_ms();
  ++request_id; // 确保每次请求都有唯一的 ID

  auto params = cJSON_CreateObject();
  if (!params)
  {
    return -1;
  }
  
  cJSON_AddNumberToObject(params, "pageSize", 10);
  cJSON_AddNumberToObject(params, "position", 1);

  bool listen_music = false;
  if (!listen_music)
  {
    cJSON_AddStringToObject(params, "intent", "listen_audiobook");
  }
  else
  {
    auto musicOption = cJSON_CreateObject();
    if (!musicOption)
    {
      cJSON_Delete(params);
      return -2;
    }

    cJSON_AddStringToObject(musicOption, "artistName", "黄绮珊");
    cJSON_AddStringToObject(musicOption, "songName", "向云端");
    cJSON_AddStringToObject(musicOption, "language", "国语");
    cJSON_AddStringToObject(musicOption, "albumName", "");
    cJSON_AddStringToObject(musicOption, "artistSex", "女");
    cJSON_AddBoolToObject(musicOption, "isOriginal", true);
    cJSON_AddStringToObject(musicOption, "area", "中国");
    cJSON_AddStringToObject(musicOption, "tag", "KTV");

    cJSON_AddItemToObject(params, "musicOption", musicOption);
  }
  cJSON_AddStringToObject(params, "content", text);
  cJSON_AddBoolToObject(params, "isRecommend", true);

  std::vector<std::string> request_resultType = {"extendParam"};
  if (std::string(SERVICE_PACKAGE_CODE) == "0")
  {
    request_resultType.emplace_back("cloudphoneInfo");
  }
  auto request = makeRequest(
      get_device_id().c_str(), std::to_string(request_id).c_str(), text, nullptr, nullptr, action.c_str(),
      request_resultType, // "cloudphoneInfo" 需要云机
      {}, params);
  cJSON_Delete(params); // 释放 params 对象
  if (request.empty())
  {
    return -1;
  }

  static std::string topic = get_publish_topic();
  if (!mqtt->Publish(topic, request))
  {
    display_hook("assistant", "发布消息失败");
    return -1;
  }
  display_hook("assistant", (std::string("已发送: ") + text).c_str());

  return 0;
}