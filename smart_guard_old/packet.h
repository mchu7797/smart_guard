/*
 * Created by Minseok Chu on 2024-06-06.
 */

#ifndef SMART_GUARD_PACKET_H
#define SMART_GUARD_PACKET_H

#include <stdbool.h>

struct PingRequest {
  int PingCommand;
  long ClientId;
  long ImageSize;
};

struct PingResponse {
  bool IsOk;
};

#endif // SMART_GUARD_PACKET_H
