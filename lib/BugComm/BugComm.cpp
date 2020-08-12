#include <Arduino.h>
#include "BugComm.h"


void BugComm::send_command(int8_t x, int8_t y, bool button) {
  // Joystick values are +/- 128, we need to scale these to +/- 100
  x =  (int8_t)(((int16_t)(x)*100)/128);
  y = -(int8_t)(((int16_t)(y)*100)/128);  // Invert Y so steering is natural
  // dampen down the small numbers
  if (abs(x) < 2) x = 0;
  if (abs(y) < 2) y = 0;
  float delta = y / 100.0;
  if(0 > delta) delta = 1.0 + delta;
  else if(0 < delta) delta = -(1.0 - delta);
  if(last_x != x || last_y != y || last_b != button) {
    last_x = x;
    last_y = y;
    last_b = button;
    uint32_t color = 0x0000;          // Black
    if     (x > 0) color = 0x001000;  // Moving forward, set color to green
    else if(x < 0) color = 0x100000;  // Moving backward, set color to red

    // If Y is < 0, trim the speed of motors 1 & 3
    // If Y is > 0. trim the speed of motors 0 & 2
    uint8_t scaled_value = (int)((float)x * delta);
    command.speed_0      = 0 >= delta ?  x : scaled_value;
    command.speed_1      = 0 <= delta ? -x : scaled_value;
    command.speed_2      = 0 >= delta ?  x : scaled_value;
    command.speed_3      = 0 <= delta ? -x : scaled_value;
    command.color_left   = color;
    command.color_right  = color;
    command.button       = button;
    NowComm<BugCommand>::send_command(&command);
  }
}


uint32_t BugComm::get_light_color(uint8_t pos) {
  if(pos > 2) return 0;
  return (pos == 0) ? command.color_left : command.color_right;
}


uint8_t  BugComm::get_motor_speed(uint8_t pos) {
  switch(pos) {
    case 0:   return command.speed_0;
    case 1:   return command.speed_1;
    case 2:   return command.speed_2;
    case 3:   return command.speed_3;
    default:  return 0;
  }
}


uint8_t  BugComm::get_button() {
  return command.button;
}
