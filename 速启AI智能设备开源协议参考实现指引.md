# 速启AI智能设备开源协议参考实现

1.在程序初始化成功后，建立mqtt对象：

```cpp
wssMqtt = new WssMqttv5();
wssMqtt->Connect();
wssMqtt->Loop();
```

2.程序内识别到语音转文字的stt文本后，调用sendToService传输到速启后台进行服务请求：

```cpp
sendToService(wssMqtt, text->valuestring);
```

3.后台应答信息会在以下函数中回调：

```cpp
void WssMqttv5::OnPublish(const std::string &topic, const std::string &message)
```

4.编译添加源文件

```cpp
"../aiservice/main/aiservice/wss_mqttv5.cc"
"../aiservice/main/aiservice/hook.cc"
"../aiservice/main/aiservice/protocol.cc"
"../aiservice/main/mongoose/mongoose.c"
```

4.添加头文件路径

```cpp
set(INCLUDE_DIRS "../aiservice")
```

5.添加编译选项

```cpp
MG_ARCH=3 MG_TLS=MG_TLS_MBED
```

## 参数获取

aiservice/main/aiservice/type.h中使用到的以下宏参数，需要联系获取

```C++
APP_LICENSE_ID
APP_KEY
SERVER_TOKEN

REGION_CODE
SERVICE_PACKAGE_CODE

MQTT_USERNAME
MQTT_PASSWORD
```