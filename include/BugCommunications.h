#pragma once
#include <stdint.h>

// Note that the motors of the BugC are arrainged like:
//   1      3
//   0      2
// ...where left is the front of the BugC


#define COMMUNICATIONS_SIGNATURE  0X42554743
#define COMMUNICATIONS_VERSION    0X0110
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


typedef struct struct_discovery {
  ulong     signature = COMMUNICATIONS_SIGNATURE;
  uint16_t  version   = COMMUNICATIONS_VERSION;
} struct_discovery;


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
