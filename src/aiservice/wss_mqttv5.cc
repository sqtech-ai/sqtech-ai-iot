#include "wss_mqttv5.h"

#include <iostream>
#include <thread>

#include "main/aiservice/type.h"
#include "main/aiservice/protocol.h"
#include "main/aiservice/hook.h"

#include "esp_log.h"

#define TAG "WssMqttv5"

static void fn(struct mg_connection *c, int ev, void *ev_data)
{
  WssMqttv5 *mq5 = ((WssMqttv5 *)c->fn_data);

  if (ev == MG_EV_ERROR)
  {
    mq5->onError(ev, (const char *)ev_data);
  }
  else if (ev == MG_EV_CONNECT)
  {
    if (c->is_tls)
    {
      ESP_LOGI(TAG, "is_tls...");
      struct mg_tls_opts opts = {0};
      mg_tls_init(c, &opts);
    }
  }
  else if (ev == MG_EV_WS_OPEN)
  {
    // WS connection established. Perform MQTT login
    ESP_LOGI(TAG, "Connected to WS. Logging in to MQTT...");
    // 设置MQTT 5.0连接属性
    std::string deviceid = get_device_id();
    struct mg_mqtt_opts opts = {0};
    opts.user = mg_str(MQTT_USERNAME);
    opts.pass = mg_str(MQTT_PASSWORD);
    opts.client_id = mg_str(deviceid.c_str());
    opts.keepalive = 0;
    opts.clean = true;
    opts.version = 5;   // 使用MQTT 5.0协议
    opts.num_props = 0;
    size_t len = c->send.len;
    mg_mqtt_login(c, &opts);
    mg_ws_wrap(c, c->send.len - len, WEBSOCKET_OP_BINARY);
  }
  else if (ev == MG_EV_WS_MSG)
  {
    struct mg_mqtt_message mm;
    struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
    uint8_t version = c->is_mqtt5 ? 5 : 4;
    ESP_LOGI(TAG, "GOT %d bytes WS msg", (int)wm->data.len);
    while ((mg_mqtt_parse((uint8_t *)wm->data.buf, wm->data.len, version,
                          &mm)) == MG_MQTT_OK)
    {
      switch (mm.cmd)
      {
      case MQTT_CMD_CONNACK:
      {
        mg_call(c, MG_EV_MQTT_OPEN, &mm.ack);
        if (mm.ack != 0)
        {
          ESP_LOGE(TAG, "%lu MQTT auth failed, code %d", c->id, mm.ack);
          c->is_draining = 1;
        }

        mq5->Subscribe(get_sucribe_topic(), 1);

        char timestamp[20] = {0};
        get_timestamp_ms_str(timestamp, sizeof(timestamp));
        uint8_t digest[32];  // SHA256 输出为 32 字节
        char hex_digest[65]; // 十六进制字符串（64字符 + '\0'）
        std::string data = std::string(timestamp) + APP_LICENSE_ID + get_device_id() + SERVICE_PACKAGE_CODE + APP_KEY;
        std::cout << "Data for HMAC-SHA256: " << data << std::endl;
        // 计算 HMAC-SHA256
        int ret = hmac_sha256(
            (const uint8_t *)APP_KEY, strlen(APP_KEY),
            (const uint8_t *)data.c_str(), strlen(data.c_str()),
            digest);
        if (ret == 0)
        {
          bin2hex(digest, sizeof(digest), hex_digest);
          printf("HMAC-SHA256: %s\n", hex_digest);
        }
        else
        {
          printf("Error: %d\n", ret);
          return;
        }
        // 连接mqtt后 发布connect消息
        mq5->Publish(get_connect_topic(), makeConnectInfo(APP_LICENSE_ID, REGION_CODE, hex_digest, timestamp, get_device_id().c_str(), SERVER_TOKEN), 1);
      }
      break;
      case MQTT_CMD_PUBLISH:
      {
        mq5->OnPublish(mm.topic.buf, mm.data.buf);
        c->is_draining = 0;
        break;
      }
      }
      wm->data.buf += mm.dgram.len;
      wm->data.len -= mm.dgram.len;
    }
  }
  if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE)
  {
    ESP_LOGE(TAG, "Connection closed or error occurred.");
    ((WssMqttv5 *)c->fn_data)->Done() = true;
  }
}

bool WssMqttv5::Connect()
{
  mg_mgr_init(&mgr);
  mg_log_set(MG_LL_ERROR);
  client_ = mg_ws_connect(&mgr, MQTT_ADDRESS, fn, this, "%s",
                          "Sec-Websocket-Protocol: mqtt\r\n");

  return client_ != nullptr;
}

bool WssMqttv5::Subscribe(const std::string &topic, int qos)
{
  if (client_ == nullptr)
  {
    return false;
  }

  auto len = client_->send.len;
  struct mg_mqtt_opts sub_opts;
  memset(&sub_opts, 0, sizeof(sub_opts));
  sub_opts.topic = mg_str(topic.c_str());
  sub_opts.qos = qos;
  mg_mqtt_sub(client_, &sub_opts);
  len = mg_ws_wrap(client_, client_->send.len - len, WEBSOCKET_OP_BINARY);

  return true;
}

bool WssMqttv5::Publish(const std::string &topic, const std::string &message, int qos)
{
  if (client_ == nullptr)
  {
    return false;
  }

  auto len = client_->send.len;
  struct mg_mqtt_opts pub_opts;
  memset(&pub_opts, 0, sizeof(pub_opts));
  pub_opts.topic = mg_str(topic.c_str());
  pub_opts.message = mg_str(message.c_str());
  pub_opts.qos = qos;
  pub_opts.retain = false;
  std::cout << "Publishing to topic: " << topic << ", message: " << message << std::endl;
  mg_mqtt_pub(client_, &pub_opts);
  len = mg_ws_wrap(client_, client_->send.len - len, WEBSOCKET_OP_BINARY);

  return true;
}

void WssMqttv5::Loop()
{
  while (!done)
  {
    mg_mgr_poll(&mgr, 1000);

    while (done)
    {
      std::this_thread::sleep_for(std::chrono::seconds(5));
      ESP_LOGI(TAG, "Reconnecting...");
      done = !Connect();
    }
  }
}

void WssMqttv5::onError(int ev, const char *msg)
{
}

void WssMqttv5::onConnect()
{
}

void WssMqttv5::OnPublish(const std::string &topic, const std::string &message)
{
  ESP_LOGI(TAG, "topic = %s, message = %s", topic.c_str(), message.c_str());
  if (topic != get_sucribe_topic())
  {
    return;
  }

  int code = -1;
  std::string response_message;
  std::string params;
  if (getResponse(message.c_str(), code, response_message, params) == 0)
  {
    if (params.size() > 0)
    {
      display_hook("assistant", ("响应信息: " + params).c_str());
      return;
    }

    if (code != 1000)
    {
      display_hook("assistant", ("code: " + std::to_string(code) + ", " + response_message).c_str());
    }
  }
  else
  {
    display_hook("assistant", "解析响应失败");
  }
}
