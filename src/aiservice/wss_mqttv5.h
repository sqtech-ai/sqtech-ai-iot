#ifndef __WSS_MQTTV5_H__
#define __WSS_MQTTV5_H__

#include <string>

#include "../mongoose/mongoose.h"

class WssMqttv5
{
public:
  bool Connect();
  bool Subscribe(const std::string &topic, int qos = 0);
  bool Publish(const std::string &topic, const std::string &message, int qos = 0);
  void Loop();

  // callback
  void onError(int ev, const char *msg);
  void onConnect();
  void OnPublish(const std::string &topic, const std::string &message);
  bool &Done() { return done; }

private:
  struct mg_mgr mgr;
  struct mg_connection *client_ = nullptr;
  bool done = false;
};

#endif //__WSS_MQTTV5_H__