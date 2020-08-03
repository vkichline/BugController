#pragma once
#include <stdint.h>

#define COMMUNICATIONS_SIGNATURE  0X42554743
#define COMMUNICATIONS_VERSION    0X0100
#define BROADCAST_MAC_ADDRESS     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

enum control_mode {
  MODE_FORWARD,
  MODE_STOP_AFTER_FORWARD,
  MODE_BACKWARD,
  MODE_STOP_AFTER_BACKWARD
};


enum response_status {
  RESP_NOERR,
  RESP_BUSY,
  RESP_ERROR
};


typedef struct discovery_message {
  ulong     signature = COMMUNICATIONS_SIGNATURE;
  uint16_t  version   = COMMUNICATIONS_VERSION;
} discovery_message;


typedef struct struct_message {
  ulong     signature = COMMUNICATIONS_SIGNATURE;
  uint16_t  version   = COMMUNICATIONS_VERSION;
  int8_t    speed_0;
  int8_t    speed_1;
  int8_t    speed_2;
  int8_t    speed_3;
  uint32_t  color_left;
  uint32_t  color_right;
  bool      button;
} struct_message;


typedef struct struct_response {
  ulong           signature = COMMUNICATIONS_SIGNATURE;
  uint16_t        version   = COMMUNICATIONS_VERSION;
  response_status status;
} struct_response;
