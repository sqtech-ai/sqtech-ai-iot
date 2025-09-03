#ifndef __HOOK_H__
#define __HOOK_H__

#include <iostream>

#include "wss_mqttv5.h"

#define TRACELINE()                     \
  do                                    \
  {                                     \
    std::cout << __LINE__ << std::endl; \
  } while (false)

int printf_hook(const char *fmt, ...);
int display_hook(const char *role, const char *content);

int sendToService(WssMqttv5 *mqtt, const char *text);

#endif //__HOOK_H__