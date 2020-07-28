#pragma once
#include <stdint.h>


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


typedef struct struct_message {
  int8_t    speed_0;
  int8_t    speed_1;
  int8_t    speed_2;
  int8_t    speed_3;
  uint32_t  color_left;
  uint32_t  color_right;
} struct_message;


typedef struct struct_response {
  response_status  status;
} struct_response;
